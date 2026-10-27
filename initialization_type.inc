/* Construction has a separate entry from copying. Default recipes are typed
 * once in their source scope and executed anew at each construction site. */
static Node *init_node(NodeKind kind, Type ty, Token *tok)
{
    Node *n = calloc(1, sizeof(*n));
    if (!n) fatal("初期化の構文木を確保できません");
    n->kind = kind; n->ty = ty; n->tok = tok; return n;
}
static Node *init_var(Var *v)
{
    Node *n = init_node(ND_VAR, v->ty, v->tok); n->var = v; return n;
}
static int contains_initializer(Type ty)
{
    Model *m; ModelField *f;
    if (type_is_array(ty)) return contains_initializer(array_element(ty));
    if (!type_is_model(ty)) return 0;
    m = type_model(ty);
    if (m->init_state) return m->init_state != 2;
    m->init_state = 1;
    if (m->initializer) { m->init_state = 3; return 1; }
    for (f = m->fields; f; f = f->next)
        if (contains_initializer(f->ty)) { m->init_state = 3; return 1; }
    m->init_state = 2; return 0;
}
static int defaultable(Type ty, ArrayDim *dims)
{
    Var *p; ModelField *f; Model *m;
    if (type_is_array(ty)) {
        if (!dims || dims->limit <= 0) return 1;
        return defaultable(array_element(ty), dims->next);
    }
    if (!type_is_model(ty)) return 1;
    m = type_model(ty);
    if (m->initializer) {
        for (p = m->initializer->params->next; p; p = p->next) if (!p->default_arg) return 0;
        return 1;
    }
    for (f = m->fields; f; f = f->next) if (!f->initial && !defaultable(f->ty, f->dims)) return 0;
    return 1;
}
static void require_default(Type ty, ArrayDim *dims, Token *tok)
{
    if (!defaultable(ty, dims)) error_tok(tok, "%s の init には必須引数が必要です。要素またはフィールド全体の初期値を指定してください", type_name(ty));
}
static void check_array_capacity(Node *source, Type ty, ArrayDim *dims)
{
    Node *item;
    if (!type_is_array(ty) || !dims) return;
    while (source && source->kind == ND_ARRAY_COPY) source = source->lhs;
    if (!source || source->kind != ND_ARRAY_LITERAL) return;
    if (dims->limit >= 0 && source->nargs > dims->limit) error_tok(source->tok, "配列の要素数が宣言された上限を超えています");
    if (type_is_array(array_element(ty)))
        for (item = source->lhs; item; item = item->next) check_array_capacity(item, array_element(ty), dims->next);
}
static void check_padding(Node *source, Type ty, ArrayDim *dims)
{
    Node *item; long long length = -1, desired;
    if (!type_is_array(ty)) return;
    if (defaultable(array_base(ty), NULL)) return;
    while (source && source->kind == ND_ARRAY_COPY) source = source->lhs;
    if (source && source->kind == ND_ARRAY_LITERAL) length = source->nargs;
    desired = dims ? dims->limit : -1;
    if (desired > 0 && (length < 0 || length < desired) && !defaultable(array_element(ty), dims->next))
        error_tok(source->tok, "init の必須引数があるため、配列の不足分を既定生成できません");
    if (type_is_array(array_element(ty))) {
        if (length < 0 && !defaultable(array_element(ty), dims ? dims->next : NULL))
            error_tok(source->tok, "配列の不足分に必要な init の必須引数を確認できません");
        if (source && source->kind == ND_ARRAY_LITERAL)
            for (item = source->lhs; item; item = item->next) check_padding(item, array_element(ty), dims ? dims->next : NULL);
    }
}
static Function *new_builder(Ast *ast, Model *m, const char *name)
{
    Function *fn = calloc(1, sizeof(*fn)); Var *ctx = calloc(1, sizeof(*ctx)); Function **end = &ast->funcs;
    if (!fn || !ctx) fatal("生成関数を確保できません");
    fn->model = m; fn->is_internal = 1; fn->name = name; fn->name_len = (int)strlen(name);
    fn->tok = m->tok; fn->ret = m->ty; fn->params = ctx; fn->param_count = 1;
    ctx->name = ""; ctx->tok = m->tok; ctx->ty = TY_ADDR; ctx->is_param = 1;
    fn->creation_context = ctx;
    while (*end) end = &(*end)->next;
    *end = fn;
    return fn;
}
static void builder_offsets(Function *fn)
{
    Var *p; int i = 0;
    for (p = fn->params; p; p = p->next, i++) p->offset = -(16 + 8 * (fn->param_count - 1 - i));
}
static void prepare_builders(Ast *ast)
{
    Model *m;
    for (m = ast->models; m; m = m->next) {
        Var *p, **end; int index = 1;
        m->builder = new_builder(ast, m, ".build"); m->default_builder = new_builder(ast, m, ".default");
        end = &m->builder->params->next;
        for (p = m->initializer ? m->initializer->params->next : NULL; p; p = p->next) {
            Var *copy = calloc(1, sizeof(*copy)); if (!copy) fatal("生成引数を確保できません");
            *copy = *p; copy->next = copy->vnext = NULL; copy->parameter_index = index++;
            *end = copy; end = &copy->next; m->builder->param_count++;
        }
        builder_offsets(m->builder); builder_offsets(m->default_builder);
    }
}
static void check_initializer_defaults(Model *m)
{
    Var *p, *copy;
    if (!m->initializer || m->recipe_state == 2) return;
    if (m->recipe_state == 1) error_tok(m->initializer->tok, "init の既定生成が循環しています");
    m->recipe_state = 1;
    copy = m->builder->params->next;
    for (p = m->initializer->params->next; p; p = p->next, copy = copy->next) {
        if (contains_file(p->ty)) error_tok(p->tok, "init の引数に file を含む型は指定できません");
        check_dims(p->dims);
        if (!p->default_arg) continue;
        check_expected(p->default_arg, p->ty);
        p->default_arg = coerce(p->default_arg, p->ty, p->tok, "init の既定引数");
        if (!model_constant_default(p->default_arg)) error_tok(p->tok, "init の既定引数は定数の入力を持つ生成式にしてください");
        copy->default_arg = p->default_arg;
    }
    m->recipe_state = 2;
}
static void check_construction(Node *node, Model *m)
{
    Var *p; Node *item, *before, **end;
    check_initializer_defaults(m);
    for (item = node->lhs; item; item = item->next) {
        for (p = m->builder->params->next; p; p = p->next)
            if (same_name(p->name, p->name_len, item->name, (int)strlen(item->name))) break;
        if (!p) error_tok(item->tok, "%s.init に引数 %s はありません", m->name, item->name);
        for (before = node->lhs; before != item; before = before->next)
            if (before->var == p) error_tok(item->tok, "init 引数 %s が重複しています", item->name);
        item->var = p; check_expected(item->lhs, p->ty); check_value_transfer(item->lhs);
        item->lhs = coerce(item->lhs, p->ty, item->tok, "init の引数");
        if (type_is_model(p->ty)) item->lhs = copy_model(item->lhs);
        if (type_is_array(p->ty)) item->lhs = copy_array(item->lhs, p->dims, 0);
    }
    end = &node->lhs; while (*end) end = &(*end)->next;
    for (p = m->builder->params->next; p; p = p->next) {
        for (item = node->lhs; item && item->var != p; item = item->next) { }
        if (item) continue;
        if (!p->default_arg) error_tok(node->tok, "%s.init の必須引数 %.*s がありません", m->name, p->name_len, p->name);
        item = init_node(ND_FIELD, TY_NONE, p->tok); item->var = p; item->lhs = p->default_arg;
        if (type_is_model(p->ty)) item->lhs = copy_model(item->lhs);
        if (type_is_array(p->ty)) item->lhs = copy_array(item->lhs, p->dims, 0);
        *end = item; end = &item->next;
    }
    node->kind = ND_CONSTRUCT; node->ty = m->ty; node->func = m->builder;
}
static Node *model_seed(Model *m)
{
    ModelField *f; Node *n = init_node(ND_MODEL_SEED, m->ty, m->tok), **end = &n->lhs;
    for (f = m->fields; f; f = f->next) {
        Node *item = init_node(ND_FIELD, TY_NONE, f->tok); item->field = f;
        if (f->pending) item->lhs = init_node(ND_NUM, f->ty, f->tok);
        else {
            item->lhs = f->initial ? f->initial : model_default(f->ty, f->dims, f->tok);
            if (f->initial && type_is_array(f->ty)) item->lhs = copy_array(item->lhs, f->dims, 1);
            if (f->initial && type_is_model(f->ty)) item->lhs = copy_model(item->lhs);
        }
        *end = item; end = &item->next;
    }
    return n;
}
static void finish_builders(Ast *ast)
{
    Model *m;
    for (m = ast->models; m; m = m->next) {
        Function *fn = m->builder; Var *p, *object = calloc(1, sizeof(*object));
        Node *decl, *ret, *body_call = NULL;
        if (!object) fatal("生成先を確保できません");
        object->name = ""; object->tok = m->tok; object->ty = m->ty; object->offset = 8;
        fn->locals = object; fn->stack_size = 16;
        m->default_value = model_seed(m);
        decl = init_node(ND_DECL, TY_NONE, m->tok); decl->var = object; decl->lhs = m->default_value;
        ret = init_node(ND_RETURN, TY_NONE, m->tok); ret->lhs = init_var(object);
        if (m->initializer) {
            Node *call = init_node(ND_CALL, m->initializer->ret, m->initializer->tok), *self, **end;
            call->func = m->initializer; call->nargs = m->initializer->param_count;
            self = init_node(ND_REFARG, TY_ADDR, m->tok); self->lhs = init_var(object); call->lhs = self;
            end = &self->next;
            for (p = fn->params->next; p; p = p->next) { *end = init_var(p); end = &(*end)->next; }
            if (call->ty == TY_BOOL) {
                body_call = init_node(ND_IF, TY_NONE, m->tok);
                body_call->cond = init_node(ND_NOT, TY_BOOL, m->tok); body_call->cond->lhs = call;
                body_call->then = init_node(ND_BUILD_REJECT, TY_NONE, m->tok); body_call->then->val = m->ty;
            } else { body_call = init_node(ND_EXPR_STMT, TY_NONE, m->tok); body_call->lhs = call; }
            decl->next = body_call; body_call->next = ret;
        } else decl->next = ret;
        fn->body = decl;
        if (defaultable(m->ty, NULL)) {
            Node *value = init_node(ND_CONSTRUCT, m->ty, m->tok);
            if (m->initializer) check_construction(value, m); else value->func = m->builder;
            m->default_builder->body = init_node(ND_RETURN, TY_NONE, m->tok);
            m->default_builder->body->lhs = value;
        } else {
            m->default_builder->body = init_node(ND_BUILD_REJECT, TY_NONE, m->tok);
            m->default_builder->body->val = m->ty;
        }
    }
}
static void visit_default_dependencies(Model *m);
static int array_may_fill(Node *source, Type ty, ArrayDim *dims)
{
    Node *item; long long length = -1, count;
    if (!type_is_array(ty)) return 0;
    while (source && source->kind == ND_ARRAY_COPY) source = source->lhs;
    if (source && source->kind == ND_ARRAY_LITERAL) length = source->nargs;
    if (dims && dims->limit == 0) return 0;
    if (dims && dims->limit > 0 && (length < 0 || length < dims->limit)) {
        if (type_is_model(array_element(ty))) return 1;
        return array_may_fill(NULL, array_element(ty), dims->next);
    }
    if (!type_is_array(array_element(ty))) return 0;
    count = length;
    if (count < 0) return array_may_fill(NULL, array_element(ty), dims ? dims->next : NULL);
    for (item = source ? source->lhs : NULL; item; item = item->next)
        if (array_may_fill(item, array_element(ty), dims ? dims->next : NULL)) return 1;
    return 0;
}
static void recipe_dependencies(Node *n)
{
    Node *item;
    if (!n) return;
    if (n->kind == ND_MODEL_NEW || n->kind == ND_CONSTRUCT) visit_default_dependencies(type_model(n->ty));
    if ((n->kind == ND_ARRAY_NEW || (n->kind == ND_ARRAY_COPY && n->val)) && type_is_model(array_base(n->ty)) &&
        array_may_fill(n->kind == ND_ARRAY_NEW ? NULL : n->lhs, n->ty, n->dims))
        visit_default_dependencies(type_model(array_base(n->ty)));
    for (item = n->lhs; item; item = item->next) recipe_dependencies(item);
    for (item = n->rhs; item; item = item->next) recipe_dependencies(item);
}
static void visit_default_dependencies(Model *m)
{
    Var *p;
    if (m->dependency_state == 2) return;
    if (m->dependency_state == 1) error_tok(m->tok, "init とフィールド既定値の生成が循環しています: %s", m->name);
    m->dependency_state = 1;
    recipe_dependencies(m->default_value);
    for (p = m->initializer ? m->initializer->params->next : NULL; p; p = p->next) recipe_dependencies(p->default_arg);
    m->dependency_state = 2;
}
