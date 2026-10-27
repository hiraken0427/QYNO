/* Clone already contract-checked templates, preserving expression sharing,
 * lexical permissions and all hidden locals. Never recheck with extra access. */
typedef struct InterfaceClone InterfaceClone;
struct InterfaceClone { InterfaceClone *next; const void *old; void *copy; };
typedef struct InterfaceSpec InterfaceSpec;
struct InterfaceSpec { InterfaceSpec *next; Function *origin, *fn; Type *types; };
static InterfaceSpec *interface_specs;
static int interface_spec_count;
static void *interface_alloc(size_t size)
{
    void *p = calloc(1, size); if (!p) fatal("interface の特殊化を確保できません"); return p;
}
static void *clone_lookup(InterfaceClone *map, const void *old)
{
    for (; map; map = map->next) if (map->old == old) return map->copy;
    return NULL;
}
static void clone_register(InterfaceClone **map, const void *old, void *copy)
{
    InterfaceClone *item = interface_alloc(sizeof(*item)); item->old = old; item->copy = copy; item->next = *map; *map = item;
}
static Var *clone_var_use(InterfaceClone *map, Var *old)
{
    Var *copy = clone_lookup(map, old); return copy ? copy : old;
}
static Node *clone_interface_node(Node *old, InterfaceClone **map)
{
    Node *n; InputField *f, **end;
    if (!old) return NULL;
    n = clone_lookup(*map, old); if (n) return n;
    n = interface_alloc(sizeof(*n)); *n = *old; clone_register(map, old, n);
    n->var = clone_var_use(*map, old->var); n->ref_temp = clone_var_use(*map, old->ref_temp);
    n->in_line = clone_var_use(*map, old->in_line); n->in_cur = clone_var_use(*map, old->in_cur); n->in_end = clone_var_use(*map, old->in_end);
#define CLONE_NODE(member) n->member = clone_interface_node(old->member, map)
    CLONE_NODE(next); CLONE_NODE(lhs); CLONE_NODE(rhs); CLONE_NODE(cond); CLONE_NODE(then); CLONE_NODE(els);
    CLONE_NODE(init); CLONE_NODE(step); CLONE_NODE(inc); CLONE_NODE(for_amount); CLONE_NODE(end);
#undef CLONE_NODE
    n->fields = NULL; end = &n->fields;
    for (f = old->fields; f; f = f->next) {
        InputField *copy = interface_alloc(sizeof(*copy)); *copy = *f; copy->next = NULL;
        copy->var = clone_interface_node(f->var, map); copy->temp = clone_var_use(*map, f->temp);
        *end = copy; end = &copy->next;
    }
    if (n->kind == ND_VAR) n->ty = n->var->ty;
    if (n->kind == ND_REFARG) n->cmp_ty = n->lhs->ty;
    return n;
}
static Function *specialize_interface_call(Ast *ast, Node *call)
{
    Function *origin = call->func, *fn; InterfaceSpec *spec; Var *p, *v, **end; Node *arg;
    Type *types = interface_alloc((size_t)origin->param_count * sizeof(*types)); int i;
    for (p = origin->params, arg = call->lhs, i = 0; p; p = p->next, arg = arg->next, i++)
        if (type_is_interface(p->ty)) {
            types[i] = arg->lhs->ty;
            if (!type_is_model(types[i])) error_tok(call->tok, "interface の特殊化に具体的なモデル型が必要です");
        }
    for (spec = interface_specs; spec; spec = spec->next)
        if (spec->origin == origin && !memcmp(spec->types, types, (size_t)origin->param_count * sizeof(*types))) { free(types); return spec->fn; }
    if (++interface_spec_count > 4096) {
        for (p = origin->params, i = 0; p; p = p->next, i++) if (types[i])
            fprintf(stderr, "  interface 引数 %.*s: %s\n", p->name_len, p->name, type_name(types[i]));
        error_tok(call->tok, "interface の特殊化はプログラム全体で4096件までです（展開元: %.*s）", origin->name_len, origin->name);
    }
    fn = interface_alloc(sizeof(*fn)); *fn = *origin; fn->next = NULL;
    fn->is_template = 0; fn->template_origin = origin; fn->label = 0;
    spec = interface_alloc(sizeof(*spec)); spec->origin = origin; spec->fn = fn; spec->types = types;
    spec->next = interface_specs; interface_specs = spec;
    { InterfaceClone *map = NULL, *item; Var *old;
      fn->params = NULL; end = &fn->params;
      for (old = origin->params, i = 0; old; old = old->next, i++) {
          v = interface_alloc(sizeof(*v)); *v = *old; v->next = NULL; v->vnext = NULL;
          if (types[i]) v->ty = types[i];
          clone_register(&map, old, v); *end = v; end = &v->next;
      }
      fn->locals = NULL; end = &fn->locals;
      for (old = origin->locals; old; old = old->next) {
          v = interface_alloc(sizeof(*v)); *v = *old; v->next = NULL; v->vnext = NULL;
          clone_register(&map, old, v); *end = v; end = &v->next;
      }
      fn->body = clone_interface_node(origin->body, &map);
      for (old = origin->locals; old; old = old->next) {
          v = clone_var_use(map, old); v->decl = clone_interface_node(old->decl, &map); v->const_val = clone_interface_node(old->const_val, &map);
      }
      while (map) { item = map; map = item->next; free(item); }
    }
    { Function **tail = &ast->funcs; while (*tail) tail = &(*tail)->next; *tail = fn; }
    return fn;
}
static void resolve_interface_calls(Ast *ast, Node *n)
{
    InputField *f;
    if (!n) return;
    resolve_interface_calls(ast, n->lhs); resolve_interface_calls(ast, n->rhs); resolve_interface_calls(ast, n->cond);
    resolve_interface_calls(ast, n->then); resolve_interface_calls(ast, n->els); resolve_interface_calls(ast, n->init);
    resolve_interface_calls(ast, n->step); resolve_interface_calls(ast, n->inc); resolve_interface_calls(ast, n->for_amount); resolve_interface_calls(ast, n->end);
    for (f = n->fields; f; f = f->next) resolve_interface_calls(ast, f->var);
    if (n->kind == ND_CALL) {
        if (n->func->contract) {
            Function *actual;
            if (!type_is_model(n->lhs->lhs->ty)) error_tok(n->tok, "interface メソッドの具体型がありません");
            actual = interface_implementation(type_model(n->lhs->lhs->ty), n->func);
            if (!actual) error_tok(n->tok, "interface の実装メソッドがありません");
            n->func = actual;
        }
        if (n->func->is_template) n->func = specialize_interface_call(ast, n);
    }
    resolve_interface_calls(ast, n->next);
}
static void specialize_interfaces(Ast *ast)
{
    Function *fn;
    interface_specs = NULL; interface_spec_count = 0;
    for (fn = ast->funcs; fn; fn = fn->next) if (!fn->is_template) resolve_interface_calls(ast, fn->body);
}
