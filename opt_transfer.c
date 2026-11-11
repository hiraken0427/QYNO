/* Fresh scalar literals have one private header per evaluation. Only their
 * outer transfer is elided; source-variable copies and literal building stay. */
#include "optimizer.h"
#include <stdlib.h>

#ifndef QYNO_ARRAY_TRANSFER_BUDGET
#define QYNO_ARRAY_TRANSFER_BUDGET 65536
#endif
#ifndef QYNO_ARRAY_TRANSFER_LIMIT
#define QYNO_ARRAY_TRANSFER_LIMIT 4096
#endif
#define TRANSFER_FORBIDDEN (OPT_WRITE|OPT_ALLOCATE|OPT_EXTERNAL|OPT_BORROW|OPT_RESOURCE|OPT_ESCAPE|OPT_RAISE)

typedef struct {size_t left;int exhausted;} TransferBudget;
static int visit(TransferBudget *b,int depth)
{
    if(depth>128||!b->left){b->exhausted=1;return 0;}
    --b->left;return 1;
}
static int scalar(Type ty)
{return ty==TY_BOOL||ty==TY_FLOAT||ty==TY_DOUBLE||(type_is_integer(ty)&&!type_is_wide(ty));}
static int retain_buffer(long long length)
{
    uint64_t capacity=4,used=(uint64_t)length;
    if(!length)return 1;
    /* nargs bounds length to INT_MAX, so this arithmetic fits in uint64_t.
     * Account for eight-byte slots and the current 24-byte GC prefix. Do not
     * retain extra 4 KiB units of backing storage instead of an exact copy. */
    while(capacity<used)capacity*=2;
    return (capacity*8+24+4095)/4096==(used*8+24+4095)/4096;
}
static int array_owner(Node *n,TransferBudget *b,int depth)
{
    Var *v;
    if(!n||!visit(b,depth)||n->kind!=ND_VAR||!(v=n->var))return 0;
    return !v->is_const&&!v->is_ref&&!v->opt_addressed&&type_is_array(v->ty)&&
        array_rank(v->ty)==1&&scalar(array_element(v->ty));
}
static int element(Node *n,TransferBudget *b,int depth)
{
    Node *cmp;
    if(!n||!visit(b,depth)||!scalar(n->ty)||(optimizer_local_effects(n)&TRANSFER_FORBIDDEN))return 0;
    switch(n->kind){
    case ND_NUM:case ND_FNUM:case ND_BOOL:return 1;
    case ND_VAR:
        if(!n->var||n->var->is_ref||n->var->opt_addressed)return 0;
        return !n->var->is_const||element(n->var->const_val,b,depth+1);
    case ND_INDEX:
        return !n->field&&array_owner(n->lhs,b,depth+1)&&element(n->rhs,b,depth+1);
    case ND_LENGTH:return array_owner(n->lhs,b,depth+1);
    case ND_CAST:case ND_NEG:case ND_NOT:case ND_BIT_NOT:case ND_ABS:
        return element(n->lhs,b,depth+1);
    case ND_ADD:case ND_SUB:case ND_MUL:case ND_DIV:case ND_MOD:case ND_POW:
    case ND_BIT_AND:case ND_BIT_OR:case ND_BIT_XOR:case ND_XOR:case ND_AND:case ND_OR:
    case ND_SHL:case ND_SHR:
        return element(n->lhs,b,depth+1)&&element(n->rhs,b,depth+1);
    case ND_CHAIN:
        for(cmp=n->lhs;cmp;cmp=cmp->next)
            if(!visit(b,depth+1)||cmp->kind!=ND_CMP||!scalar(cmp->cmp_ty)||
               !element(cmp->lhs,b,depth+2)||!element(cmp->rhs,b,depth+2))return 0;
        return n->lhs!=NULL;
    default:return 0;
    }
}
static int candidate(Function *fn,Node *site,TransferBudget *b,ArrayTransferPlan *p)
{
    Node *literal,*item,*copy=NULL;
    Var *v,*local;
    long long count=0,limit;
    if(site->kind!=ND_DECL&&site->kind!=ND_ASSIGN)return 0;
    v=site->var;
    if(!v||v->is_param||v->is_global||v->is_const||v->is_readonly||v->is_ref||v->opt_addressed||
       !type_is_array(v->ty)||array_rank(v->ty)!=1||!scalar(array_element(v->ty)))return 0;
    for(local=fn->locals;local&&local!=v;local=local->next)if(!visit(b,0))return 0;
    if(!local)return 0;
    if(v->dims&&(!v->dims->checked||v->dims->next))return 0;
    limit=v->dims?v->dims->limit:-1;
    if(limit < -1)return 0;
    literal=site->lhs;
    if(site->kind==ND_DECL){
        copy=literal;
        if(!copy||copy->kind!=ND_ARRAY_COPY||copy->ty!=v->ty||copy->val!=1||copy->dims!=v->dims)return 0;
        literal=copy->lhs;
    }
    if(!literal||literal->kind!=ND_ARRAY_LITERAL||literal->constant_value||literal->ty!=v->ty)return 0;
    for(item=literal->lhs;item;item=item->next){
        if(item->ty!=array_element(v->ty)||!element(item,b,0))return 0;
        ++count;
    }
    if(count!=literal->nargs)return 0;
    if(!retain_buffer(count))return 0;
    if(limit>=0&&(count>limit||(site->kind==ND_DECL&&count!=limit)))return 0;
    if(p)*p=(ArrayTransferPlan){NULL,fn,site,literal,copy,v,v->ty,count,limit,0};
    return 1;
}
int optimizer_transfer_valid(const ArrayTransferPlan *p,Function *owner)
{
    TransferBudget budget={QYNO_ARRAY_TRANSFER_BUDGET,0};ArrayTransferPlan current;
    if(!p||!owner||!p->site||p->owner!=owner||p->conflict||p->site->opt_transfer!=p)return 0;
    if(!candidate(owner,p->site,&budget,&current))return 0;
    return current.producer==p->producer&&current.copy==p->copy&&current.target==p->target&&
        current.ty==p->ty&&current.length==p->length&&current.limit==p->limit;
}
static void walk(Function *fn,Node *n,int depth,TransferBudget *budget,ArrayTransferPlan **plans,int *count)
{
    ArrayTransferPlan value,*p,*prior;
    InputField *field;
    for(;n&&!budget->exhausted;n=n->next){
        if(!visit(budget,depth))return;
        if(candidate(fn,n,budget,&value)){
            if(*count>=QYNO_ARRAY_TRANSFER_LIMIT){budget->exhausted=1;return;}
            p=malloc(sizeof(*p));if(!p)fatal("配列転送の計画を確保できません");
            *p=value;
            for(prior=*plans;prior;prior=prior->next)if(prior->site==p->site||prior->producer==p->producer){prior->conflict=1;p->conflict=1;}
            p->next=*plans;*plans=p;++*count;
        }
#define WALK(member) walk(fn,n->member,depth+1,budget,plans,count)
        WALK(lhs);WALK(rhs);WALK(cond);WALK(then);WALK(els);WALK(init);WALK(step);WALK(inc);WALK(for_amount);WALK(end);
#undef WALK
        for(field=n->fields;field&&!budget->exhausted;field=field->next)walk(fn,field->var,depth+1,budget,plans,count);
        if(n->opt_inline&&n->opt_inline->owner==fn)walk(fn,n->opt_inline->body,depth+1,budget,plans,count);
    }
}
void optimizer_transfers(Ast *ast)
{
    Function *fn;
    for(fn=ast->funcs;fn;fn=fn->next)if(!fn->is_template){
        TransferBudget budget={QYNO_ARRAY_TRANSFER_BUDGET,0};
        ArrayTransferPlan *plans=NULL,*p,*next;int count=0;
        walk(fn,fn->body,0,&budget,&plans,&count);
        for(p=plans;p;p=next){
            next=p->next;
            if(budget.exhausted||p->conflict)free(p);
            else{
                p->next=NULL;
                p->site->opt_transfer=p;
                if(!optimizer_transfer_valid(p,fn))fatal("内部エラー: 配列転送の計画が不整合です");
            }
        }
    }
}
