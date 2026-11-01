/* Keep every function reachable from emitted AST or legacy runtime callbacks. */
#include "optimizer.h"
#include <string.h>

static void mark(Function *fn)
{
    if (fn && !fn->is_template && !fn->opt_reachable) fn->opt_reachable = 1;
}
static void visit(Ast *ast, Function *owner, Node *n)
{
    InputField *f;
    for (; n; n = n->next) {
        if (n->opt_inline && n->opt_inline->owner == owner) visit(ast,owner,n->opt_inline->body);
        else if (n->kind == ND_CALL || n->kind == ND_CONSTRUCT) {
            if (n->func) mark(n->func);
            else { Function *fn; for(fn=ast->funcs;fn;fn=fn->next)mark(fn); }
        }
        if (n->kind == ND_MODEL_NEW) mark(type_model(n->ty)->default_builder);
        if (n->kind == ND_VAR && n->var && n->var->is_const) visit(ast,owner,n->var->const_val);
        visit(ast,owner,n->lhs); visit(ast,owner,n->rhs); visit(ast,owner,n->cond);
        if (n->kind == ND_IF && n->cond->kind == ND_BOOL) visit(ast,owner,n->cond->val?n->then:n->els);
        else {
            if (!(n->kind == ND_WHILE && n->cond->kind == ND_BOOL && !n->cond->val)) visit(ast,owner,n->then);
            visit(ast,owner,n->els);
        }
        visit(ast,owner,n->init); visit(ast,owner,n->step); visit(ast,owner,n->inc); visit(ast,owner,n->for_amount); visit(ast,owner,n->end);
        for(f=n->fields;f;f=f->next)visit(ast,owner,f->var);
        if (n->kind == ND_RETURN || n->kind == ND_THROW || n->kind == ND_BREAK || n->kind == ND_CONTINUE) break;
    }
}
void optimizer_reachable(Ast *ast)
{
    Function *fn;
    Model *m;
    int changed;
    for(fn=ast->funcs;fn;fn=fn->next)fn->opt_reachable=0;
    for(fn=ast->funcs;fn;fn=fn->next)
        if(!fn->model&&fn->tok->source->module==ast->entry&&fn->name_len==4&&!memcmp(fn->name,"main",4))mark(fn);
    /* The generic array-copy helper still dispatches over every model type. */
    for(m=ast->models;m;m=m->next)mark(m->default_builder);
    visit(ast,NULL,ast->globals);
    do {
        changed=0;
        for(fn=ast->funcs;fn;fn=fn->next)if(fn->opt_reachable==1){
            fn->opt_reachable=2;visit(ast,fn,fn->body);changed=1;
        }
    } while(changed);
}
