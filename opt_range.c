/* Closed difference bounds: x-y<=c. Facts describe values at an access, not
 * mathematical integers with an assumed absence of machine overflow. */
#include "optimizer.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define RANGE_SYMBOLS 32
#define RANGE_INF (INT64_C(1) << 55)
typedef struct { int symbol, valid; int64_t offset; } Term;
typedef struct {
    Var *vars[RANGE_SYMBOLS];
    int count, length[RANGE_SYMBOLS];
    Function *fn;
    int budget;
} Context;
typedef struct { int64_t d[RANGE_SYMBOLS][RANGE_SYMBOLS]; int live; } Facts;

static int symbol(Context *c, Var *v, int length)
{
    int i;
    for (i=1;i<c->count;i++) if(c->vars[i]==v && c->length[i]==length) return i;
    return -1;
}
static void constrain(Context *c,Facts *s,int a,int b,int64_t limit)
{
    int i,j;
    int64_t value;
    if(a<0||b<0||limit>=s->d[a][b]||!s->live)return;
    for(i=0;i<c->count;i++) if(s->d[i][a]<RANGE_INF)
        for(j=0;j<c->count;j++) if(s->d[b][j]<RANGE_INF){
            value=s->d[i][a]+limit+s->d[b][j];
            if(value < -RANGE_INF/2)value=-RANGE_INF/2;
            if(value<s->d[i][j])s->d[i][j]=value;
        }
    for(i=0;i<c->count;i++)if(s->d[i][i]<0)s->live=0;
}
static void type_bounds(Context *c,Facts *s,int i)
{
    Var *v=c->vars[i];
    int width;
    if(c->length[i]) { constrain(c,s,0,i,0); return; }
    width=integer_width(v->ty);
    if(integer_unsigned(v->ty))constrain(c,s,0,i,0);
    if(width<55){
        constrain(c,s,i,0,integer_unsigned(v->ty)?(INT64_C(1)<<width)-1:(INT64_C(1)<<(width-1))-1);
        if(!integer_unsigned(v->ty))constrain(c,s,0,i,INT64_C(1)<<(width-1));
    }
}
static void forget(Context *c,Facts *s,int id)
{
    int i;
    for(i=0;i<c->count;i++)s->d[id][i]=s->d[i][id]=i==id?0:RANGE_INF;
    type_bounds(c,s,id);
}
static void initial(Context *c,Facts *s)
{
    int i,j;
    memset(s,0,sizeof(*s));s->live=1;
    for(i=0;i<c->count;i++)for(j=0;j<c->count;j++)s->d[i][j]=i==j?0:RANGE_INF;
    for(i=1;i<c->count;i++)type_bounds(c,s,i);
}
static int widening(Type from,Type to)
{
    return type_is_integer(from)&&type_is_integer(to)&&!type_is_wide(from)&&!type_is_wide(to)&&
        (integer_unsigned(from)==integer_unsigned(to)?integer_width(to)>=integer_width(from):
         integer_unsigned(from)&&!integer_unsigned(to)&&integer_width(to)>integer_width(from));
}
static int fits(Type ty,int64_t low,int64_t high)
{
    int bits;
    if(!type_is_integer(ty)||type_is_wide(ty))return 0;
    bits=integer_width(ty);
    if(integer_unsigned(ty))return low>=0&&(bits>=55||high<=((INT64_C(1)<<bits)-1));
    return bits>=55||(low>=-(INT64_C(1)<<(bits-1))&&high<=(INT64_C(1)<<(bits-1))-1);
}
static Term term(Context *c,Facts *s,Node *n,int depth)
{
    Term t={0},r;
    int id;
    int64_t delta,low,high;
    if(!n||depth>64)return t;
    if(n->kind==ND_CAST&&widening(n->lhs->ty,n->ty))return term(c,s,n->lhs,depth+1);
    if(n->kind==ND_NUM && n->val > -RANGE_INF/8 && n->val < RANGE_INF/8){
        t.valid=1;t.offset=n->val;return t;
    }
    if(n->kind==ND_VAR && !n->var->is_ref && !n->var->opt_addressed){
        if(n->var->is_const)return term(c,s,n->var->const_val,depth+1);
        id=symbol(c,n->var,0);
        if(id>=0){t.valid=1;t.symbol=id;}return t;
    }
    if(n->kind==ND_LENGTH && n->lhs->kind==ND_VAR && type_is_array(n->lhs->ty)){
        id=symbol(c,n->lhs->var,1);
        if(id>=0){t.valid=1;t.symbol=id;}return t;
    }
    if(n->kind!=ND_ADD&&n->kind!=ND_SUB)return t;
    t=term(c,s,n->lhs,depth+1);r=term(c,s,n->rhs,depth+1);
    if(!t.valid||!r.valid||r.symbol)return (Term){0};
    delta=n->kind==ND_ADD?r.offset:-r.offset;
    if(s->d[t.symbol][0]>=RANGE_INF||s->d[0][t.symbol]>=RANGE_INF)return (Term){0};
    low=-s->d[0][t.symbol]+t.offset+delta;high=s->d[t.symbol][0]+t.offset+delta;
    if(!fits(n->ty,low,high))return (Term){0};
    t.offset+=delta;return t;
}
static void relation(Context *c,Facts *s,Term a,Term b,int strict)
{
    if(a.valid&&b.valid)constrain(c,s,a.symbol,b.symbol,b.offset-a.offset-strict);
}
static void assume(Context *c,Facts *s,Node *n,int truth)
{
    Node *cmp;
    Term a,b;
    CmpKind kind;
    if(!n)return;
    if(n->kind==ND_NOT){assume(c,s,n->lhs,!truth);return;}
    if(n->kind==ND_AND&&truth){assume(c,s,n->lhs,1);assume(c,s,n->rhs,1);return;}
    if(n->kind==ND_BOOL){if((n->val!=0)!=truth)s->live=0;return;}
    if(n->kind!=ND_CHAIN||(!truth&&n->lhs->next))return;
    for(cmp=n->lhs;cmp;cmp=cmp->next){
        if(!type_is_integer(cmp->cmp_ty)||type_is_wide(cmp->cmp_ty))continue;
        /* Mixed signedness/width conversions are modeled only if lossless. */
        if(cmp->lhs->ty!=cmp->cmp_ty&&!widening(cmp->lhs->ty,cmp->cmp_ty))continue;
        if(cmp->rhs->ty!=cmp->cmp_ty&&!widening(cmp->rhs->ty,cmp->cmp_ty))continue;
        a=term(c,s,cmp->lhs,0);b=term(c,s,cmp->rhs,0);kind=(CmpKind)cmp->val;
        if(!truth){switch(kind){case CMP_LT:kind=CMP_GE;break;case CMP_LE:kind=CMP_GT;break;
            case CMP_GT:kind=CMP_LE;break;case CMP_GE:kind=CMP_LT;break;
            case CMP_EQ:kind=CMP_NE;break;case CMP_NE:kind=CMP_EQ;break;}}
        if(kind==CMP_LT||kind==CMP_LE||kind==CMP_EQ)relation(c,s,a,b,kind==CMP_LT);
        if(kind==CMP_GT||kind==CMP_GE||kind==CMP_EQ)relation(c,s,b,a,kind==CMP_GT);
    }
}
static void invalidate(Context *c,Facts *s,Node *n,int whole_list)
{
    Node copy;
    int i;
    if(!n)return;
    copy=*n;
    if(!whole_list)copy.next=NULL;
    for(i=1;i<c->count;i++)if(optimizer_may_change(&copy,c->vars[i],c->length[i]))forget(c,s,i);
}
static void access(Context *c,Facts *s,Node *n)
{
    Term index,len;
    int safe=0;
    if(n->field||!type_is_array(n->lhs->ty))return;
    index=term(c,s,n->rhs,0);
    if(n->lhs->kind==ND_VAR){
        len=(Term){symbol(c,n->lhs->var,1),1,0};
        if(len.symbol<0)len.valid=0;
        safe=s->live&&index.valid&&len.valid&&s->d[0][index.symbol]<=index.offset&&
            s->d[index.symbol][len.symbol] < -index.offset;
    }
    if(!n->opt_bounds)n->opt_proof_owner=c->fn;
    if(n->opt_proof_owner!=c->fn||!safe)n->opt_bounds=2;
    else if(!n->opt_bounds)n->opt_bounds=1;
}
static void expression(Context *c,Facts *s,Node *list,int depth)
{
    Node *n;
    if(depth>128||c->budget<=0)return;
    for(n=list;n;n=n->next){
        if(--c->budget<0)return;
        expression(c,s,n->lhs,depth+1);expression(c,s,n->rhs,depth+1);
        if(n->kind==ND_INDEX)access(c,s,n);
        if(n->kind==ND_CALL||n->kind==ND_BUILTIN||n->kind==ND_INPUT||n->kind==ND_METHOD||n->kind==ND_ADDR_ASSIGN)
            invalidate(c,s,n,0);
    }
}
static void statements(Context *c,Facts *s,Node *list,int depth)
{
    Node *n,*arm,*cmp;
    Facts a,b;
    Term start,counter;
    int i,j,id;
    if(depth>64||c->budget<=0){initial(c,s);return;}
    for(n=list;n&&s->live;n=n->next){
        if(--c->budget<0){initial(c,s);return;}
        switch(n->kind){
        case ND_IF:
            expression(c,s,n->cond,0);a=*s;b=*s;assume(c,&a,n->cond,1);assume(c,&b,n->cond,0);
            statements(c,&a,n->then,depth+1);statements(c,&b,n->els,depth+1);
            if(!a.live)*s=b;else if(!b.live)*s=a;
            else for(i=0;i<c->count;i++)for(j=0;j<c->count;j++)s->d[i][j]=a.d[i][j]>b.d[i][j]?a.d[i][j]:b.d[i][j];
            break;
        case ND_FOR:
            expression(c,s,n->init->lhs,0);a=*s;
            invalidate(c,&a,n->then,1);id=symbol(c,n->init->var,0);
            if(id>=0)forget(c,&a,id);
            expression(c,&a,n->cond,0);assume(c,&a,n->cond,1);
            if(id>=0&&type_is_integer(n->init->var->ty)&&!type_is_wide(n->init->var->ty)&&
                !optimizer_may_change(n->then,n->init->var,0)){
                /* The start relation is usable only if its referenced values
                 * are invariant across every backedge. */
                start=term(c,&a,n->init->lhs,0);counter=(Term){id,1,0};
                if(start.valid&&start.symbol&&optimizer_may_change(n->then,c->vars[start.symbol],c->length[start.symbol]))start.valid=0;
                if(n->inc_sign>0)relation(c,&a,start,counter,n->for_open);
                else relation(c,&a,counter,start,n->for_open);
            }
            statements(c,&a,n->then,depth+1);
            invalidate(c,s,n,0);
            break;
        case ND_WHILE:
            a=*s;invalidate(c,&a,n->then,1);expression(c,&a,n->cond,0);assume(c,&a,n->cond,1);
            statements(c,&a,n->then,depth+1);invalidate(c,s,n,0);break;
        case ND_TRY: case ND_MATCH:
            initial(c,&a);expression(c,&a,n->lhs,0);statements(c,&a,n->then,depth+1);
            for(arm=n->els;arm;arm=arm->next){initial(c,&a);statements(c,&a,arm->then,depth+1);}
            initial(c,&a);statements(c,&a,n->rhs,depth+1);initial(c,s);break;
        case ND_DECL: case ND_ASSIGN:
            expression(c,s,n->lhs,0);start=term(c,s,n->lhs,0);
            invalidate(c,s,n,0);id=symbol(c,n->var,0);
            if(id>=0){
                forget(c,s,id);
                if(!n->lhs&&n->kind==ND_DECL)start=(Term){0,1,0};
                if(start.valid&&start.symbol!=id){counter=(Term){id,1,0};relation(c,s,counter,start,0);relation(c,s,start,counter,0);}
            }
            break;
        case ND_RETURN: case ND_THROW:
            expression(c,s,n->lhs,0);s->live=0;break;
        case ND_BREAK: case ND_CONTINUE:s->live=0;break;
        default:
            if(n->kind==ND_STORE){
                Node *p=n->lhs,*base=p;
                while(base&&base->kind==ND_INDEX)base=base->lhs;
                if(base&&base->kind==ND_VAR&&optimizer_may_change(n->val==UPDATE_SET?n->rhs:n->cond,base->var,1))
                    for(;p&&p->kind==ND_INDEX;p=p->lhs)p->opt_bounds=2;
            }
            expression(c,s,n->lhs,0);expression(c,s,n->rhs,0);expression(c,s,n->cond,0);
            if(n->kind==ND_STORE||n->kind==ND_METHOD||n->kind==ND_INPUT||n->kind==ND_ADDR_ASSIGN)invalidate(c,s,n,0);
            break;
        }
    }
    (void)cmp;
}
static void cache_lengths(Function *fn,Node *list,Var *root,OptHoist *h,int depth)
{
    Node *n;
    if(depth>128)return;
    for(n=list;n;n=n->next){
        if(n->kind==ND_LENGTH&&n->lhs->kind==ND_VAR&&n->lhs->var==root&&!n->opt_cached)n->opt_cached=h;
        cache_lengths(fn,n->lhs,root,h,depth+1);cache_lengths(fn,n->rhs,root,h,depth+1);
        cache_lengths(fn,n->cond,root,h,depth+1);cache_lengths(fn,n->then,root,h,depth+1);
        cache_lengths(fn,n->els,root,h,depth+1);
    }
    (void)fn;
}
typedef struct {Var *formal,*actual;} Receiver;
static Var *actual_receiver(Node *n,Receiver *map,int count)
{
    Var *v;
    int i;
    if(n&&n->kind==ND_REFARG)n=n->lhs;
    if(!n||n->kind!=ND_VAR)return NULL;
    v=n->var;
    for(i=0;i<count;i++)if(map[i].formal==v)return map[i].actual;
    return v;
}
static void cache_fields(Function *fn,Node *loop,Node *list,Receiver *map,int count,int depth,int *budget)
{
    Node *n,*arg;
    Var *v;
    Receiver bindings[64];
    OptHoist *h;
    int i;
    if(depth>128)return;
    for(n=list;n&&--*budget>=0;n=n->next){
        if(n->kind==ND_INDEX&&n->field&&n->lhs->kind==ND_VAR&&type_is_model(n->lhs->ty)&&
            optimizer_scalar(n->ty)&&!n->opt_cached&&fn->opt_hoist_checks++<256){
            v=actual_receiver(n->lhs,map,count);
            if(v&&!v->is_const&&!v->opt_addressed&&(v->is_param||(v->decl&&v->decl->kind==ND_DECL))&&
                !optimizer_may_change(loop->then,v,0)&&
                !optimizer_may_change(loop->cond,v,0)&&
                (!v->is_ref||(v->is_param&&!fn->is_initializer&&!fn->is_internal&&!optimizer_may_change(fn->body,v,1)))){
                h=calloc(1,sizeof(*h));if(!h)fatal("不変フィールドを保持できません");
                h->owner=fn;h->root=v;h->source=malloc(sizeof(Node));if(!h->source)fatal("フィールド式を保持できません");
                *h->source=*n;h->source->next=NULL;h->source->opt_cached=NULL;h->source->opt_machine=NULL;
                h->source->lhs=malloc(sizeof(Node));if(!h->source->lhs)fatal("受信者を保持できません");
                *h->source->lhs=*n->lhs;h->source->lhs->var=v;h->source->lhs->opt_machine=NULL;
                fn->stack_size+=8;h->slot=fn->stack_size;
                h->next=loop->opt_hoists;loop->opt_hoists=h;n->opt_cached=h;
            }
        }
        cache_fields(fn,loop,n->lhs,map,count,depth+1,budget);cache_fields(fn,loop,n->rhs,map,count,depth+1,budget);
        cache_fields(fn,loop,n->cond,map,count,depth+1,budget);cache_fields(fn,loop,n->then,map,count,depth+1,budget);
        cache_fields(fn,loop,n->els,map,count,depth+1,budget);
        if(n->opt_inline&&n->opt_inline->owner==fn){
            for(arg=n->lhs,i=0;arg&&i<n->opt_inline->param_count&&i<64;arg=arg->next,i++)
                bindings[i]=(Receiver){n->opt_inline->params[i],actual_receiver(arg,map,count)};
            cache_fields(fn,loop,n->opt_inline->body,bindings,i,depth+1,budget);
        }
    }
}
static void hoists(Function *fn,Node *list,int depth)
{
    Node *n,*bound;
    OptHoist *h;
    Var *v;
    if(depth>128)return;
    for(n=list;n;n=n->next){
        if(n->kind==ND_FOR){
            int field_budget=65536;
            bound=n->cond->lhs->rhs;
            while(bound->kind==ND_CAST&&widening(bound->lhs->ty,bound->ty))bound=bound->lhs;
            if(bound->kind==ND_LENGTH&&bound->lhs->kind==ND_VAR&&type_is_array(bound->lhs->ty)&&!bound->opt_cached){
                v=bound->lhs->var;
                if(!v->is_const&&!v->opt_addressed&&!optimizer_may_change(n->then,v,1)){
                    h=calloc(1,sizeof(*h));if(!h)fatal("ループの長さを保持できません");
                    h->owner=fn;h->root=v;h->source=malloc(sizeof(Node));if(!h->source)fatal("ループの式を保持できません");
                    *h->source=*bound;h->source->next=NULL;h->source->opt_cached=NULL;
                    fn->stack_size+=8;h->slot=fn->stack_size;
                    h->next=n->opt_hoists;n->opt_hoists=h;
                    cache_lengths(fn,n->cond,v,h,0);cache_lengths(fn,n->then,v,h,0);
                }
            }
            cache_fields(fn,n,n->then,NULL,0,0,&field_budget);
        }
        hoists(fn,n->then,depth+1);hoists(fn,n->els,depth+1);
        if(n->kind==ND_TRY){hoists(fn,n->lhs,depth+1);hoists(fn,n->rhs,depth+1);}
    }
}
static void clear_proofs(Node *list,int depth)
{
    Node *n;
    if(depth>128)return;
    for(n=list;n;n=n->next){
        if(n->opt_bounds==1)n->opt_bounds=2;
        clear_proofs(n->lhs,depth+1);clear_proofs(n->rhs,depth+1);clear_proofs(n->cond,depth+1);
        clear_proofs(n->then,depth+1);clear_proofs(n->els,depth+1);clear_proofs(n->init,depth+1);
        clear_proofs(n->step,depth+1);clear_proofs(n->inc,depth+1);clear_proofs(n->end,depth+1);
    }
}
void optimizer_ranges(Ast *ast)
{
    Function *fn;
    Var *v;
    for(fn=ast->funcs;fn;fn=fn->next)if(!fn->is_template){
        Context c={0};Facts state;
        c.fn=fn;c.count=1;c.budget=65536;
        for(v=fn->params;v&&c.count<RANGE_SYMBOLS;v=v->next)
            if(type_is_array(v->ty)||(type_is_integer(v->ty)&&!type_is_wide(v->ty))){c.vars[c.count]=v;c.length[c.count++]=type_is_array(v->ty);}
        for(v=fn->locals;v&&c.count<RANGE_SYMBOLS;v=v->next)
            if(!v->is_const&&(type_is_array(v->ty)||(type_is_integer(v->ty)&&!type_is_wide(v->ty)))){c.vars[c.count]=v;c.length[c.count++]=type_is_array(v->ty);}
        initial(&c,&state);statements(&c,&state,fn->body,0);
        if(c.budget>0)hoists(fn,fn->body,0);else clear_proofs(fn->body,0);
        fn->stack_size=(fn->stack_size+15)/16*16;
    }
}
