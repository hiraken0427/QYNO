/* Bounded leaf expansion. Arguments keep the ordinary evaluation/borrow path;
 * only the callee frame and control transfer are replaced. */
#include "optimizer.h"
#include <limits.h>
#include <stdlib.h>

#define INLINE_COST 32
#define INLINE_VARS 64
#define INLINE_GROWTH 256
#define INLINE_FRAME 2048

typedef struct VarCopy VarCopy;
typedef struct NodeCopy NodeCopy;
struct VarCopy { VarCopy *next; Var *old, *copy; };
struct NodeCopy { NodeCopy *next; Node *old, *copy; };
typedef struct {
    Function *caller;
    VarCopy *vars;
    NodeCopy *nodes;
    int offset;
} Clone;

static void *allocate(size_t size)
{
    void *p = calloc(1, size);
    if (!p) fatal("インライン展開の領域を確保できません");
    return p;
}
static int value_type(Type ty)
{
    return ty != TY_NONE && ty != TY_FILE && ty != TY_ADDR &&
        !type_is_array(ty) && !type_is_model(ty) && !type_is_interface(ty);
}
static int leaf(Node *n, int *cost)
{
    for (; n; n = n->next) {
        if (++*cost > INLINE_COST) return 0;
        switch (n->kind) {
        case ND_NUM: case ND_FNUM: case ND_BOOL: case ND_STRLIT: case ND_WIDE: case ND_UNIT:
        case ND_VAR: case ND_SAVED: case ND_ADD: case ND_SUB: case ND_MUL: case ND_DIV:
        case ND_MOD: case ND_POW: case ND_NEG: case ND_ABS: case ND_NOT:
        case ND_AND: case ND_OR: case ND_XOR: case ND_BIT_AND: case ND_BIT_OR:
        case ND_BIT_XOR: case ND_BIT_NOT: case ND_SHL: case ND_SHR:
        case ND_CMP: case ND_CHAIN: case ND_CREAL: case ND_CIMAG:
        case ND_CAST: case ND_ENUM_CONVERT: case ND_INDEX: case ND_LENGTH:
        case ND_INT_PARSE: case ND_INT_FORMAT: case ND_IF: case ND_RETURN:
        case ND_USING: case ND_EXPR_STMT:
            break;
        case ND_DECL:
            if (!n->var || n->var->is_ref || !value_type(n->var->ty)) return 0;
            break;
        case ND_ASSIGN:
            if (!n->var || !value_type(n->var->ty)) return 0;
            break;
        case ND_STORE:
            if (!n->lhs || !value_type(n->lhs->ty)) return 0;
            break;
        default: return 0;
        }
        if (!leaf(n->lhs, cost) || !leaf(n->rhs, cost) || !leaf(n->cond, cost) ||
            !leaf(n->then, cost) || !leaf(n->els, cost) || !leaf(n->init, cost) ||
            !leaf(n->step, cost) || !leaf(n->inc, cost) || !leaf(n->for_amount, cost) ||
            !leaf(n->end, cost)) return 0;
        if (n->fields) return 0;
    }
    return 1;
}
static int candidate(Function *fn)
{
    Var *v;
    int count = 0, cost = 0;
    unsigned forbidden = OPT_RAISE | OPT_ALLOCATE | OPT_EXTERNAL | OPT_RESOURCE | OPT_ESCAPE;
    if (fn->is_template || fn->is_internal || fn->is_initializer || !fn->opt_ready ||
        !value_type(fn->ret) || (fn->opt_effects & forbidden)) return 0;
    for (v = fn->params; v; v = v->next) {
        if (++count > INLINE_VARS) return 0;
        if (!value_type(v->ty) && !(v->is_ref && (type_is_array(v->ty) || type_is_model(v->ty)))) return 0;
    }
    for (v = fn->locals; v; v = v->next)
        if (++count > INLINE_VARS || !value_type(v->ty) || v->is_ref) return 0;
    return leaf(fn->body, &cost) ? (cost ? cost : 1) : 0;
}
static Var *lookup(Clone *clone, Var *old)
{
    VarCopy *p;
    for (p = clone->vars; p; p = p->next) if (p->old == old) return p->copy;
    return old; /* Global constants keep their typed, immutable value. */
}
static Var *copy_var(Clone *clone, Var *old, int parameter)
{
    VarCopy *entry = allocate(sizeof(*entry));
    Var *v = allocate(sizeof(*v));
    *v = *old;
    v->is_param = 0;
    v->opt_valid_ref = parameter && old->is_ref;
    v->vnext = NULL;
    if (v->is_const) v->offset = 0;
    else { clone->offset += 8; v->offset = clone->offset; }
    v->next = clone->caller->locals;
    clone->caller->locals = v;
    entry->old = old; entry->copy = v; entry->next = clone->vars; clone->vars = entry;
    return v;
}
static Node *copy_node(Clone *clone, Node *old)
{
    NodeCopy *entry;
    Node *n;
    if (!old) return NULL;
    for (entry = clone->nodes; entry; entry = entry->next) if (entry->old == old) return entry->copy;
    entry = allocate(sizeof(*entry)); n = allocate(sizeof(*n)); *n = *old;
    entry->old = old; entry->copy = n; entry->next = clone->nodes; clone->nodes = entry;
    n->var = lookup(clone, old->var); n->ref_temp = lookup(clone, old->ref_temp);
#define COPY(member) n->member = copy_node(clone, old->member)
    COPY(next); COPY(lhs); COPY(rhs); COPY(cond); COPY(then); COPY(els);
    COPY(init); COPY(step); COPY(inc); COPY(for_amount); COPY(end);
#undef COPY
    return n;
}
static OptInline *plan(Function *caller, Node *call)
{
    Function *fn = call->func;
    OptInline *result = allocate(sizeof(*result));
    Clone clone = {0};
    Var *v;
    int i = 0;
    clone.caller = caller; clone.offset = caller->stack_size;
    result->owner = caller;
    result->param_count = fn->param_count;
    result->params = allocate((size_t)(fn->param_count ? fn->param_count : 1) * sizeof(*result->params));
    for (v = fn->params; v; v = v->next) result->params[i++] = copy_var(&clone, v, 1);
    for (v = fn->locals; v; v = v->next) copy_var(&clone, v, 0);
    result->body = copy_node(&clone, fn->body);
    result->roots = allocate(INLINE_VARS * sizeof(*result->roots));
    { VarCopy *entry;
      for (entry = clone.vars; entry; entry = entry->next) {
          v = entry->copy;
          if (!v->is_const && (v->is_ref || type_is_text(v->ty) || type_is_complex(v->ty) ||
              v->ty == TY_I128 || v->ty == TY_U128 || v->ty == TY_QUAD))
              result->roots[result->root_count++] = v->offset;
      }
    }
    caller->stack_size = (clone.offset + 15) / 16 * 16;
    while (clone.vars) { VarCopy *p = clone.vars; clone.vars = p->next; free(p); }
    while (clone.nodes) { NodeCopy *p = clone.nodes; clone.nodes = p->next; free(p); }
    return result;
}
static void expand(Function *caller, Node *n, int *budget, int base)
{
    InputField *f;
    for (; n; n = n->next) {
        if (n->kind == ND_CALL && !n->opt_inline && n->func && n->func != caller && n->func->opt_inline_cost &&
            n->nargs == n->func->param_count && n->func->opt_inline_cost <= *budget) {
            Var *v;
            int slots = n->func->param_count;
            for (v = n->func->locals; v; v = v->next) if (!v->is_const) slots++;
            if (caller->stack_size <= INT_MAX - slots * 8 - 15 &&
                caller->stack_size - base + slots * 8 + 15 <= INLINE_FRAME) {
                n->opt_inline = plan(caller, n);
                *budget -= n->func->opt_inline_cost;
            }
        }
        expand(caller,n->lhs,budget,base); expand(caller,n->rhs,budget,base);
        expand(caller,n->cond,budget,base);
        if (n->kind == ND_IF && n->cond->kind == ND_BOOL)
            expand(caller,n->cond->val?n->then:n->els,budget,base);
        else {
            if (!(n->kind == ND_WHILE && n->cond->kind == ND_BOOL && !n->cond->val)) expand(caller,n->then,budget,base);
            expand(caller,n->els,budget,base);
        }
        expand(caller,n->init,budget,base); expand(caller,n->step,budget,base);
        expand(caller,n->inc,budget,base); expand(caller,n->for_amount,budget,base); expand(caller,n->end,budget,base);
        for(f=n->fields;f;f=f->next)expand(caller,f->var,budget,base);
        if (n->kind == ND_RETURN || n->kind == ND_THROW || n->kind == ND_BREAK || n->kind == ND_CONTINUE) break;
    }
}
void optimizer_inline(Ast *ast)
{
    Function *fn;
    for (fn = ast->funcs; fn; fn = fn->next) fn->opt_inline_cost = candidate(fn);
    for (fn = ast->funcs; fn; fn = fn->next) if (!fn->is_template) {
        int budget = INLINE_GROWTH, base = fn->stack_size;
        expand(fn, fn->body, &budget, base);
    }
}
