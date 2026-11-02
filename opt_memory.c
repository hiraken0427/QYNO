/* Parameter-relative effects. Value aggregates have independent storage;
 * references and explicitly addressed homes may alias. Unknowns only widen. */
#include "optimizer.h"

static Var *root(Node *n)
{
    if (!n) return NULL;
    if (n->kind == ND_REFARG || n->kind == ND_ADDR) n = n->lhs;
    while (n && n->kind == ND_INDEX) n = n->lhs;
    return n && n->kind == ND_VAR ? n->var : NULL;
}
static int aliases(Var *a, Var *b)
{
    return a && b && (a == b || ((a->is_ref || a->opt_addressed) &&
        (b->is_ref || b->opt_addressed)));
}
static unsigned target(Node *place, unsigned effects)
{
    if (place && place->kind == ND_REFARG) place = place->lhs;
    if (place && place->kind == ND_INDEX) {
        if (!type_is_array(place->ty) && !type_is_model(place->ty))
            effects &= ~(OA_SHAPE | OA_REBIND);
        effects |= OA_WRITE;
    }
    return effects;
}
static unsigned mutation(Node *n, Var **place)
{
    unsigned effects = 0;
    *place = NULL;
    switch (n->kind) {
    case ND_TRY: case ND_MATCH:
        *place=n->var;effects=OA_WRITE|OA_SHAPE|OA_REBIND;break;
    case ND_DECL: case ND_ASSIGN:
        *place = n->var;
        effects = OA_WRITE | ((type_is_array(n->var->ty) || type_is_model(n->var->ty)) ? OA_SHAPE | OA_REBIND : 0);
        break;
    case ND_ADDR_ASSIGN: *place = n->var; effects = OA_ALL; break;
    case ND_METHOD: *place = root(n->lhs); effects = OA_WRITE | OA_SHAPE; break;
    case ND_STORE:
        *place = root(n->lhs); effects = target(n->lhs, OA_WRITE | OA_SHAPE); break;
    default: break;
    }
    return effects;
}
static int changes(Node *list, Var *wanted, unsigned mask, int level)
{
    Node *n, *arg;
    Var *v, *param;
    InputField *field;
    unsigned effects;
    if (level > 128) return 1;
    for (n = list; n; n = n->next) {
        effects = mutation(n, &v);
        if (((n->kind == ND_DECL && v == wanted) ||
            (n->kind != ND_DECL && aliases(v, wanted))) && (effects & mask)) return 1;
        if (n->kind == ND_INPUT) for (field = n->fields; field; field = field->next)
            if (aliases(root(field->var), wanted)) return 1;
        if (n->kind == ND_CALL || n->kind == ND_CONSTRUCT) {
            if (!n->func || (n->func->opt_unknown_memory && (wanted->is_ref || wanted->opt_addressed))) return 1;
            param = n->func->params;
            for (arg = n->lhs; arg; arg = arg->next) {
                if (arg->ty == TY_ADDR && (wanted->is_ref || wanted->opt_addressed)) return 1;
                if (arg->kind == ND_REFARG && aliases(root(arg), wanted) &&
                    (target(arg, param ? param->opt_access : OA_ALL) & mask)) return 1;
                if (param) param = param->next;
            }
        } else if (n->kind == ND_BUILTIN) {
            for (arg = n->lhs; arg; arg = arg->next)
                if (arg->kind == ND_REFARG && aliases(root(arg), wanted)) return 1;
            if (wanted->opt_addressed) return 1;
        }
        if (changes(n->lhs,wanted,mask,level+1) || changes(n->rhs,wanted,mask,level+1) ||
            changes(n->cond,wanted,mask,level+1) || changes(n->then,wanted,mask,level+1) ||
            changes(n->els,wanted,mask,level+1) || changes(n->init,wanted,mask,level+1) ||
            changes(n->step,wanted,mask,level+1) || changes(n->end,wanted,mask,level+1)) return 1;
    }
    return 0;
}
static int unknown_memory(Node *list,int level)
{
    Node *n;
    if(level>128)return 1;
    for(n=list;n;n=n->next){
        if(n->kind==ND_CALL&&(!n->func||n->func->opt_unknown_memory))return 1;
        if(unknown_memory(n->lhs,level+1)||unknown_memory(n->rhs,level+1)||
            unknown_memory(n->cond,level+1)||unknown_memory(n->then,level+1)||
            unknown_memory(n->els,level+1)||unknown_memory(n->init,level+1)||unknown_memory(n->step,level+1))return 1;
    }
    return 0;
}
int optimizer_may_change(Node *nodes, Var *v, int shape_only)
{
    return !v || changes(nodes, v, shape_only ? OA_SHAPE | OA_REBIND | OA_ESCAPE : OA_WRITE | OA_SHAPE | OA_REBIND | OA_ESCAPE, 0);
}
static void addressed(Node *list, int level)
{
    Node *n;
    Var *v;
    if (level > 128) return;
    for (n = list; n; n = n->next) {
        if (n->kind == ND_ADDR && (v = root(n->lhs))) v->opt_addressed = 1;
        addressed(n->lhs,level+1); addressed(n->rhs,level+1); addressed(n->cond,level+1);
        addressed(n->then,level+1); addressed(n->els,level+1); addressed(n->init,level+1);
        addressed(n->step,level+1); addressed(n->inc,level+1); addressed(n->end,level+1);
        if (n->opt_inline) addressed(n->opt_inline->body,level+1);
    }
}
static int escaping_arguments(Node *list,int all,int level)
{
    Node *n,*arg;
    Var *p,*v;
    int changed=0;
    if(level>128)return 0;
    for(n=list;n;n=n->next){
        if(n->kind==ND_CALL){
            p=n->func?n->func->params:NULL;
            for(arg=n->lhs;arg;arg=arg->next){
                if(arg->kind==ND_REFARG&&(all||!p||(p->opt_access&OA_ESCAPE))&&(v=root(arg))&&!v->opt_addressed){v->opt_addressed=1;changed=1;}
                if(p)p=p->next;
            }
        }
        changed|=escaping_arguments(n->lhs,all,level+1)|escaping_arguments(n->rhs,all,level+1)|
            escaping_arguments(n->cond,all,level+1)|escaping_arguments(n->then,all,level+1)|
            escaping_arguments(n->els,all,level+1)|escaping_arguments(n->init,all,level+1)|
            escaping_arguments(n->step,all,level+1)|escaping_arguments(n->end,all,level+1);
        if(n->opt_inline)changed|=escaping_arguments(n->opt_inline->body,all,level+1);
    }
    return changed;
}
void optimizer_memory(Ast *ast)
{
    Function *fn;
    Var *v;
    int changed, rounds = 0;
    for (fn = ast->funcs; fn; fn = fn->next) if (!fn->is_template) {
        addressed(fn->body,0);
        for(v=fn->params;v;v=v->next)if(v->ty==TY_ADDR)fn->opt_unknown_memory=1;
    }
    do {
        changed = 0;
        for (fn = ast->funcs; fn; fn = fn->next) if (!fn->is_template) {
            changed|=escaping_arguments(fn->body,0,0);
            if(!fn->opt_unknown_memory&&unknown_memory(fn->body,0)){fn->opt_unknown_memory=1;changed=1;}
            for (v = fn->params; v; v = v->next) if (v->is_ref) {
                unsigned effects = OA_READ;
                if (optimizer_may_change(fn->body,v,0)) effects |= OA_WRITE;
                if (optimizer_may_change(fn->body,v,1)) effects |= OA_SHAPE | OA_REBIND;
                if (v->opt_addressed) effects |= OA_ESCAPE;
                if ((v->opt_access | effects) != v->opt_access) { v->opt_access |= effects; changed = 1; }
            }
        }
    } while (changed && ++rounds < 128);
    if (changed) for (fn = ast->funcs; fn; fn = fn->next) {
        fn->opt_unknown_memory=1;
        for (v = fn->params; v; v = v->next) if (v->is_ref) v->opt_access = OA_ALL;
        escaping_arguments(fn->body,1,0);
    }
}
