/* Canonical aggregate values retain nominal types and field metadata. They may
 * share immutable compiler nodes; runtime construction still allocates values. */
static int const_eval_depth;
static long long const_model_budget;
static Value eval_const_inner(Node *node);

static Value eval_const(Node *node)
{
    Value v;
    if (const_eval_depth++ == 0) const_model_budget = 1000000;
    v = eval_const_inner(node);
    const_eval_depth--;
    return v;
}

static Value constant_model(Node *node)
{
    Model *m = type_model(node->ty);
    ModelField *f; Node *item, **values; Node head = {0}, *tail = &head;
    Value v = {0};
    v.ty = node->ty; v.len = m->field_count;
    if (node->constant_value) { v.items = node->lhs; return v; }
    if (node->kind == ND_MODEL_NEW) {
        Node initial = {0};
        if (m->constant_default) return eval_const(m->constant_default);
        check_model_definition(m);
        initial.kind = ND_MODEL_INIT; initial.tok = node->tok;
        check_model_init(&initial, m->ty);
        v = constant_model(&initial);
        m->constant_default = value_node(v, node->tok);
        return v;
    }
    if ((const_model_budget -= m->field_count) < 0)
        const_error(node, "const モデル・配列の展開が100万スロットを超えています");
    values = calloc((size_t)m->field_count + 1, sizeof(*values));
    if (!values) fatal("モデル定数を確保できません");
    /* Evaluate in source order, then store the canonical fields in layout order. */
    for (item = node->lhs; item; item = item->next)
        values[item->field->index] = value_node(eval_const(item->lhs), item->tok);
    for (f = m->fields; f; f = f->next) {
        item = calloc(1, sizeof(*item));
        if (!item) fatal("モデル定数のフィールドを確保できません");
        item->kind = ND_FIELD; item->field = f; item->name = f->name; item->tok = f->tok;
        item->lhs = values[f->index]; tail = tail->next = item;
    }
    free(values); v.items = head.next;
    return v;
}

static void constant_size(Node *where, Node *value, long long *budget)
{
    Node *item;
    if (!type_is_array(value->ty) && !type_is_model(value->ty)) return;
    *budget -= value->nargs;
    if (*budget < 0) const_error(where, "const モデル・配列の展開が100万スロットを超えています");
    for (item = value->lhs; item; item = item->next)
        constant_size(where, type_is_model(value->ty) ? item->lhs : item, budget);
}
