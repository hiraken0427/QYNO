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
typedef struct FcPredicate FcPredicate;
struct FcPredicate {FcPredicate *next;Node *site;int success_id;};
typedef struct FcContext FcContext;
struct FcEntry { FcEntry *next; char *path; int id; };
struct FcMap { int success_id; FcEntry *entries; int origin; char *prefix; long long length; };
struct FcEnv { FcEnv *next; Var *var; FcMap *value; Var *alias; };
struct FcState { unsigned char live[FC_MAX_RESOURCES], known[FC_MAX_RESOURCES]; FcEnv *env; FcPredicate *predicates; };
struct FcFlow { FcFlow *next; FcState *state; int kind; int truth; unsigned error_codes; FcMap *value; };
typedef struct { Node *site; int parameter; char *path; } FcResource;
struct FcContext { FcInfo *info; int count; unsigned caught_codes; FcFlow *errors; FcResource resources[FC_MAX_RESOURCES]; };
struct FcInfo {
    FcInfo *next; Function *fn; int relevant, status; FcContext *context;
    FcFlow *exits, *errors;
};
static FcInfo *fc_infos;
static FcMap *fc_eval(FcContext *ctx, FcState *s, Node *n);
static FcFlow *fc_run(FcContext *ctx, FcState *s, Node *n);
static void fc_analyze(FcInfo *info);
static FcFlow *fc_flow(FcState *s,int kind,FcMap *value);
static FcFlow *fc_append(FcFlow *a,FcFlow *b,Token *site);
static FcState *fc_clone(FcState *s);
static void fc_error(FcContext *ctx,FcState *s,Node *site,unsigned codes)
{
    FcFlow *f;
    if(!io_has_recovery)return;
    f=fc_flow(fc_clone(s),4,NULL);f->error_codes=codes;
    ctx->errors=fc_append(ctx->errors,f,site->tok);
}

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
static int fc_file_type(Type ty)
{
    ModelField *f;
    if (type_is_array(ty)) return fc_file_type(array_base(ty));
    if (type_is_model(ty)) {
        for (f = type_model(ty)->fields; f; f = f->next) if (fc_file_type(f->ty)) return 1;
        return 0;
    }
    return ty == TY_FILE;
}
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
        if (calls && (n->kind == ND_CALL || n->kind == ND_CONSTRUCT) && fc_info(n->func)->relevant) return 1;
        if (calls && n->kind == ND_MODEL_NEW && fc_info(type_model(n->ty)->default_builder)->relevant) return 1;
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
    {int id=0;FcPredicate *predicate;
     for(predicate=s->predicates;predicate;predicate=predicate->next)if(predicate->site==condition){id=predicate->success_id;break;}
     if(condition->kind==ND_VAR&&condition->ty==TY_BOOL)id=fc_value(s,condition->var)->success_id;
     if(condition->kind==ND_BUILTIN&&condition->val==BI_FILE_TRY_OPEN)id=fc_get(ctx,fc_place(ctx,s,condition->lhs->next->lhs),"");
     if(id>0){s->known[id]=1;s->live[id]=(unsigned char)truth;}}
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
static FcMap *fc_transfer(FcContext *ctx, FcState *s, FcInfo *callee, FcMap **args, int output, Node *site, FcMap *owned)
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
        int owner = 0, source = -2; char *identity = "";
        for (f = callee->exits; f; f = f->next) {
            FcMap *m = output ? fc_value(f->state, parameter) : f->value;
            int id = fc_get(callee->context, m, e->path);
            { char part[32]; snprintf(part, sizeof(part), "/%d", id); identity = fc_join(identity, part); }
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
        if (owner) {
            FcEntry *entry;
            for (entry = owned->entries; entry && strcmp(entry->path, identity); entry = entry->next) { }
            if (!entry) { fc_put(owned, identity, fc_id(ctx, site, 0, "")); entry = owned->entries; }
            e->id = entry->id; s->known[e->id] = s->live[e->id] = 1;
        }
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
    FcInfo *callee = fc_info(n->func); FcMap **args; Node *arg; Var *p; FcFlow *f; FcMap *owned = fc_empty();
    int i, id;
    args = fc_new((size_t)(n->nargs + 1) * sizeof(*args));
    i = 0; for (arg = n->lhs; arg; arg = arg->next) {
        if (arg->kind == ND_REFARG && arg->rhs) fc_set(s, arg->ref_temp ? arg->ref_temp : arg->lhs->var, fc_eval(ctx, s, arg->rhs));
        args[i++] = fc_eval(ctx, s, arg->kind == ND_REFARG ? arg->lhs : arg);
    }
    if (!callee->relevant) return fc_empty();
    if (callee->status == 1) error_tok(n->tok, "再帰する file 関数の close を証明できません。ファイル操作を再帰の外へ分離してください");
    fc_analyze(callee);
    for(f=callee->errors;f;f=f->next){
        FcState *failed=fc_clone(s);FcFlow *saved_exits=callee->exits,*saved_next=f->next;FcMap *moved=fc_empty();
        for(id=1;id<callee->context->count;id++){
            FcResource *res=&callee->context->resources[id];
            if(res->parameter>0&&!fc_alive(callee->context,f->state,id)){
                int actual=fc_get(ctx,args[res->parameter-1],res->path);
                if(actual>0){failed->known[actual]=1;failed->live[actual]=0;}
            }
        }
        callee->exits=f;f->next=NULL;
        i=1;p=callee->fn->params;
        for(arg=n->lhs;arg;arg=arg->next,p=p->next,i++)if(p->is_ref&&fc_file_type(p->ty))
            fc_store(ctx,failed,arg->lhs,fc_transfer(ctx,failed,callee,args,i,n,moved));
        f->next=saved_next;callee->exits=saved_exits;
        fc_error(ctx,failed,n,f->error_codes);
    }
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
        if (p->is_ref && fc_file_type(p->ty)) fc_store(ctx, s, arg->lhs, fc_transfer(ctx, s, callee, args, i, n, owned));
    {FcMap *result=fc_transfer(ctx,s,callee,args,0,n,owned);
     if(n->ty==TY_BOOL){FcEntry *e;for(e=owned->entries;e;e=e->next){int matches=1,has_true=0,has_false=0;
       for(f=callee->exits;f;f=f->next){int live=0;Var *q;
         for(q=callee->fn->params;q;q=q->next)if(q->is_ref){FcEntry *entry;for(entry=fc_value(f->state,q)->entries;entry;entry=entry->next)
           if(entry->id>0&&callee->context->resources[entry->id].parameter==0&&fc_alive(callee->context,f->state,entry->id))live=1;}
         if(f->truth==1)has_true=1;else if(f->truth==0)has_false=1;else matches=0;
         if(f->truth!=live)matches=0;
       }
       if(matches&&has_true&&has_false&&owned->entries->next==NULL)result->success_id=e->id;
     }}
     if(result->success_id){FcPredicate *predicate=fc_new(sizeof(*predicate));predicate->site=n;predicate->success_id=result->success_id;predicate->next=s->predicates;s->predicates=predicate;}
     return result;}
}
static Node *fc_array_recipe(Node *site, long long index, Type ty, ArrayDim *dims)
{
    Node **end = &site->init;
    while (*end && (*end)->val != index) end = &(*end)->next;
    if (!*end) {
        *end = fc_new(sizeof(**end)); (*end)->val = index; (*end)->tok = site->tok;
        (*end)->ty = ty; (*end)->dims = dims;
        (*end)->kind = type_is_model(ty) ? ND_MODEL_NEW : ND_ARRAY_NEW;
    }
    return *end;
}
static FcMap *fc_array_padding(FcContext *ctx, FcState *s, Node *site, Type ty, ArrayDim *dims, FcMap *source, Node *syntax)
{
    long long length = source->length, target = dims ? dims->limit : -1, count, i;
    FcMap *result = fc_empty(); Type element = array_element(ty); Node *item = NULL;
    while (syntax && syntax->kind == ND_ARRAY_COPY) syntax = syntax->lhs;
    if (syntax && syntax->kind == ND_ARRAY_LITERAL) { length = syntax->nargs; item = syntax->lhs; }
    if (length < 0) {
        if (target >= 0 || (type_is_array(element) && dims && dims->next))
            error_tok(site->tok, "init による配列要素の file 所有責任を追跡できません。要素数が分かる初期値を指定してください");
        return source;
    }
    count = target > length ? target : length;
    if (count > FC_MAX_RESOURCES) error_tok(site->tok, "file を生成する配列が大きすぎて close を証明できません");
    result->length = count;
    for (i = 0; i < count; i++) {
        char key[64]; FcMap *child; FcEntry *e;
        Node *recipe = fc_array_recipe(site, i, element, dims ? dims->next : NULL);
        snprintf(key, sizeof(key), "[%lld]", i);
        if (i < length) child = fc_slice(source, key);
        else { child = fc_empty(); child->length = 0; }
        if (type_is_array(element)) child = fc_array_padding(ctx, s, recipe, element, dims ? dims->next : NULL, child, item);
        else if (i >= length) child = fc_eval(ctx, s, recipe);
        for (e = child->entries; e; e = e->next) fc_put(result, fc_join(key, e->path), e->id);
        if (item) item = item->next;
    }
    return result;
}
static FcMap *fc_eval(FcContext *ctx, FcState *s, Node *n)
{
    Node *p; FcMap *m; int i;
    if (!n) return fc_empty();
    if (n->kind == ND_VAR) return fc_value(s, n->var);
    if (n->kind == ND_CONSTRUCT || n->kind == ND_MODEL_NEW) {
        if (n->kind == ND_CONSTRUCT) for (p = n->lhs; p; p = p->next) fc_eval(ctx, s, p->lhs);
        if (!n->init) {
            Node *call = fc_new(sizeof(*call)), **end = &call->lhs; Var *p;
            call->kind = ND_CALL; call->ty = n->ty; call->tok = n->tok;
            call->func = n->kind == ND_MODEL_NEW ? type_model(n->ty)->default_builder : n->func;
            call->nargs = call->func->param_count;
            *end = fc_new(sizeof(**end)); (*end)->kind = ND_NUM; (*end)->ty = TY_ADDR; end = &(*end)->next;
            for (p = call->func->params->next; p; p = p->next) {
                Node *item;
                for (item = n->lhs; item && item->var != p; item = item->next) { }
                if (!item) fatal("生成引数の close 解析が不正です");
                *end = fc_new(sizeof(**end)); (*end)->kind = ND_NUM; (*end)->ty = p->ty; end = &(*end)->next;
            }
            n->init = call;
        }
        return fc_call(ctx, s, n->init);
    }
    if (n->kind == ND_CALL) return fc_call(ctx, s, n);
    if (n->kind == ND_MODEL_COPY || n->kind == ND_ARRAY_COPY) {
        m = fc_eval(ctx, s, n->lhs);
        if (n->kind == ND_ARRAY_COPY && n->val && contains_file(n->ty) && contains_initializer(n->ty))
            m = fc_array_padding(ctx, s, n, n->ty, n->dims, m, n->lhs);
        return m;
    }
    if (n->kind == ND_ARRAY_NEW) {
        m = fc_empty(); m->length = 0;
        if (contains_file(n->ty) && contains_initializer(n->ty)) return fc_array_padding(ctx, s, n, n->ty, n->dims, m, NULL);
        m->length = n->dims && n->dims->limit >= 0 ? n->dims->limit : 0; return m;
    }
    if (n->kind == ND_MODEL_INIT || n->kind == ND_MODEL_SEED) {
        if (n->kind == ND_MODEL_SEED && ctx->info->fn == type_model(n->ty)->builder && type_model(n->ty)->initializer)
            return fc_empty(); /* The initializer summary owns and evaluates its seed. */
        m = fc_empty(); m->length = type_model(n->ty)->field_count;
        for (p = n->lhs; p; p = p->next) {
            char key[64]; FcEntry *e; FcMap *item = fc_eval(ctx, s, p->lhs);
            snprintf(key, sizeof(key), "[%d]", p->field->index);
            for (e = item->entries; e; e = e->next) fc_put(m, fc_join(key, e->path), e->id);
        }
        return m;
    }
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
        if(n->val==BI_FILE_TRY_OPEN){
            int id=fc_id(ctx,n,0,"");FcMap *result=fc_empty();
            s->known[id]=s->live[id]=1;fc_store(ctx,s,n->lhs->next->lhs,fc_scalar(id));result->success_id=id;return result;
        }
        if(n->val>=BI_FILE_OPEN&&n->val!=BI_FILE_CLOSE&&n->val!=BI_FILE_IS_OPEN)fc_error(ctx,s,n,0xFFEu);
        if (n->val == BI_FILE_OPEN) { int id = fc_id(ctx, n, 0, ""); s->known[id] = s->live[id] = 1; return fc_scalar(id); }
        if (n->val == BI_FILE_CLOSE) { int id = fc_get(ctx, m, ""); if (id > 0) { s->known[id] = 1; s->live[id] = 0; } }
        if(n->val==BI_FILE_CLOSE)fc_error(ctx,s,n,0xFFEu);
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
    if (n->kind == ND_INT_PARSE || n->kind == ND_INT_FORMAT || n->kind == ND_FORMAT || n->kind == ND_PRINT) {
        for (p = n->lhs; p; p = p->next) fc_eval(ctx, s, p);
        for (p = n->end; p; p = p->next) fc_eval(ctx, s, p);
    } else { fc_eval(ctx, s, n->lhs); fc_eval(ctx, s, n->rhs); }
    return fc_empty();
}
static FcFlow *fc_flow(FcState *s, int kind, FcMap *value)
{
    FcFlow *f = fc_new(sizeof(*f)); f->state = s; f->kind = kind; f->truth = -1; f->value = value ? value : fc_empty(); return f;
}
static FcFlow *fc_copy_flow(FcFlow *f)
{
    FcFlow *copy = fc_flow(f->state, f->kind, f->value); copy->truth = f->truth; copy->error_codes=f->error_codes;return copy;
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
static FcFlow *fc_try(FcContext *ctx,FcState *s,Node *n)
{
    FcFlow *head=fc_run(ctx,s,n->lhs),*f,*flows=NULL,*out=NULL;
    for(f=head;f;f=f->next)flows=fc_append(flows,f->kind?fc_copy_flow(f):fc_run(ctx,f->state,n->then),n->tok);
    for(f=flows;f;f=f->next){
        FcFlow *handled=NULL,*h;Node *arm;
        if(f->kind==4){
            unsigned rest=f->error_codes,saved=ctx->caught_codes;
            for(arm=n->els;arm;arm=arm->next){
                Node *p;unsigned codes=arm->lhs?0:rest;
                for(p=arm->lhs;p;p=p->next)codes|=1u<<p->val;
                codes&=rest;if(!codes)continue;rest&=~codes;ctx->caught_codes=codes;
                handled=fc_append(handled,fc_run(ctx,fc_clone(f->state),arm->then),n->tok);
            }
            ctx->caught_codes=saved;
            if(rest){FcFlow *copy=fc_copy_flow(f);copy->error_codes=rest;handled=fc_append(handled,copy,n->tok);}
        }else handled=fc_copy_flow(f);
        for(h=handled;h;h=h->next){
            FcFlow *cleanup=fc_run(ctx,h->state,n->rhs),*c;
            for(c=cleanup;c;c=c->next){
                if(!c->kind){c->kind=h->kind;c->truth=h->truth;c->value=h->value;c->error_codes=h->error_codes;}
                out=fc_append(out,fc_copy_flow(c),n->tok);
            }
        }
    }
    return out;
}
static FcFlow *fc_step(FcContext *ctx, FcState *s, Node *n)
{
    if(n->kind==ND_TRY)return fc_try(ctx,s,n);
    if(n->kind==ND_THROW){FcFlow *f=fc_flow(s,4,NULL);f->error_codes=ctx->caught_codes?ctx->caught_codes:0xFFEu;return f;}
    if (n->kind == ND_BUILD_REJECT) return NULL;
    if (n->kind == ND_IF) {
        int truth; FcFlow *a = NULL, *b = NULL;
        fc_eval(ctx, s, n->cond); truth = fc_truth(ctx, s, n->cond);
        if (truth != 0) { FcState *branch = fc_clone(s); fc_assume(ctx, branch, n->cond, 1); a = fc_run(ctx, branch, n->then); }
        if (truth != 1) { FcState *branch = fc_clone(s); fc_assume(ctx, branch, n->cond, 0); b = fc_run(ctx, branch, n->els); }
        return fc_append(a, b, n->tok);
    }
    if (n->kind == ND_FOR || n->kind == ND_WHILE) {
        FcFlow *body, *f, *out = NULL; FcState *baseline; int before, truth, has_exit = 0;
        if (n->init) fc_eval(ctx, s, n->init->lhs);
        baseline = fc_clone(s); before = ctx->count;
        fc_eval(ctx, s, n->cond); truth = n->kind == ND_WHILE ? fc_truth(ctx, s, n->cond) : -1;
        if (truth != 1) { FcState *empty = fc_clone(s); fc_assume(ctx, empty, n->cond, 0); out = fc_flow(empty, 0, NULL); }
        if (truth == 0) return out;
        { FcState *iteration = fc_clone(s); fc_assume(ctx, iteration, n->cond, 1); body = fc_run(ctx, iteration, n->then); }
        /* A reachable break/return supplies a normal exit, just as a dynamic
         * while condition does. Keep per-iteration resource/alias checks below. */
        for (f = body; f; f = f->next)
            if (f->kind == 1 || f->kind == 2) has_exit = 1;
        for (f = body; f; f = f->next) {
            int id;
            if (f->kind == 1 || f->kind==4) { out = fc_append(out, fc_copy_flow(f), n->tok); continue; }
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
            else if (!has_exit) for (id = 1; id < ctx->count; id++)
                if (ctx->resources[id].parameter == 0 && fc_alive(ctx, f->state, id))
                    error_tok(ctx->resources[id].site->tok, "終了しないループへ入る前に file を close してください");
        }
        return out;
    }
    if (n->kind == ND_RETURN) {
        FcFlow *f = fc_flow(s, 1, fc_eval(ctx, s, n->lhs));
        if(n->lhs&&n->lhs->ty==TY_BOOL){
            int id=f->value->success_id;
            f->truth=fc_truth(ctx,s,n->lhs);
            if(id>0&&f->truth<0){FcFlow *failed=fc_copy_flow(f);failed->state=fc_clone(s);failed->state->known[id]=1;failed->state->live[id]=0;failed->truth=0;f->truth=1;f->next=failed;}
        }
        return f;
    }
    if (n->kind == ND_BREAK || n->kind == ND_CONTINUE) return fc_flow(s, n->kind == ND_BREAK ? 2 : 3, NULL);
    if (n->kind == ND_DECL) {
        if (n->var->is_ref && n->lhs && n->lhs->kind == ND_ADDR) fc_env(s, n->var)->alias = fc_reference_target(s, n->lhs->lhs->var);
        else { FcMap *m = fc_eval(ctx, s, n->lhs); if (fc_file_type(n->var->ty)||n->var->ty==TY_BOOL) fc_set(s, n->var, m); }
    } else if (n->kind == ND_ASSIGN) { FcMap *m = fc_eval(ctx, s, n->lhs); if (fc_file_type(n->var->ty)||n->var->ty==TY_BOOL) fc_set(s, n->var, m); }
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
        for (f = flows; f; f = f->next) {
            FcFlow *saved=ctx->errors,*step;
            ctx->errors=NULL;
            step=f->kind?fc_copy_flow(f):fc_step(ctx,f->state,n);
            step=fc_append(step,ctx->errors,n->tok);ctx->errors=saved;
            next=fc_append(next,step,n->tok);
        }
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
    if (info->fn->is_initializer)
        fc_set(s, info->fn->params, fc_eval(ctx, s, info->fn->model->default_value));
    info->exits = fc_run(ctx, s, info->fn->body);
    {FcFlow **end=&info->exits;while(*end){FcFlow *flow=*end;if(flow->kind==4){*end=flow->next;flow->next=info->errors;info->errors=flow;}else end=&flow->next;}}
    if(info->fn->io_caught)for(f=info->errors;f;f=f->next){int id;for(id=1;id<ctx->count;id++)
        if(ctx->resources[id].parameter==0&&fc_alive(ctx,f->state,id))
            error_tok(ctx->resources[id].site->tok,"回復するエラー経路で file の close を確認できません（関数 %.*s）。finally または case で close してください",info->fn->name_len,info->fn->name);}
    for (f = info->exits; f; f = f->next) {
        int id;
        for (id = 1; id < ctx->count; id++) if (ctx->resources[id].parameter == 0 && fc_alive(ctx, f->state, id)) {
            int escaped = fc_contains(f->value, id);
            for (p = info->fn->params; p; p = p->next)
                if (p->is_ref && fc_file_type(p->ty) && fc_contains(fc_value(f->state, p), id) &&
                    (!info->fn->is_initializer || info->fn->ret == TY_VOID || f->truth == 1)) escaped = 1;
            if (!escaped) error_tok(ctx->resources[id].site->tok,
                "この file が正常終了する全経路で明示的に close されることを確認できません（関数 %.*s）", info->fn->name_len, info->fn->name);
        }
    }
    if (info->fn->is_initializer && info->fn->ret == TY_BOOL) {
        FcFlow **end = &info->exits;
        while (*end) { if ((*end)->truth == 0) *end = (*end)->next; else end = &(*end)->next; }
    }
    info->status = 2;
}
static void check_file_closes(Ast *ast)
{
    Function *fn; FcInfo *i; int changed;
    fc_infos = NULL;
    for (fn = ast->funcs; fn; fn = fn->next) {
        Var *p; if (fn->is_template) continue; i = fc_new(sizeof(*i)); i->fn = fn; i->next = fc_infos; fc_infos = i;
        if (fc_file_type(fn->ret)) i->relevant = 1;
        for (p = fn->params; p; p = p->next) if (fc_file_type(p->ty)) i->relevant = 1;
    }
    for (i = fc_infos; i; i = i->next) if (fc_direct(i->fn->body, 0)) i->relevant = 1;
    do { changed = 0; for (i = fc_infos; i; i = i->next) if (!i->relevant && fc_direct(i->fn->body, 1)) { i->relevant = 1; changed = 1; } } while (changed);
    for (i = fc_infos; i; i = i->next) fc_analyze(i);
}
