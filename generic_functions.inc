/* Clone untyped source, never an already specialized/lowered body. */
typedef struct GenericCopy GenericCopy;
struct GenericCopy { GenericCopy *next; const void *old; void *copy; };
typedef struct GenericSpec GenericSpec;
struct GenericSpec { GenericSpec *next; Function *origin, *fn; Type *types; };
static GenericSpec *generic_specs;
static int generic_spec_count;
static void *generic_alloc(size_t size)
{
    void *p = calloc(1, size);
    if (!p) fatal("関数の具体化を確保できません");
    return p;
}
static void *generic_lookup(GenericCopy *map, const void *old)
{
    for (; map; map = map->next) if (map->old == old) return map->copy;
    return NULL;
}
static void generic_register(GenericCopy **map, const void *old, void *copy)
{
    GenericCopy *item = generic_alloc(sizeof(*item));
    item->old = old; item->copy = copy; item->next = *map; *map = item;
}
static Var *generic_var(GenericCopy *map, Var *old)
{
    Var *copy = generic_lookup(map, old);
    return copy ? copy : old;
}
static Node *generic_node(Node *old, GenericCopy **map);
static ArrayDim *generic_dims(ArrayDim *old, GenericCopy **map)
{
    ArrayDim *d;
    if (!old) return NULL;
    d = generic_lookup(*map, old); if (d) return d;
    d = generic_alloc(sizeof(*d)); *d = *old; generic_register(map, old, d);
    d->next = generic_dims(old->next, map); d->bound = generic_node(old->bound, map);
    return d;
}
static Node *generic_node(Node *old, GenericCopy **map)
{
    Node *n; InputField *f, **end;
    if (!old) return NULL;
    n = generic_lookup(*map, old); if (n) return n;
    n = generic_alloc(sizeof(*n)); *n = *old; generic_register(map, old, n);
    n->var = generic_var(*map, old->var); n->ref_temp = generic_var(*map, old->ref_temp);
    n->in_line = generic_var(*map, old->in_line); n->in_cur = generic_var(*map, old->in_cur); n->in_end = generic_var(*map, old->in_end);
    n->dims = generic_dims(old->dims, map);
#define GENERIC_NODE(member) n->member = generic_node(old->member, map)
    GENERIC_NODE(next); GENERIC_NODE(lhs); GENERIC_NODE(rhs); GENERIC_NODE(cond); GENERIC_NODE(then); GENERIC_NODE(els);
    GENERIC_NODE(init); GENERIC_NODE(step); GENERIC_NODE(inc); GENERIC_NODE(for_amount); GENERIC_NODE(end);
#undef GENERIC_NODE
    n->fields = NULL; end = &n->fields;
    for (f = old->fields; f; f = f->next) {
        InputField *copy = generic_alloc(sizeof(*copy)); *copy = *f; copy->next = NULL;
        copy->var = generic_node(f->var, map); copy->temp = generic_var(*map, f->temp);
        *end = copy; end = &copy->next;
    }
    return n;
}
static Function *specialize_generic_call(Node *call)
{
    Function *origin = call->func, *fn, **tail;
    GenericSpec *spec;
    GenericCopy *map = NULL, *item;
    Type *types = generic_alloc((size_t)origin->param_count * sizeof(*types));
    Var *p, *v, *old, **end; Node *arg; int i;
    for (p = origin->params, arg = call->lhs, i = 0; p; p = p->next, arg = arg->next, i++) {
        if (!p->is_inferred) continue;
        if (p->is_ref != (arg->kind == ND_REFARG))
            error_tok(arg->tok, "%d 番目の推論する引数は %s渡しです。呼び出し側の ref を確認してください", i + 1, p->is_ref ? "ref " : "値");
        { Node *value = p->is_ref ? arg->lhs : arg;
          check_expr(value);
          concrete_only(value->ty, value->tok);
          if (value->ty == TY_NONE || value->ty == TY_VOID || array_base(value->ty) == TY_ADDR)
              error_tok(value->tok, "推論する引数には具体的な値型が必要です（addr・void は使用できません）");
          types[i] = value->ty;
        }
    }
    for (spec = generic_specs; spec; spec = spec->next)
        if (spec->origin == origin && !memcmp(spec->types, types, (size_t)origin->param_count * sizeof(*types))) {
            free(types); return spec->fn;
        }
    if (++generic_spec_count > 4096)
        error_tok(call->tok, "関数の具体化はプログラム全体で4096件までです（展開元: %.*s）", origin->name_len, origin->name);
    fn = generic_alloc(sizeof(*fn)); *fn = *origin; fn->next = NULL;
    fn->is_generic = 0; fn->is_template = 0; fn->generic_origin = origin;
    fn->generic_instances = 0; fn->check_state = 0; fn->label = 0;
    spec = generic_alloc(sizeof(*spec)); spec->origin = origin; spec->fn = fn; spec->types = types;
    spec->next = generic_specs; generic_specs = spec; origin->generic_instances++;
    fn->params = NULL; end = &fn->params;
    for (old = origin->params, i = 0; old; old = old->next, i++) {
        v = generic_alloc(sizeof(*v)); *v = *old; v->next = NULL;
        if (types[i]) v->ty = types[i];
        if (type_is_interface(v->ty)) fn->is_template = 1;
        generic_register(&map, old, v); *end = v; end = &v->next;
    }
    fn->locals = NULL; end = &fn->locals;
    for (old = origin->locals; old; old = old->next) {
        v = generic_alloc(sizeof(*v)); *v = *old; v->next = NULL;
        generic_register(&map, old, v); *end = v; end = &v->next;
    }
    for (old = origin->params; old; old = old->next) {
        v = generic_var(map, old); v->vnext = generic_var(map, old->vnext); v->dims = generic_dims(old->dims, &map);
    }
    for (old = origin->locals; old; old = old->next) {
        v = generic_var(map, old); v->vnext = generic_var(map, old->vnext); v->dims = generic_dims(old->dims, &map);
        v->decl = generic_node(old->decl, &map); v->const_val = generic_node(old->const_val, &map);
    }
    fn->return_dims = generic_dims(origin->return_dims, &map);
    fn->body = generic_node(origin->body, &map);
    while (map) { item = map; map = item->next; free(item); }
    tail = &cur_ast->funcs; while (*tail) tail = &(*tail)->next; *tail = fn;
    return fn;
}
