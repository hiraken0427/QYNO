/* Typed straight-line HIR -> virtual registers. Effectful operations remain
 * explicit barriers. Every source store is retained, including addressable
 * homes, so CFG joins and legacy cleanup have the ordinary memory state. */
#include "machine_ir.h"
#include <stdlib.h>
#include <string.h>

#define MACHINE_LIMIT 512
typedef struct {Var *var;int value;} Binding;
typedef struct {Var *var;ModelField *field;int value;} FieldBinding;
typedef struct {Var *formal,*actual;} RootBinding;
typedef struct {
    MachineRegion region;
    MachineInst ops[MACHINE_LIMIT];
    Binding bindings[128];
    FieldBinding fields[128];
    int field_count;
    RootBinding roots[64];
    int root_count,strict;
    int bound_count,failed,arithmetic,saved_value;
    Node *saved;
} Builder;

int optimizer_scalar(Type ty)
{
    return ty==TY_BOOL||ty==TY_FLOAT||ty==TY_DOUBLE||(type_is_integer(ty)&&!type_is_wide(ty));
}
static int append(Builder *b,MachineOp op,Node *n,int a,int c,uint64_t bits,Var *root,int reuse)
{
    int i,id=b->region.count;
    MachineInst *p;
    if(b->failed||id==MACHINE_LIMIT){b->failed=1;return -1;}
    if(reuse)for(i=id-1;i>=0;i--){
        p=&b->ops[i];
        if(p->available&&p->op==op&&p->ty==n->ty&&p->a==a&&p->b==c&&p->root==root&&p->bits==bits&&
            (op!=MI_BINARY||p->source->kind==n->kind)&&
            (op!=MI_CONVERT||p->source->lhs->ty==n->lhs->ty)&&
            (op!=MI_COMPARE||(p->source->val==n->val&&p->source->cmp_ty==n->cmp_ty)))return i;
    }
    p=&b->ops[id];memset(p,0,sizeof(*p));
    p->op=op;p->source=n;p->ty=n->ty;p->a=a;p->b=c;p->root=root;p->bits=bits;p->available=1;p->last=-1;
    b->region.count++;return id;
}
static void context(Builder *b,Node *n)
{append(b,MI_CONTEXT,n,-1,-1,0,NULL,0);}
static void barrier(Builder *b)
{
    int i;
    b->bound_count=0;b->field_count=0;
    for(i=0;i<b->region.count;i++)if(b->ops[i].op==MI_VARIABLE||b->ops[i].op==MI_FIELD||
        b->ops[i].op==MI_ARRAY||b->ops[i].op==MI_LENGTH)b->ops[i].available=0;
}
static int simple_var(Node *n)
{return n&&n->kind==ND_VAR&&n->var&&!n->var->is_const;}
static int conversion(Type from,Type to)
{
    if(from==to)return 1;
    if((from==TY_FLOAT||from==TY_DOUBLE)&&(to==TY_FLOAT||to==TY_DOUBLE))return 1;
    if(type_is_integer(from)&&!type_is_wide(from)){
        if((to==TY_FLOAT||to==TY_DOUBLE)&&integer_width(from)<=32)return 1;
        if(type_is_integer(to)&&!type_is_wide(to))
            return integer_unsigned(from)==integer_unsigned(to)?integer_width(to)>=integer_width(from):
                integer_unsigned(from)&&!integer_unsigned(to)&&integer_width(to)>integer_width(from);
    }
    return 0;
}
static Var *actual_root(Builder *b,Var *v)
{
    int i;
    for(i=b->root_count-1;i>=0;i--)if(b->roots[i].formal==v)return b->roots[i].actual;
    return v;
}
static int index_safe(Node *n,int depth)
{
    if(!n||depth>64)return 0;
    switch(n->kind){
    case ND_NUM:return 1;
    case ND_VAR:return !n->var->is_ref&&!n->var->opt_addressed;
    case ND_CAST:return conversion(n->lhs->ty,n->ty)&&type_is_integer(n->ty)&&index_safe(n->lhs,depth+1);
    case ND_ADD:case ND_SUB:case ND_MUL:return index_safe(n->lhs,depth+1)&&index_safe(n->rhs,depth+1);
    default:return 0;
    }
}
static void bind(Builder *b,Var *v,int value);
static int expression(Builder *b,Node *n,int depth)
{
    int a,c,i,saved_roots,saved_strict,values[64];
    Node *arg,*body;
    Var *v,*receivers[64];
    uint64_t bits=0;
    if(!n||!optimizer_scalar(n->ty)||depth>128){b->failed=1;return -1;}
    switch(n->kind){
    case ND_NUM:case ND_BOOL:bits=(uint64_t)n->val;break;
    case ND_FNUM:
        if(n->ty==TY_FLOAT){float f=(float)n->fval;uint32_t v;memcpy(&v,&f,4);bits=v;}
        else memcpy(&bits,&n->fval,8);
        break;
    case ND_SAVED:
        if(n==b->saved)return b->saved_value;
        goto opaque;
    case ND_VAR:
        if(n->var->is_const)return expression(b,n->var->const_val,depth+1);
        if(!n->var->is_ref&&!n->var->opt_addressed)
            for(i=b->bound_count-1;i>=0;i--)if(b->bindings[i].var==n->var)return b->bindings[i].value;
        return append(b,MI_VARIABLE,n,-1,-1,0,n->var,!n->var->opt_addressed);
    case ND_INDEX:
        if(n->field&&simple_var(n->lhs)&&type_is_model(n->lhs->ty)){
            v=actual_root(b,n->lhs->var);
            context(b,n);
            for(i=b->field_count-1;i>=0;i--)if(b->fields[i].var==v&&b->fields[i].field==n->field)return b->fields[i].value;
            return append(b,MI_FIELD,n,-1,-1,(uint64_t)n->field->index,v,!v->opt_addressed);
        }
        if(simple_var(n->lhs)&&type_is_array(n->lhs->ty)&&n->opt_bounds==1&&n->opt_proof_owner==b->region.owner&&
            index_safe(n->rhs,0)){
            a=expression(b,n->rhs,depth+1);context(b,n);
            return append(b,MI_ARRAY,n,a,-1,0,n->lhs->var,!n->lhs->var->opt_addressed);
        }
        goto opaque;
    case ND_LENGTH:
        if(simple_var(n->lhs)&&type_is_array(n->lhs->ty))
            return append(b,MI_LENGTH,n,-1,-1,0,n->lhs->var,!n->lhs->var->opt_addressed);
        goto opaque;
    case ND_ADD:case ND_SUB:case ND_MUL:case ND_DIV:case ND_MOD:
    case ND_BIT_AND:case ND_BIT_OR:case ND_BIT_XOR:case ND_XOR:
        a=expression(b,n->lhs,depth+1);c=expression(b,n->rhs,depth+1);b->arithmetic++;
        return append(b,MI_BINARY,n,a,c,0,NULL,1);
    case ND_CAST:
        if(!conversion(n->lhs->ty,n->ty))goto opaque;
        a=expression(b,n->lhs,depth+1);b->arithmetic++;
        return append(b,MI_CONVERT,n,a,-1,0,NULL,1);
    case ND_CALL:
        if(!n->opt_inline||n->opt_inline->owner!=b->region.owner||n->opt_inline->param_count>64)goto opaque;
        body=n->opt_inline->body;
        if(!body||body->kind!=ND_RETURN||body->next||!body->lhs)goto opaque;
        saved_roots=b->root_count;saved_strict=b->strict;
        for(arg=n->lhs,i=0;arg&&i<n->opt_inline->param_count;arg=arg->next,i++){
            v=n->opt_inline->params[i];receivers[i]=NULL;
            if(v->is_ref){
                if(!type_is_model(v->ty)||arg->kind!=ND_REFARG||arg->rhs||!simple_var(arg->lhs))goto opaque;
                receivers[i]=actual_root(b,arg->lhs->var);
                /* A reference binding is captured before later arguments.
                 * Only a stable direct receiver can be substituted. */
                if(receivers[i]->is_ref&&arg->next&&(optimizer_effects(arg->next)&
                    (OPT_WRITE|OPT_ESCAPE|OPT_EXTERNAL|OPT_BORROW|OPT_ALLOCATE|OPT_RAISE)))goto opaque;
                values[i]=-1;
            }else if(!optimizer_scalar(v->ty)||!optimizer_scalar(arg->ty))goto opaque;
        }
        /* Eligibility precedes all IR mutations, then arguments are bound
         * exactly once, in source order, before evaluating the return. */
        for(arg=n->lhs,i=0;arg;arg=arg->next,i++){
            if(receivers[i])append(b,MI_REF_CHECK,arg,-1,-1,0,receivers[i],0);
            else values[i]=expression(b,arg,depth+1);
        }
        for(i=0;i<n->opt_inline->param_count;i++){
            v=n->opt_inline->params[i];
            if(receivers[i]){if(b->root_count==64){b->failed=1;return -1;}b->roots[b->root_count++]=(RootBinding){v,receivers[i]};}
            else bind(b,v,values[i]);
        }
        b->strict=1;a=expression(b,body->lhs,depth+1);b->strict=saved_strict;b->root_count=saved_roots;
        b->arithmetic++;return a;
    case ND_NEG:
        if(!type_is_integer(n->ty))goto opaque;
        a=expression(b,n->lhs,depth+1);b->arithmetic++;
        return append(b,MI_NEGATE,n,a,-1,0,NULL,1);
    case ND_NOT:case ND_BIT_NOT:
        a=expression(b,n->lhs,depth+1);b->arithmetic++;
        return append(b,MI_NOT,n,a,-1,(uint64_t)n->kind,NULL,1);
    case ND_CHAIN:
        if(n->lhs->next||n->lhs->lhs->ty!=n->lhs->cmp_ty||n->lhs->rhs->ty!=n->lhs->cmp_ty||
            !type_is_integer(n->lhs->cmp_ty)||type_is_wide(n->lhs->cmp_ty))goto opaque;
        a=expression(b,n->lhs->lhs,depth+1);c=expression(b,n->lhs->rhs,depth+1);b->arithmetic++;
        i=append(b,MI_COMPARE,n->lhs,a,c,0,NULL,0);
        if(i>=0)b->ops[i].ty=TY_BOOL;
        return i;
    default:goto opaque;
    }
    return append(b,MI_CONSTANT,n,-1,-1,bits,NULL,1);
opaque:
    if(b->strict){b->failed=1;return -1;}
    /* A legacy expression may call or collect even if its final type is
     * scalar. Its entire register/flag state is materialized at this edge. */
    barrier(b);return append(b,MI_OPAQUE,n,-1,-1,0,NULL,0);
}
static int store_candidate(Node *n)
{
    unsigned effects;
    Node *p=n->lhs;
    if(n->kind!=ND_STORE||!p||p->kind!=ND_INDEX||!p->field||!optimizer_scalar(p->ty)||
        !simple_var(p->lhs)||p->lhs->var->is_ref||p->lhs->var->opt_addressed)return 0;
    effects=optimizer_effects(n->val==UPDATE_SET?n->rhs:n->cond);
    return !(effects&(OPT_WRITE|OPT_RAISE|OPT_ALLOCATE|OPT_EXTERNAL|OPT_BORROW|OPT_RESOURCE|OPT_ESCAPE));
}
static int statement_candidate(Node *n)
{
    if(n->opt_dead_store)return 0;
    if(n->kind==ND_DECL||n->kind==ND_ASSIGN)
        return optimizer_scalar(n->var->ty)&&!n->var->is_const&&!(n->kind==ND_DECL&&n->var->is_ref);
    return store_candidate(n);
}
static void bind(Builder *b,Var *v,int value)
{
    int i;
    for(i=0;i<b->bound_count;i++)if(b->bindings[i].var==v){b->bindings[i].value=value;return;}
    if(b->bound_count<128)b->bindings[b->bound_count++]=(Binding){v,value};
}
static void statement(Builder *b,Node *n)
{
    int value,i,old;
    Node zero={0};
    if(n->kind!=ND_STORE){
        zero.kind=ND_NUM;zero.ty=n->var->ty;zero.tok=n->tok;
        value=expression(b,n->lhs?n->lhs:&zero,0);
        /* The zero node must outlive construction. Use the declaration as
         * the source; the instruction already owns its immediate bits. */
        if(!n->lhs&&value>=0)b->ops[value].source=n;
        append(b,MI_STORE,n,value,-1,0,n->var,0);
        if(n->var->is_ref||n->var->opt_addressed)barrier(b);
        else{
            for(i=0;i<b->region.count;i++)if(b->ops[i].op==MI_VARIABLE&&b->ops[i].root==n->var)b->ops[i].available=0;
            bind(b,n->var,value);
        }
        return;
    }
    context(b,n);old=expression(b,n->lhs,0);
    b->saved=n->init;b->saved_value=old;
    value=expression(b,n->val==UPDATE_SET?n->rhs:n->cond,0);
    b->saved=NULL;
    context(b,n);append(b,MI_FIELD_STORE,n,value,-1,(uint64_t)n->lhs->field->index,n->lhs->lhs->var,0);
    for(i=0;i<b->region.count;i++)if(b->ops[i].op==MI_FIELD&&b->ops[i].root==n->lhs->lhs->var&&b->ops[i].bits==(uint64_t)n->lhs->field->index)b->ops[i].available=0;
    for(i=0;i<b->field_count;i++)if(b->fields[i].var==n->lhs->lhs->var&&b->fields[i].field==n->lhs->field)break;
    if(i<128){b->fields[i]=(FieldBinding){n->lhs->lhs->var,n->lhs->field,value};if(i==b->field_count)b->field_count++;}
}
int machine_region_valid(const MachineRegion *r)
{
    int i;
    if(!r||r->count<1||r->count>MACHINE_LIMIT||(!r->statements&&(r->result<0||r->result>=r->count)))return 0;
    for(i=0;i<r->count;i++){
        const MachineInst *p=&r->inst[i];
        if(p->a>=i||p->b>=i||p->a < -1||p->b < -1||!p->source)return 0;
        if(p->op!=MI_CONTEXT&&p->op!=MI_STORE&&p->op!=MI_FIELD_STORE&&p->op!=MI_REF_CHECK&&!optimizer_scalar(p->ty))return 0;
    }
    return 1;
}
static MachineRegion *finish(Builder *b)
{
    MachineRegion *r;
    int i;
    if(b->failed||!b->region.count||b->region.owner->opt_machine_ops>65536-b->region.count)return NULL;
    b->region.owner->opt_machine_ops+=b->region.count;
    r=malloc(sizeof(*r));if(!r)fatal("機械命令の領域を確保できません");*r=b->region;
    r->inst=malloc((size_t)r->count*sizeof(*r->inst));if(!r->inst)fatal("機械命令を確保できません");
    memcpy(r->inst,b->ops,(size_t)r->count*sizeof(*r->inst));
    for(i=0;i<r->count;i++){
        if(r->inst[i].a>=0)r->inst[r->inst[i].a].last=i;
        if(r->inst[i].b>=0)r->inst[r->inst[i].b].last=i;
    }
    if(!r->statements)r->inst[r->result].last=r->count;
    if(!machine_region_valid(r))fatal("内部エラー: 機械命令の値が不整合です");
    if(r->count>r->owner->opt_machine_slots)r->owner->opt_machine_slots=r->count;
    return r;
}
static void lower(Function *fn,Node *list,int depth,int *budget)
{
    Node *n,*end;
    InputField *f;
    Builder *b;
    int count;
    if(depth>128||*budget<=0)return;
    for(n=list;n&&--*budget>=0;n=n->next){
        lower(fn,n->lhs,depth+1,budget);lower(fn,n->rhs,depth+1,budget);lower(fn,n->cond,depth+1,budget);
        lower(fn,n->then,depth+1,budget);lower(fn,n->els,depth+1,budget);lower(fn,n->init,depth+1,budget);
        lower(fn,n->step,depth+1,budget);lower(fn,n->inc,depth+1,budget);lower(fn,n->for_amount,depth+1,budget);lower(fn,n->end,depth+1,budget);
        for(f=n->fields;f;f=f->next)lower(fn,f->var,depth+1,budget);
        if(n->opt_inline&&n->opt_inline->owner==fn)lower(fn,n->opt_inline->body,depth+1,budget);
        if(optimizer_scalar(n->ty)){
            b=calloc(1,sizeof(*b));if(!b)fatal("数値命令を確保できません");b->region.owner=fn;
            b->region.result=expression(b,n,0);
            if(b->arithmetic)n->opt_machine=finish(b);
            free(b);
        }
    }
    /* Maximal scalar statement runs, with stores left visible at every CFG
     * boundary. Control flow, resources and cleanup never cross this region. */
    for(n=list;n;n=n->next)if(statement_candidate(n)){
        b=calloc(1,sizeof(*b));if(!b)fatal("数値ブロックを確保できません");
        b->region.owner=fn;b->region.statements=1;
        for(end=n,count=0;end&&count<32&&statement_candidate(end);end=end->next,count++)statement(b,end);
        b->region.after=end;
        if(b->arithmetic||count>1)n->opt_machine=finish(b);
        free(b);
        while(n->next&&n->next!=end)n=n->next;
    }
}
void optimizer_machine(Ast *ast)
{
    Function *fn;
    int budget;
    for(fn=ast->funcs;fn;fn=fn->next)if(!fn->is_template){
        budget=65536;lower(fn,fn->body,0,&budget);
        fn->opt_machine_base=fn->stack_size;
        fn->stack_size=(fn->stack_size+fn->opt_machine_slots*8+15)/16*16;
    }
}
