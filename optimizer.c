/* Typed scalar HIR/CFG. Aggregate operations and handlers remain explicit
 * barriers; no source is rechecked or evaluated with the diagnostic evaluator. */
#include "optimizer.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define OPT_MAX_BLOCKS 4096
#define OPT_MAX_VARS 256
#define OPT_MAX_CELLS 262144

typedef struct { Type ty; uint64_t bits; int known; } Scalar;
typedef struct {
    Node *source;
    Node *condition;
    Var *kill;
    int next[2], reached, dirty, barrier;
    Scalar *in, *out;
} Block;
typedef struct {
    Var *vars[OPT_MAX_VARS];
    int eligible[OPT_MAX_VARS], count;
    Block *blocks;
    int block_count, capacity, failed;
} Graph;

static Scalar unknown(void) { Scalar v = {0}; return v; }
static int scalar_type(Type ty)
{
    return ty == TY_BOOL || ty == TY_FLOAT || ty == TY_DOUBLE ||
        (type_is_integer(ty) && !type_is_wide(ty));
}
static uint64_t normalize(uint64_t bits, Type ty)
{
    int width;
    uint64_t mask, sign;
    if (ty == TY_BOOL) return bits != 0;
    if (!type_is_integer(ty)) return bits;
    width = integer_width(ty);
    if (width == 64) return bits;
    mask = (UINT64_C(1) << width) - 1;
    sign = UINT64_C(1) << (width - 1);
    bits &= mask;
    return !integer_unsigned(ty) && (bits & sign) ? bits | ~mask : bits;
}
static Scalar integer_value(Type ty, uint64_t bits)
{
    Scalar v = {0}; v.ty = ty; v.bits = normalize(bits, ty); v.known = 1; return v;
}
static Scalar real_value(Type ty, double value)
{
    Scalar v = {0};
    v.ty = ty; v.known = 1;
    if (ty == TY_FLOAT) { float f = (float)value; uint32_t bits; memcpy(&bits, &f, 4); v.bits = bits; }
    else memcpy(&v.bits, &value, 8);
    return v;
}
static double real_number(Scalar v)
{
    double d;
    if (v.ty == TY_FLOAT) { float f; uint32_t bits = (uint32_t)v.bits; memcpy(&f, &bits, 4); return f; }
    memcpy(&d, &v.bits, 8); return d;
}
static int equal(Scalar a, Scalar b)
{
    return a.known == b.known && (!a.known || (a.ty == b.ty && a.bits == b.bits));
}
static int variable(Graph *g, Var *v)
{
    int i;
    for (i = 0; i < g->count; i++) if (g->vars[i] == v) return i;
    return -1;
}
static Scalar convert(Scalar v, Type ty)
{
    int width;
    uint64_t maximum;
    long long signed_value;
    if (!v.known || !scalar_type(ty)) return unknown();
    if (v.ty == ty) return v;
    if (type_is_integer(v.ty) && type_is_integer(ty)) {
        signed_value = integer_bits(v.bits);
        width = integer_width(ty);
        maximum = width == 64 ? UINT64_MAX : (UINT64_C(1) << width) - 1;
        if (integer_unsigned(ty)) {
            if ((!integer_unsigned(v.ty) && signed_value < 0) || v.bits > maximum) return unknown();
        } else {
            maximum >>= 1;
            if (integer_unsigned(v.ty)) { if (v.bits > maximum) return unknown(); }
            else if (signed_value > (long long)maximum || signed_value < -(long long)maximum - 1) return unknown();
        }
        return integer_value(ty, v.bits);
    }
    if ((ty == TY_FLOAT || ty == TY_DOUBLE) && (v.ty == TY_FLOAT || v.ty == TY_DOUBLE))
        return real_value(ty, real_number(v));
    /* Narrow integers are exact in binary64 before binary32 rounding. */
    if ((ty == TY_FLOAT || ty == TY_DOUBLE) && type_is_integer(v.ty) && integer_width(v.ty) <= 32)
        return real_value(ty, integer_unsigned(v.ty) ? (double)v.bits : (double)integer_bits(v.bits));
    return unknown();
}

static Scalar evaluate(Graph *g, const Node *n, const Scalar *state, int level);
static Scalar compare(Graph *g, const Node *n, const Scalar *state, int level)
{
    Scalar l = convert(evaluate(g, n->lhs, state, level + 1), n->cmp_ty);
    Scalar r = convert(evaluate(g, n->rhs, state, level + 1), n->cmp_ty);
    int result = 0;
    if (!l.known || !r.known) return unknown();
#define COMPARE(a,b) do { switch ((CmpKind)n->val) { \
    case CMP_EQ: result = (a) == (b); break; case CMP_NE: result = (a) != (b); break; \
    case CMP_LT: result = (a) < (b); break; case CMP_LE: result = (a) <= (b); break; \
    case CMP_GT: result = (a) > (b); break; case CMP_GE: result = (a) >= (b); break; } } while (0)
    if (l.ty == TY_FLOAT || l.ty == TY_DOUBLE) { double a = real_number(l), b = real_number(r); if (!isfinite(a) || !isfinite(b)) return unknown(); COMPARE(a,b); }
    else if (integer_unsigned(l.ty) || l.ty == TY_BOOL) { COMPARE(l.bits,r.bits); }
    else { long long a = integer_bits(l.bits), b = integer_bits(r.bits); COMPARE(a,b); }
#undef COMPARE
    return integer_value(TY_BOOL, (uint64_t)result);
}
static Scalar evaluate(Graph *g, const Node *n, const Scalar *state, int level)
{
    Scalar l, r;
    uint64_t bits;
    long long a, b;
    int slot;
    const Node *part;
    if (!n || level > 128 || !scalar_type(n->ty)) return unknown();
    switch (n->kind) {
    case ND_NUM: case ND_BOOL: return integer_value(n->ty, (uint64_t)n->val);
    case ND_FNUM: return real_value(n->ty, n->fval);
    case ND_VAR:
        if (n->var->is_const) return evaluate(g, n->var->const_val, state, level + 1);
        slot = variable(g, n->var);
        return slot >= 0 && g->eligible[slot] && state ? state[slot] : unknown();
    case ND_CAST: return convert(evaluate(g, n->lhs, state, level + 1), n->ty);
    case ND_CHAIN:
        for (part = n->lhs; part; part = part->next) {
            l = compare(g, part, state, level + 1);
            if (!l.known || !l.bits) return l;
        }
        return integer_value(TY_BOOL, 1);
    case ND_AND: case ND_OR:
        l = evaluate(g, n->lhs, state, level + 1);
        if (!l.known) return unknown();
        if ((n->kind == ND_AND && !l.bits) || (n->kind == ND_OR && l.bits)) return l;
        return evaluate(g, n->rhs, state, level + 1);
    case ND_NOT:
        l = evaluate(g, n->lhs, state, level + 1);
        return l.known ? integer_value(TY_BOOL, !l.bits) : unknown();
    case ND_NEG: case ND_ABS: case ND_BIT_NOT:
        l = evaluate(g, n->lhs, state, level + 1);
        if (!l.known) return unknown();
        if (type_is_integer(n->ty)) {
            bits = n->kind == ND_BIT_NOT ? ~l.bits : n->kind == ND_NEG || (!integer_unsigned(n->ty) && integer_bits(l.bits) < 0) ? 0 - l.bits : l.bits;
            return integer_value(n->ty, bits);
        }
        if (!isfinite(real_number(l))) return unknown();
        return real_value(n->ty, n->kind == ND_NEG ? -real_number(l) : fabs(real_number(l)));
    case ND_ADD: case ND_SUB: case ND_MUL: case ND_DIV: case ND_MOD:
    case ND_BIT_AND: case ND_BIT_OR: case ND_BIT_XOR: case ND_XOR: case ND_SHL: case ND_SHR:
        l = evaluate(g, n->lhs, state, level + 1); r = evaluate(g, n->rhs, state, level + 1);
        if (!l.known || !r.known) return unknown();
        if (type_is_integer(n->ty) || n->kind == ND_XOR) {
            a = integer_bits(l.bits); b = integer_bits(r.bits);
            switch (n->kind) {
            case ND_ADD: bits = l.bits + r.bits; break;
            case ND_SUB: bits = l.bits - r.bits; break;
            case ND_MUL: bits = l.bits * r.bits; break;
            case ND_BIT_AND: bits = l.bits & r.bits; break;
            case ND_BIT_OR: bits = l.bits | r.bits; break;
            case ND_BIT_XOR: case ND_XOR: bits = l.bits ^ r.bits; break;
            case ND_SHL: case ND_SHR:
                if (r.bits >= (unsigned)integer_width(n->ty)) return unknown();
                bits = n->kind == ND_SHL ? l.bits << r.bits : l.bits >> r.bits;
                if (n->kind == ND_SHR && r.bits && !integer_unsigned(n->ty) && a < 0) bits |= UINT64_MAX << (64 - r.bits);
                break;
            default:
                if (!r.bits) return unknown();
                if (!integer_unsigned(n->ty) && a == integer_bits(UINT64_MAX << (integer_width(n->ty) - 1)) && b == -1) {
                    if (n->kind != ND_MOD) return unknown();
                    bits = 0;
                } else if (integer_unsigned(n->ty)) bits = n->kind == ND_MOD ? l.bits % r.bits : l.bits / r.bits;
                else bits = (uint64_t)(n->kind == ND_MOD ? a % b : a / b);
                break;
            }
            return integer_value(n->ty, bits);
        }
        if (!isfinite(real_number(l)) || !isfinite(real_number(r))) return unknown();
        if (n->kind == ND_DIV && real_number(r) == 0) return unknown();
        if (n->ty == TY_FLOAT) {
            volatile float x = (float)real_number(l), y = (float)real_number(r), z;
            switch (n->kind) { case ND_ADD: z=x+y; break; case ND_SUB: z=x-y; break; case ND_MUL: z=x*y; break; case ND_DIV: z=x/y; break; default: return unknown(); }
            return isfinite(z) ? real_value(n->ty, z) : unknown();
        } else {
            volatile double x = real_number(l), y = real_number(r), z;
            switch (n->kind) { case ND_ADD: z=x+y; break; case ND_SUB: z=x-y; break; case ND_MUL: z=x*y; break; case ND_DIV: z=x/y; break; default: return unknown(); }
            return isfinite(z) ? real_value(n->ty, z) : unknown();
        }
    default: return unknown();
    }
}

/* All source aliases are excluded before propagation, including aliases in
 * handlers and unreachable source. Input targets are writes, not reads. */
static void escape(Graph *g, Node *n)
{
    InputField *f;
    int i;
    for (; n; n = n->next) {
        if ((n->kind == ND_ADDR || n->kind == ND_REFARG) && n->lhs) {
            Node *root = n->lhs;
            while (root->kind == ND_INDEX) root = root->lhs;
            i = variable(g, root->var); if (i >= 0) g->eligible[i] = 0;
        }
        if (n->kind == ND_INPUT) for (f = n->fields; f; f = f->next) {
            Node *root = f->var; while (root->kind == ND_INDEX) root = root->lhs;
            i = variable(g, root->var); if (i >= 0) g->eligible[i] = 0;
        }
        escape(g,n->lhs); escape(g,n->rhs); escape(g,n->cond); escape(g,n->then); escape(g,n->els);
        escape(g,n->init); escape(g,n->step); escape(g,n->inc); escape(g,n->for_amount); escape(g,n->end);
        for (f=n->fields;f;f=f->next) escape(g,f->var);
    }
}
static int block(Graph *g, Node *source, Node *condition, int next, int other)
{
    Block *b;
    if (g->block_count >= OPT_MAX_BLOCKS) { g->failed = 1; return -1; }
    if (g->block_count == g->capacity) {
        int size = g->capacity ? g->capacity * 2 : 32;
        Block *grown = realloc(g->blocks, (size_t)size * sizeof(*grown));
        if (!grown) fatal("最適化の制御フローを確保できません");
        g->blocks = grown; g->capacity = size;
    }
    b = &g->blocks[g->block_count]; memset(b,0,sizeof(*b));
    b->source=source; b->condition=condition; b->next[0]=next; b->next[1]=other;
    return g->block_count++;
}
static int build(Graph *g, Node *list, int next, int stop, int again)
{
    Node *n;
    Node **items;
    int count=0,i,id,a,b,header,update;
    for(n=list;n;n=n->next) { if(++count>OPT_MAX_BLOCKS){g->failed=1;return -1;} }
    items=malloc((size_t)(count?count:1)*sizeof(*items));if(!items)fatal("最適化の文を確保できません");
    for(n=list,i=0;n;n=n->next)items[i++]=n;
    for(i=count-1;i>=0&&!g->failed;i--){
        n=items[i];
        switch(n->kind){
        case ND_IF:
            a=build(g,n->then,next,stop,again);b=build(g,n->els,next,stop,again);
            next=block(g,n,n->cond,a,b);break;
        case ND_WHILE:
            header=block(g,n,n->cond,-1,next);
            if(header<0)break;
            a=build(g,n->then,header,next,header);g->blocks[header].next[0]=a;next=header;break;
        case ND_FOR:
            header=block(g,n,n->cond,-1,next);
            if(header<0)break;
            update=block(g,NULL,NULL,header,next);if(update<0)break;
            g->blocks[update].kill=n->init->var;
            a=build(g,n->then,update,next,update);g->blocks[header].next[0]=a;
            id=block(g,n->init,NULL,header,-1);if(id>=0)g->blocks[id].kill=n->init->var;next=id;break;
        case ND_BREAK: next=block(g,n,NULL,stop,-1);break;
        case ND_CONTINUE: next=block(g,n,NULL,again,-1);break;
        case ND_RETURN: case ND_THROW: next=block(g,n,NULL,-1,-1);break;
        default:
            next=block(g,n,NULL,next,-1);
            if(next>=0&&(n->kind==ND_TRY||n->kind==ND_MATCH))g->blocks[next].barrier=1;
            break;
        }
    }
    free(items);return next;
}
static void transfer(Graph *g, Block *b, Scalar *state)
{
    Node *n=b->source;
    int i;
    if(b->barrier)memset(state,0,(size_t)g->count*sizeof(*state));
    else if(n&&(n->kind==ND_DECL||n->kind==ND_ASSIGN)){
        i=variable(g,n->var);
        if(i>=0&&g->eligible[i])state[i]=n->lhs?evaluate(g,n->lhs,state,0):
            n->kind==ND_DECL?(n->var->ty==TY_FLOAT||n->var->ty==TY_DOUBLE?real_value(n->var->ty,0):integer_value(n->var->ty,0)):unknown();
    }
    if(b->kill){i=variable(g,b->kill);if(i>=0)state[i]=unknown();}
}
static void materialize(Node *n, Scalar v)
{
    /* Preserve source identity, linked-list membership and interpolation tags. */
    n->kind=v.ty==TY_BOOL?ND_BOOL:v.ty==TY_FLOAT||v.ty==TY_DOUBLE?ND_FNUM:ND_NUM;
    n->val=integer_bits(v.bits);n->fval=(v.ty==TY_FLOAT||v.ty==TY_DOUBLE)?real_number(v):0;
    n->lhs=n->rhs=n->cond=n->then=n->els=n->init=n->step=n->inc=n->for_amount=n->end=NULL;
    n->fields=NULL;
}
static void rewrite(Graph *g,Node *n,const Scalar *state);
static void rewrite_place(Graph *g,Node *n,const Scalar *state)
{
    if(n&&n->kind==ND_INDEX){rewrite_place(g,n->lhs,state);rewrite(g,n->rhs,state);}
}
static void rewrite_list(Graph *g,Node *n,const Scalar *state)
{for(;n;n=n->next)rewrite(g,n,state);}
static void rewrite(Graph *g,Node *n,const Scalar *state)
{
    Scalar v;
    InputField *f;
    if(!n)return;
    v=evaluate(g,n,state,0);
    if(v.known){materialize(n,v);return;}
    if(n->kind==ND_ADDR)return;
    if(n->kind==ND_REFARG){rewrite(g,n->rhs,state);rewrite_place(g,n->lhs,state);return;}
    if(n->kind==ND_STORE||n->kind==ND_METHOD){rewrite_place(g,n->lhs,state);}
    else rewrite_list(g,n->lhs,state);
    rewrite_list(g,n->rhs,state);rewrite_list(g,n->cond,state);
    for(f=n->fields;f;f=f->next)rewrite_place(g,f->var,state);
}
static void optimize_region(Graph *g,Node *list);
static void rewrite_block(Graph *g,Block *b)
{
    Node *n=b->source,*arm;
    if(!n)return;
    if(b->condition){rewrite(g,b->condition,b->in);return;}
    if(n->kind==ND_TRY){
        optimize_region(g,n->lhs);optimize_region(g,n->then);
        for(arm=n->els;arm;arm=arm->next)optimize_region(g,arm->then);
        optimize_region(g,n->rhs);return;
    }
    if(n->kind==ND_MATCH){rewrite(g,n->lhs,b->in);for(arm=n->then;arm;arm=arm->next)optimize_region(g,arm->then);return;}
    if(n->kind==ND_DECL||n->kind==ND_ASSIGN)rewrite(g,n->lhs,b->in);
    else if(n->kind==ND_PRINT){rewrite_list(g,n->lhs,b->in);rewrite_list(g,n->end,b->in);}
    else rewrite(g,n,b->in);
}
static void optimize_region(Graph *context,Node *list)
{
    Graph graph=*context,*g=&graph;
    Scalar *storage,*temporary;
    size_t cells;
    int entry,i,j,k,changed=1,iterations=0,limit;
    g->blocks=NULL;g->block_count=g->capacity=g->failed=0;
    entry=build(g,list,-1,-1,-1);
    cells=(size_t)g->block_count*(size_t)(g->count?g->count:1);
    if(g->failed||entry<0||cells>OPT_MAX_CELLS){free(g->blocks);return;}
    storage=calloc(cells*2,sizeof(*storage));temporary=calloc((size_t)(g->count?g->count:1),sizeof(*temporary));
    if(!storage||!temporary)fatal("最適化の値を確保できません");
    for(i=0;i<g->block_count;i++){g->blocks[i].in=storage+(size_t)i*g->count;g->blocks[i].out=storage+cells+(size_t)i*g->count;}
    g->blocks[entry].reached=1;g->blocks[entry].dirty=1;
    limit=g->block_count*4+16;
    while(changed&&iterations++<limit){
        changed=0;
        for(i=g->block_count-1;i>=0;i--){
            Block *b=&g->blocks[i];
            if(!b->reached||!b->dirty)continue;
            b->dirty=0;memcpy(temporary,b->in,(size_t)g->count*sizeof(*temporary));transfer(g,b,temporary);
            memcpy(b->out,temporary,(size_t)g->count*sizeof(*temporary));
            /* Both branch edges participate. No speculative path facts. */
            for(k=0;k<2;k++)if(b->next[k]>=0){
                Block *target=&g->blocks[b->next[k]];
                if(!target->reached){memcpy(target->in,temporary,(size_t)g->count*sizeof(*temporary));target->reached=target->dirty=changed=1;}
                else for(j=0;j<g->count;j++)if(target->in[j].known&&!equal(target->in[j],temporary[j])){
                    target->in[j]=unknown();target->dirty=changed=1;
                }
            }
        }
    }
    /* An unfinished fixed point is never used to rewrite loop values. */
    if(!changed)for(i=0;i<g->block_count;i++)if(g->blocks[i].reached)rewrite_block(g,&g->blocks[i]);
    free(temporary);free(storage);free(g->blocks);
}
static void remaining_reads(Graph *g,Node *n,int *read)
{
    InputField *f;
    int i;
    for(;n;n=n->next){
        if(n->kind==ND_VAR){i=variable(g,n->var);if(i>=0)read[i]=1;}
        remaining_reads(g,n->lhs,read);remaining_reads(g,n->rhs,read);
        remaining_reads(g,n->cond,read);remaining_reads(g,n->then,read);remaining_reads(g,n->els,read);
        remaining_reads(g,n->init,read);remaining_reads(g,n->step,read);remaining_reads(g,n->inc,read);
        remaining_reads(g,n->for_amount,read);remaining_reads(g,n->end,read);
        for(f=n->fields;f;f=f->next)remaining_reads(g,f->var,read);
    }
}
static void dead_stores(Graph *g,Node *n,const int *read)
{
    InputField *f;
    int i;
    for(;n;n=n->next){
        if(n->kind==ND_DECL||n->kind==ND_ASSIGN){
            i=variable(g,n->var);
            if(i>=0&&g->eligible[i]&&!read[i]&&(!n->lhs||n->lhs->kind==ND_NUM||n->lhs->kind==ND_BOOL||n->lhs->kind==ND_FNUM))n->opt_dead_store=1;
        }
        dead_stores(g,n->lhs,read);dead_stores(g,n->rhs,read);dead_stores(g,n->cond,read);
        dead_stores(g,n->then,read);dead_stores(g,n->els,read);dead_stores(g,n->init,read);
        dead_stores(g,n->step,read);dead_stores(g,n->inc,read);dead_stores(g,n->for_amount,read);dead_stores(g,n->end,read);
        for(f=n->fields;f;f=f->next)dead_stores(g,f->var,read);
    }
}
void optimize_program(Ast *ast)
{
    Function *fn;
    int changed;
    for(fn=ast->funcs;fn;fn=fn->next)if(!fn->is_template){
        Graph g={0};Var *v;
        for(v=fn->params;v&&g.count<OPT_MAX_VARS;v=v->next)if(scalar_type(v->ty)){
            g.vars[g.count]=v;g.eligible[g.count++]=!v->is_ref;
        }
        for(v=fn->locals;v&&g.count<OPT_MAX_VARS;v=v->next)if(scalar_type(v->ty)&&!v->is_const){
            g.vars[g.count]=v;g.eligible[g.count++]=!v->is_ref;
        }
        escape(&g,fn->body);optimize_region(&g,fn->body);
        {int read[OPT_MAX_VARS]={0};remaining_reads(&g,fn->body,read);dead_stores(&g,fn->body,read);}
        fn->opt_ready=1;fn->opt_effects=0;
    }
    do{
        changed=0;
        for(fn=ast->funcs;fn;fn=fn->next)if(!fn->is_template){
            unsigned effects=optimizer_effects(fn->body);
            if((effects|fn->opt_effects)!=fn->opt_effects){fn->opt_effects|=effects;changed=1;}
        }
    }while(changed);
    optimizer_inline(ast);
    optimizer_memory(ast);
    optimizer_ranges(ast);
    optimizer_machine(ast);
    optimizer_reachable(ast);
}
