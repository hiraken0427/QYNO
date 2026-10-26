/* Definite explicit-close analysis. Handles have symbolic identities; aliases
 * share an identity, while file arrays keep sparse constant-index paths.
 * Summaries transfer returned/ref-output ownership and definite parameter closes.
 * Unknown dynamic paths are conservative: they never discharge an obligation.
 */
#define FC_MAX_RESOURCES 1024
#define FC_MAX_PATHS 128

typedef struct FcEntry FcEntry;
typedef struct FcMap FcMap;
typedef struct FcEnv FcEnv;
typedef struct FcState FcState;
typedef struct FcFlow FcFlow;
typedef struct FcInfo FcInfo;
typedef struct FcContext FcContext;
struct FcEntry { FcEntry *next; char *path; int id; };
struct FcMap { FcEntry *entries; int origin; char *prefix; long long length; };
struct FcEnv { FcEnv *next; Var *var; FcMap *value; Var *alias; };
struct FcState { unsigned char live[FC_MAX_RESOURCES], known[FC_MAX_RESOURCES]; FcEnv *env; };
struct FcFlow { FcFlow *next; FcState *state; int kind; FcMap *value; };
typedef struct { Node *site; int parameter; char *path; } FcResource;
struct FcContext { FcInfo *info; int count; FcResource resources[FC_MAX_RESOURCES]; };
struct FcInfo {
    FcInfo *next; Function *fn; int relevant, status; FcContext *context;
    FcFlow *exits;
};
static FcInfo *fc_infos;
static FcMap *fc_eval(FcContext *ctx, FcState *s, Node *n);
static FcFlow *fc_run(FcContext *ctx, FcState *s, Node *n);
static void fc_analyze(FcInfo *info);

static void *fc_new(size_t size)
{
    void *p = calloc(1, size);
    if (!p) fatal("close の解析領域を確保できません");
    return p;
}
static char *fc_join(const char *a, const char *b)
{
    char *p = fc_new(strlen(a) + strlen(b) + 1);
    strcpy(p, a); strcat(p, b); return p;
}
static FcMap *fc_empty(void)
{
    FcMap *m = fc_new(sizeof(*m)); m->length = -1; m->prefix = ""; return m;
}
static void fc_put(FcMap *m, const char *path, int id)
{
    FcEntry *e = fc_new(sizeof(*e)); e->path = fc_join(path, ""); e->id = id;
    e->next = m->entries; m->entries = e;
}
static FcMap *fc_scalar(int id)
{
    FcMap *m = fc_empty(); fc_put(m, "", id); return m;
}
static int fc_id(FcContext *ctx, Node *site, int parameter, const char *path)
{
    int i;
    if (parameter > 0) for (i = 1; i < ctx->count; i++)
        if (ctx->resources[i].parameter == parameter && strcmp(ctx->resources[i].path, path) == 0) return i;
    if (ctx->count >= FC_MAX_RESOURCES) error_tok(site ? site->tok : ctx->info->fn->tok, "close の解析が複雑すぎます。関数を分割してください");
    i = ctx->count++; ctx->resources[i].site = site;
    ctx->resources[i].parameter = parameter; ctx->resources[i].path = fc_join(path, ""); return i;
}
static int fc_alive(FcContext *ctx, FcState *s, int id)
{
    if (id <= 0) return 0;
    return s->known[id] ? s->live[id] : ctx->resources[id].parameter > 0;
}
static int fc_get(FcContext *ctx, FcMap *m, const char *path)
{
    FcEntry *e;
    for (e = m->entries; e; e = e->next) if (strcmp(e->path, path) == 0) return e->id;
    if (m->origin < 0) return -1;
    if (m->origin > 0) return fc_id(ctx, NULL, m->origin, fc_join(m->prefix, path));
    return 0;
}
static FcMap *fc_slice(FcMap *m, const char *path)
{
    FcMap *r = fc_empty(); FcEntry *e; size_t n = strlen(path);
    r->origin = m->origin; r->prefix = fc_join(m->prefix, path);
    for (e = m->entries; e; e = e->next)
        if (strncmp(e->path, path, n) == 0) fc_put(r, e->path + n, e->id);
    return r;
}
static FcEnv *fc_env(FcState *s, Var *v)
{
    FcEnv *e;
    for (e = s->env; e; e = e->next) if (e->var == v) return e;
    e = fc_new(sizeof(*e)); e->var = v; e->value = fc_empty(); e->next = s->env; s->env = e; return e;
}
/* .addr captures the current underlying slot, not a movable ref variable. */
static Var *fc_reference_target(FcState *s, Var *v)
{
    int limit = 0;
    while (fc_env(s, v)->alias) {
        if (++limit > 256) error_tok(v->tok, "ref の循環により close を確認できません");
        v = fc_env(s, v)->alias;
    }
    return v;
}
static FcMap *fc_value(FcState *s, Var *v)
{
    int limit = 0; FcEnv *e = fc_env(s, v);
    while (e->alias) {
        if (++limit > 256) error_tok(v->tok, "ref の循環により close を確認できません");
        e = fc_env(s, e->alias);
    }
    return e->value;
}
static void fc_set(FcState *s, Var *v, FcMap *m)
{
    int limit = 0; FcEnv *e = fc_env(s, v);
    while (e->alias) {
        if (++limit > 256) error_tok(v->tok, "ref の循環により close を確認できません");
        e = fc_env(s, e->alias);
    }
    e->value = m;
}
static FcState *fc_clone(FcState *s)
{
    FcState *r = fc_new(sizeof(*r)); FcEnv *e;
    memcpy(r, s, sizeof(*r)); r->env = NULL;
    for (e = s->env; e; e = e->next) {
        FcEnv *copy = fc_new(sizeof(*copy)); *copy = *e; copy->next = r->env; r->env = copy;
    }
    return r;
}
static int fc_file_type(Type ty) { return ty == TY_FILE || (type_is_array(ty) && array_base(ty) == TY_FILE); }
static FcInfo *fc_info(Function *fn)
{
    FcInfo *i; for (i = fc_infos; i; i = i->next) if (i->fn == fn) return i; return NULL;
}
static int fc_direct(Node *n, int calls)
{
    for (; n; n = n->next) {
        InputField *field;
        for (field = n->fields; field; field = field->next) if (fc_direct(field->var, calls)) return 1;
        if (fc_file_type(n->ty) || (n->var && fc_file_type(n->var->ty)) ||
            (n->kind == ND_BUILTIN && n->val >= BI_FILE_OPEN)) return 1;
        if (calls && n->kind == ND_CALL && fc_info(n->func)->relevant) return 1;
        if (fc_direct(n->lhs, calls) || fc_direct(n->rhs, calls) || fc_direct(n->cond, calls) ||
            fc_direct(n->then, calls) || fc_direct(n->els, calls) || fc_direct(n->init, calls) ||
            fc_direct(n->step, calls) || fc_direct(n->end, calls)) return 1;
    }
    return 0;
}
static char *fc_index(Node *n)
{
    char key[64];
    if (!is_const_expr(n)) return NULL;
    snprintf(key, sizeof(key), "[%lld]", eval_const(n).i); return fc_join(key, "");
}
static FcMap *fc_place(FcContext *ctx, FcState *s, Node *n)
{
    if (n->kind == ND_VAR) return fc_value(s, n->var);
    if (n->kind == ND_INDEX) {
        char *key = fc_index(n->rhs);
        FcMap *base = fc_place(ctx, s, n->lhs);
        if (!key) { FcMap *u = fc_empty(); u->origin = -1; return u; }
        return fc_slice(base, key);
    }
    return fc_empty();
}
static void fc_store(FcContext *ctx, FcState *s, Node *target, FcMap *m)
{
    if (target->kind == ND_VAR) fc_set(s, target->var, m);
    else if (target->kind == ND_INDEX) {
        char *key = fc_index(target->rhs); FcMap *base = fc_place(ctx, s, target->lhs);
        FcMap *copy = fc_empty(); FcEntry *e;
        if (!key) { copy->origin = -1; fc_store(ctx, s, target->lhs, copy); return; }
        copy->origin = base->origin; copy->prefix = base->prefix; copy->length = base->length;
        for (e = base->entries; e; e = e->next)
            if (strncmp(e->path, key, strlen(key)) != 0) fc_put(copy, e->path, e->id);
        for (e = m->entries; e; e = e->next) fc_put(copy, fc_join(key, e->path), e->id);
        if (m->origin != 0 && m->entries == NULL) fc_put(copy, key, fc_get(ctx, m, ""));
        fc_store(ctx, s, target->lhs, copy);
    }
}
static int fc_truth(FcContext *ctx, FcState *s, Node *n)
{
    if (is_const_expr(n) && n->ty == TY_BOOL) return eval_const(n).i != 0;
    if (n->kind == ND_NOT) { int v = fc_truth(ctx, s, n->lhs); return v < 0 ? -1 : !v; }
    if (n->kind == ND_BUILTIN && n->val == BI_FILE_IS_OPEN &&
        (n->lhs->kind == ND_VAR || n->lhs->kind == ND_INDEX)) {
        int id = fc_get(ctx, fc_place(ctx, s, n->lhs), "");
        if (id >= 0 && !(id > 0 && ctx->resources[id].parameter > 0 && !s->known[id])) return fc_alive(ctx, s, id);
    }
    return -1;
}
static void fc_assume(FcContext *ctx, FcState *s, Node *condition, int truth)
{
    if (condition->kind == ND_NOT) { fc_assume(ctx, s, condition->lhs, !truth); return; }
    if (condition->kind == ND_BUILTIN && condition->val == BI_FILE_IS_OPEN &&
        (condition->lhs->kind == ND_VAR || condition->lhs->kind == ND_INDEX)) {
        int id = fc_get(ctx, fc_place(ctx, s, condition->lhs), "");
        if (id > 0) { s->known[id] = 1; s->live[id] = (unsigned char)truth; }
    }
}
static void fc_merge_optional(FcContext *ctx, FcState *s, FcState *other)
{
    FcEnv *e; int i;
    for (i = 1; i < ctx->count; i++) {
        int live = fc_alive(ctx, s, i) || fc_alive(ctx, other, i);
        s->known[i] = 1; s->live[i] = (unsigned char)live;
    }
    for (e = s->env; e; e = e->next) if (fc_value(s, e->var) != fc_value(other, e->var)) {
        FcMap *u = fc_empty(); u->origin = -1; fc_set(s, e->var, u);
    }
}
/* Instantiate a callee's return/ref-output map in the caller. */
static FcMap *fc_transfer(FcContext *ctx, FcState *s, FcInfo *callee, FcMap **args, int output, Node *site)
{
    FcFlow *f; FcMap *result = fc_empty(); FcEntry *e;
    FcMap *first = NULL; Var *parameter = callee->fn->params; int j;
    for (j = 1; parameter && j < output; j++) parameter = parameter->next;
    for (f = callee->exits; f; f = f->next) {
        FcMap *m = output ? fc_value(f->state, parameter) : f->value;
        if (!first) first = m;
        for (e = m->entries; e; e = e->next) {
            FcEntry *r;
            for (r = result->entries; r; r = r->next) if (strcmp(r->path, e->path) == 0) break;
            if (!r) fc_put(result, e->path, 0);
        }
    }
    if (!first) return result;
    result->length = first->length;
    for (e = result->entries; e; e = e->next) {
        int owner = 0, source = -2;
        for (f = callee->exits; f; f = f->next) {
            FcMap *m = output ? fc_value(f->state, parameter) : f->value;
            int id = fc_get(callee->context, m, e->path);
            int actual = 0;
            if (id < 0) actual = -1;
            else if (id > 0) {
                FcResource *res = &callee->context->resources[id];
                if (res->parameter > 0) actual = fc_get(ctx, args[res->parameter - 1], res->path);
                else if (fc_alive(callee->context, f->state, id)) owner = 1;
            }
            if (source == -2) source = actual;
            else if (source != actual) source = -1;
        }
        if (owner) { e->id = fc_id(ctx, site, 0, ""); s->known[e->id] = s->live[e->id] = 1; }
        else e->id = source == -2 ? 0 : source;
    }
    /* An untouched borrowed array carries its origin through the call. */
    if (first->origin > 0 && result->entries == NULL) {
        FcMap *arg = args[first->origin - 1]; return fc_slice(arg, first->prefix);
    }
    return result;
}
static FcMap *fc_call(FcContext *ctx, FcState *s, Node *n)
{
    FcInfo *callee = fc_info(n->func); FcMap **args; Node *arg; Var *p; FcFlow *f;
    int i, id;
    args = fc_new((size_t)(n->nargs + 1) * sizeof(*args));
    i = 0; for (arg = n->lhs; arg; arg = arg->next) args[i++] = fc_eval(ctx, s, arg->kind == ND_REFARG ? arg->lhs : arg);
    if (!callee->relevant) return fc_empty();
    if (callee->status == 1) error_tok(n->tok, "再帰する file 関数の close を証明できません。ファイル操作を再帰の外へ分離してください");
    fc_analyze(callee);
    for (id = 1; id < callee->context->count; id++) {
        FcResource *res = &callee->context->resources[id]; int closed = 1;
        if (res->parameter <= 0) continue;
        for (f = callee->exits; f; f = f->next) if (fc_alive(callee->context, f->state, id)) closed = 0;
        if (closed) {
            int caller = fc_get(ctx, args[res->parameter - 1], res->path);
            if (caller > 0) { s->known[caller] = 1; s->live[caller] = 0; }
        }
    }
    i = 1; p = callee->fn->params;
    for (arg = n->lhs; arg; arg = arg->next, p = p->next, i++)
        if (p->is_ref && fc_file_type(p->ty)) fc_store(ctx, s, arg->lhs, fc_transfer(ctx, s, callee, args, i, n));
    return fc_transfer(ctx, s, callee, args, 0, n);
}
static FcMap *fc_eval(FcContext *ctx, FcState *s, Node *n)
{
    Node *p; FcMap *m; int i;
    if (!n) return fc_empty();
    if (n->kind == ND_VAR) return fc_value(s, n->var);
    if (n->kind == ND_CALL) return fc_call(ctx, s, n);
    if (n->kind == ND_ARRAY_COPY) return fc_eval(ctx, s, n->lhs);
    if (n->kind == ND_ARRAY_NEW) { m = fc_empty(); m->length = n->dims && n->dims->limit >= 0 ? n->dims->limit : 0; return m; }
    if (n->kind == ND_ARRAY_LITERAL) {
        m = fc_empty(); m->length = n->nargs; i = 0;
        for (p = n->lhs; p; p = p->next, i++) {
            char key[64]; FcEntry *e; FcMap *item = fc_eval(ctx, s, p);
            snprintf(key, sizeof(key), "[%d]", i);
            for (e = item->entries; e; e = e->next) fc_put(m, fc_join(key, e->path), e->id);
        }
        return m;
    }
    if (n->kind == ND_INDEX) {
        char *key; m = fc_eval(ctx, s, n->lhs); fc_eval(ctx, s, n->rhs); key = fc_index(n->rhs);
        if (!key) { FcMap *u = fc_empty(); u->origin = -1; return u; }
        m = fc_slice(m, key);
        if (n->ty == TY_FILE) return fc_scalar(fc_get(ctx, m, ""));
        return m;
    }
    if (n->kind == ND_BUILTIN) {
        m = fc_empty(); i = 0;
        for (p = n->lhs; p; p = p->next, i++) { FcMap *v = fc_eval(ctx, s, p->kind == ND_REFARG ? p->lhs : p); if (i == 0) m = v; }
        if (n->val == BI_FILE_OPEN) { int id = fc_id(ctx, n, 0, ""); s->known[id] = s->live[id] = 1; return fc_scalar(id); }
        if (n->val == BI_FILE_CLOSE) { int id = fc_get(ctx, m, ""); if (id > 0) { s->known[id] = 1; s->live[id] = 0; } }
        return fc_empty();
    }
    if (n->kind == ND_METHOD) {
        m = fc_eval(ctx, s, n->lhs);
        {
            FcMap *item = fc_eval(ctx, s, n->rhs); FcMap *copy = fc_empty(); FcEntry *e;
            copy->origin = m->origin; copy->prefix = m->prefix; copy->length = m->length;
            for (e = m->entries; e; e = e->next) fc_put(copy, e->path, e->id);
            if (n->val == AM_ADD && m->length >= 0) {
                char key[64]; snprintf(key, sizeof(key), "[%lld]", m->length); copy->length++;
                for (e = item->entries; e; e = e->next) fc_put(copy, fc_join(key, e->path), e->id);
            } else { copy = fc_empty(); copy->origin = n->val == AM_CLEAR ? 0 : -1; copy->length = n->val == AM_CLEAR ? 0 : -1; }
            fc_store(ctx, s, n->lhs, copy);
        }
        return fc_empty();
    }
    if (n->kind == ND_AND || n->kind == ND_OR) {
        int truth; fc_eval(ctx, s, n->lhs); truth = fc_truth(ctx, s, n->lhs);
        if (truth == (n->kind == ND_AND ? 0 : 1)) return fc_empty();
        if (truth >= 0) fc_eval(ctx, s, n->rhs);
        else { FcState *optional = fc_clone(s); fc_eval(ctx, optional, n->rhs); fc_merge_optional(ctx, s, optional); }
        return fc_empty();
    }
    if (n->kind == ND_CHAIN) {
        Node *previous = NULL;
        for (p = n->lhs; p; p = p->next) {
            FcState *evaluated = previous ? fc_clone(s) : s;
            if (p->lhs != previous) fc_eval(ctx, evaluated, p->lhs);
            fc_eval(ctx, evaluated, p->rhs);
            if (previous) fc_merge_optional(ctx, s, evaluated);
            previous = p->rhs;
        }
        return fc_empty();
    }
    if (n->kind == ND_INPUT) {
        InputField *field;
        fc_eval(ctx, s, n->lhs);
        for (field = n->fields; field; field = field->next) fc_eval(ctx, s, field->var);
        return fc_empty();
    }
    if (n->kind == ND_FORMAT || n->kind == ND_PRINT) {
        for (p = n->lhs; p; p = p->next) fc_eval(ctx, s, p);
        for (p = n->end; p; p = p->next) fc_eval(ctx, s, p);
    } else { fc_eval(ctx, s, n->lhs); fc_eval(ctx, s, n->rhs); }
    return fc_empty();
}
static FcFlow *fc_flow(FcState *s, int kind, FcMap *value)
{
    FcFlow *f = fc_new(sizeof(*f)); f->state = s; f->kind = kind; f->value = value ? value : fc_empty(); return f;
}
static FcFlow *fc_append(FcFlow *a, FcFlow *b, Token *site)
{
    FcFlow *p; int count = 0;
    if (!a) return b;
    for (p = a; p->next; p = p->next) count++;
    p->next = b;
    for (p = b; p; p = p->next) count++;
    if (count >= FC_MAX_PATHS) error_tok(site, "close の解析経路が多すぎます。関数を分割してください");
    return a;
}
static FcFlow *fc_step(FcContext *ctx, FcState *s, Node *n)
{
    if (n->kind == ND_IF) {
        int truth; FcFlow *a = NULL, *b = NULL;
        fc_eval(ctx, s, n->cond); truth = fc_truth(ctx, s, n->cond);
        if (truth != 0) { FcState *branch = fc_clone(s); fc_assume(ctx, branch, n->cond, 1); a = fc_run(ctx, branch, n->then); }
        if (truth != 1) { FcState *branch = fc_clone(s); fc_assume(ctx, branch, n->cond, 0); b = fc_run(ctx, branch, n->els); }
        return fc_append(a, b, n->tok);
    }
    if (n->kind == ND_FOR || n->kind == ND_WHILE) {
        FcFlow *body, *f, *out = NULL; FcState *baseline; int before, truth;
        if (n->init) fc_eval(ctx, s, n->init->lhs);
        baseline = fc_clone(s); before = ctx->count;
        fc_eval(ctx, s, n->cond); truth = n->kind == ND_WHILE ? fc_truth(ctx, s, n->cond) : -1;
        if (truth != 1) { FcState *empty = fc_clone(s); fc_assume(ctx, empty, n->cond, 0); out = fc_flow(empty, 0, NULL); }
        if (truth == 0) return out;
        { FcState *iteration = fc_clone(s); fc_assume(ctx, iteration, n->cond, 1); body = fc_run(ctx, iteration, n->then); }
        for (f = body; f; f = f->next) {
            int id;
            if (f->kind == 1) { out = fc_append(out, fc_flow(f->state, 1, f->value), n->tok); continue; }
            if (f->kind == 2) { out = fc_append(out, fc_flow(f->state, 0, NULL), n->tok); continue; }
            for (id = before; id < ctx->count; id++)
                if (ctx->resources[id].parameter == 0 && fc_alive(ctx, f->state, id))
                    error_tok(ctx->resources[id].site->tok, "ループの次の反復までに file の close を確認できません");
            {
                FcEnv *e;
                for (e = baseline->env; e; e = e->next) {
                    FcMap *old = fc_value(baseline, e->var), *now = fc_value(f->state, e->var); FcEntry *entry;
                    if (old == now) continue;
                    for (entry = old->entries; entry; entry = entry->next)
                        if (entry->id > 0 && fc_alive(ctx, f->state, entry->id))
                            error_tok(n->tok, "ループで file の別名が変わるため close を証明できません");
                }
            }
            if (truth != 1) out = fc_append(out, fc_flow(f->state, 0, NULL), n->tok);
            else for (id = 1; id < ctx->count; id++)
                if (ctx->resources[id].parameter == 0 && fc_alive(ctx, f->state, id))
                    error_tok(ctx->resources[id].site->tok, "終了しないループへ入る前に file を close してください");
        }
        return out;
    }
    if (n->kind == ND_RETURN) return fc_flow(s, 1, fc_eval(ctx, s, n->lhs));
    if (n->kind == ND_BREAK || n->kind == ND_CONTINUE) return fc_flow(s, n->kind == ND_BREAK ? 2 : 3, NULL);
    if (n->kind == ND_DECL) {
        if (n->var->is_ref && n->lhs && n->lhs->kind == ND_ADDR) fc_env(s, n->var)->alias = fc_reference_target(s, n->lhs->lhs->var);
        else { FcMap *m = fc_eval(ctx, s, n->lhs); if (fc_file_type(n->var->ty)) fc_set(s, n->var, m); }
    } else if (n->kind == ND_ASSIGN) { FcMap *m = fc_eval(ctx, s, n->lhs); if (fc_file_type(n->var->ty)) fc_set(s, n->var, m); }
    else if (n->kind == ND_STORE) {
        fc_eval(ctx, s, n->lhs); { FcMap *m = fc_eval(ctx, s, n->rhs); if (fc_file_type(n->lhs->ty)) fc_store(ctx, s, n->lhs, m); }
    } else if (n->kind == ND_ADDR_ASSIGN) {
        if (n->lhs->kind == ND_ADDR) fc_env(s, n->var)->alias = fc_reference_target(s, n->lhs->lhs->var);
        else { fc_env(s, n->var)->alias = NULL; fc_set(s, n->var, fc_empty()); }
    } else fc_eval(ctx, s, n->kind == ND_EXPR_STMT ? n->lhs : n);
    return fc_flow(s, 0, NULL);
}
static FcFlow *fc_run(FcContext *ctx, FcState *s, Node *n)
{
    FcFlow *flows = fc_flow(s, 0, NULL);
    for (; n; n = n->next) {
        FcFlow *f, *next = NULL;
        for (f = flows; f; f = f->next)
            next = fc_append(next, f->kind ? fc_flow(f->state, f->kind, f->value) : fc_step(ctx, f->state, n), n->tok);
        flows = next;
    }
    return flows;
}
static int fc_contains(FcMap *m, int id)
{
    FcEntry *e; for (e = m->entries; e; e = e->next) if (e->id == id) return 1; return 0;
}
static void fc_analyze(FcInfo *info)
{
    FcContext *ctx; FcState *s; FcFlow *f; Var *p; int index = 1;
    if (info->status == 2 || !info->relevant) return;
    info->status = 1; ctx = fc_new(sizeof(*ctx)); ctx->info = info; ctx->count = 1; info->context = ctx;
    s = fc_new(sizeof(*s));
    for (p = info->fn->params; p; p = p->next, index++) if (fc_file_type(p->ty)) {
        FcMap *m = fc_empty(); m->origin = index;
        if (p->ty == TY_FILE) fc_put(m, "", fc_id(ctx, NULL, index, ""));
        fc_set(s, p, m);
    }
    info->exits = fc_run(ctx, s, info->fn->body);
    for (f = info->exits; f; f = f->next) {
        int id;
        for (id = 1; id < ctx->count; id++) if (ctx->resources[id].parameter == 0 && fc_alive(ctx, f->state, id)) {
            int escaped = fc_contains(f->value, id);
            for (p = info->fn->params; p; p = p->next)
                if (p->is_ref && fc_file_type(p->ty) && fc_contains(fc_value(f->state, p), id)) escaped = 1;
            if (!escaped) error_tok(ctx->resources[id].site->tok,
                "この file が正常終了する全経路で明示的に close されることを確認できません（関数 %.*s）", info->fn->name_len, info->fn->name);
        }
    }
    info->status = 2;
}
static void check_file_closes(Ast *ast)
{
    Function *fn; FcInfo *i; int changed;
    fc_infos = NULL;
    for (fn = ast->funcs; fn; fn = fn->next) {
        Var *p; i = fc_new(sizeof(*i)); i->fn = fn; i->next = fc_infos; fc_infos = i;
        if (fc_file_type(fn->ret)) i->relevant = 1;
        for (p = fn->params; p; p = p->next) if (fc_file_type(p->ty)) i->relevant = 1;
    }
    for (i = fc_infos; i; i = i->next) if (fc_direct(i->fn->body, 0)) i->relevant = 1;
    do { changed = 0; for (i = fc_infos; i; i = i->next) if (!i->relevant && fc_direct(i->fn->body, 1)) { i->relevant = 1; changed = 1; } } while (changed);
    for (i = fc_infos; i; i = i->next) fc_analyze(i);
}
