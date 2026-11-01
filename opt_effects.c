#include "optimizer.h"

static unsigned function_effects(const Function *fn)
{
    return fn && fn->opt_ready ? fn->opt_effects : OPT_ALL;
}

static unsigned copy_effects(Type ty, Type *path, int level, int *budget)
{
    unsigned effects = 0;
    ModelField *field;
    int i;
    if (!type_is_array(ty) && !type_is_model(ty)) return 0;
    if (level == 64 || --*budget < 0) return OPT_ALL;
    for (i = 0; i < level; i++) if (path[i] == ty) return OPT_ALL;
    path[level] = ty;
    effects = OPT_READ | OPT_ALLOCATE | OPT_TRAP | OPT_CONTEXT;
    if (type_is_array(ty)) {
        Type base = array_base(ty);
        effects |= copy_effects(array_element(ty), path, level + 1, budget);
        /* Copying to a constrained shape can construct missing elements. */
        if (type_is_model(base)) effects |= function_effects(type_model(base)->default_builder);
    } else {
        for (field = type_model(ty)->fields; field; field = field->next)
            effects |= copy_effects(field->ty, path, level + 1, budget);
    }
    return effects;
}

unsigned optimizer_copy_effects(Type ty)
{
    Type path[64];
    int budget = 1024;
    return copy_effects(ty, path, 0, &budget);
}

unsigned optimizer_local_effects(const Node *n)
{
    if (!n) return 0;
    switch (n->kind) {
    case ND_NUM: case ND_FNUM: case ND_BOOL: case ND_STRLIT:
    case ND_STR: case ND_UNIT: case ND_WIDE: case ND_NONE:
    case ND_CMP: case ND_AND: case ND_OR:
    case ND_XOR: case ND_BIT_AND: case ND_BIT_OR: case ND_BIT_XOR:
    case ND_CREAL: case ND_CIMAG:
    case ND_IF: case ND_WHILE: case ND_FOR: case ND_MATCH: case ND_CASE:
    case ND_TRY: case ND_USING: case ND_RETURN: case ND_EXPR_STMT:
    case ND_BREAK: case ND_CONTINUE: case ND_BUILD_CONTEXT:
        return 0;
    case ND_NOT: case ND_NEG: case ND_ABS: case ND_BIT_NOT:
        return type_is_wide(n->ty) || type_is_complex(n->ty) ? OPT_ALLOCATE | OPT_TRAP | OPT_CONTEXT : 0;
    case ND_CHAIN: {
        const Node *cmp;
        unsigned effects = 0;
        if (n->lhs) effects |= optimizer_copy_effects(n->lhs->lhs->ty);
        for (cmp = n->lhs; cmp; cmp = cmp->next)
            if (cmp->next) effects |= optimizer_copy_effects(cmp->rhs->ty);
        return effects;
    }
    case ND_VAR:
        return OPT_READ | (n->var && n->var->is_ref ? OPT_TRAP : 0);
    case ND_SAVED: return OPT_READ;
    case ND_ADD: case ND_SUB: case ND_MUL:
        return type_is_text(n->ty) || type_is_complex(n->ty) || type_is_wide(n->ty)
            ? OPT_ALLOCATE | OPT_TRAP : 0;
    case ND_DIV: case ND_MOD: case ND_POW: case ND_SHL: case ND_SHR:
    case ND_CAST: case ND_ENUM_CONVERT: case ND_INT_PARSE: case ND_INT_FORMAT:
        return OPT_TRAP | ((type_is_text(n->ty) || type_is_wide(n->ty) || type_is_complex(n->ty)) ? OPT_ALLOCATE : 0);
    case ND_COMPLEX: return OPT_ALLOCATE | OPT_TRAP;
    case ND_INDEX: case ND_LENGTH:
        return OPT_READ | OPT_TRAP | (n->kind == ND_INDEX && n->lhs && type_is_text(n->lhs->ty) ? OPT_ALLOCATE : 0);
    case ND_CALL: case ND_CONSTRUCT:
        return function_effects(n->func);
    case ND_MODEL_NEW:
        return function_effects(type_model(n->ty)->default_builder);
    case ND_MODEL_INIT: case ND_MODEL_SEED:
        return OPT_ALLOCATE | OPT_TRAP | optimizer_copy_effects(n->ty);
    case ND_MODEL_COPY: case ND_ARRAY_NEW: case ND_ARRAY_COPY: case ND_ARRAY_LITERAL:
        return optimizer_copy_effects(n->ty);
    case ND_METHOD:
        return OPT_READ | OPT_WRITE | OPT_TRAP | OPT_ALLOCATE | OPT_CONTEXT |
            (n->val == AM_APPEND ? optimizer_copy_effects(n->lhs->ty) : 0);
    case ND_DECL:
        return OPT_WRITE;
    case ND_ASSIGN:
        return OPT_WRITE | (n->var ? optimizer_copy_effects(n->var->ty) : OPT_ALL);
    case ND_STORE:
        return OPT_WRITE | OPT_READ | OPT_TRAP | OPT_CONTEXT |
            (n->lhs ? optimizer_copy_effects(n->lhs->ty) : OPT_ALL);
    case ND_ADDR: return OPT_ESCAPE | OPT_TRAP;
    case ND_ADDR_ASSIGN: return OPT_ESCAPE | OPT_WRITE | OPT_TRAP;
    case ND_REFARG: return OPT_BORROW | OPT_ESCAPE | OPT_TRAP | OPT_CONTEXT;
    case ND_INPUT: return OPT_EXTERNAL | OPT_WRITE | OPT_ALLOCATE | OPT_TRAP;
    case ND_PRINT: return OPT_EXTERNAL | OPT_CONTEXT;
    case ND_FORMAT: return OPT_ALLOCATE | OPT_TRAP | OPT_CONTEXT;
    case ND_TEXT_METHOD: return OPT_READ | OPT_ALLOCATE | OPT_TRAP | OPT_CONTEXT;
    case ND_BUILTIN:
        return OPT_EXTERNAL | OPT_READ | OPT_WRITE | OPT_ALLOCATE | OPT_TRAP | OPT_CONTEXT |
            (builtin_node_may_raise((Node *)n) ? OPT_RAISE : 0) |
            (n->val >= BI_FILE_OPEN ? OPT_RESOURCE : 0);
    case ND_THROW: return OPT_RAISE;
    case ND_BUILD_REJECT: return OPT_TRAP;
    default: return OPT_ALL;
    }
}

unsigned optimizer_effects(const Node *n)
{
    unsigned effects = 0;
    InputField *field;
    for (; n; n = n->next) {
        effects |= optimizer_local_effects(n);
        if (n->kind == ND_MODEL_INIT || n->kind == ND_MODEL_SEED || n->kind == ND_CONSTRUCT) {
            const Node *item;
            for (item = n->lhs; item; item = item->next) effects |= optimizer_effects(item->lhs);
            continue; /* Named initializer items are containers, not field reads. */
        }
        if (n->kind == ND_VAR && n->var && n->var->is_const)
            effects |= optimizer_effects(n->var->const_val);
        effects |= optimizer_effects(n->lhs) | optimizer_effects(n->rhs);
        effects |= optimizer_effects(n->cond) | optimizer_effects(n->then) | optimizer_effects(n->els);
        effects |= optimizer_effects(n->init) | optimizer_effects(n->step);
        effects |= optimizer_effects(n->inc) | optimizer_effects(n->for_amount) | optimizer_effects(n->end);
        for (field = n->fields; field; field = field->next)
            effects |= optimizer_effects(field->var);
    }
    return effects;
}

int optimizer_integer_canonical(const Node *n)
{
    switch (n->kind) {
    case ND_NUM: case ND_BOOL: case ND_VAR: case ND_SAVED: case ND_INDEX:
    case ND_LENGTH: case ND_CALL:
        return 1;
    default: return 0;
    }
}
