/* Permissions belong to paths, not nominal model types. Copies of owned data
 * are independent, whereas file handles retain their shared resource identity. */
static int contains_file(Type ty)
{
    Model *m; ModelField *f;
    if (type_is_array(ty)) return contains_file(array_element(ty));
    if (ty == TY_FILE) return 1;
    if (!type_is_model(ty)) return 0;
    m = type_model(ty);
    if (m->file_state) return m->file_state != 2;
    m->file_state = 1;
    for (f = m->fields; f; f = f->next)
        if (contains_file(f->ty)) { m->file_state = 3; return 1; }
    m->file_state = 2;
    return 0;
}

static Node *place_root(Node *n)
{
    while (n && (n->kind == ND_INDEX || n->kind == ND_FIELD)) n = n->lhs;
    return n;
}

static int readonly_place(Node *n)
{
    n = place_root(n);
    return n && n->kind == ND_VAR && (n->var->is_const || n->var->is_readonly);
}

static void check_value_transfer(Node *n)
{
    if (contains_file(n->ty) && readonly_place(n) && place_root(n)->var->is_readonly)
        error_tok(n->tok, "readonly の file または file を含む値を変更可能な値としてコピーできません");
}

static Var *hidden_local(Type ty, ArrayDim *dims, int readonly, Token *tok)
{
    Var *v, *other; int offset = 0;
    if (!cur_func) error_tok(tok, "const の初期値にはメソッド・通常関数呼び出しを使えません");
    v = calloc(1, sizeof(*v));
    if (!v) fatal("一時値を確保できません");
    for (other = cur_func->locals; other; other = other->next)
        if (other->offset > offset) offset = other->offset;
    v->name = ""; v->ty = ty; v->dims = dims; v->tok = tok;
    v->is_readonly = readonly; v->offset = offset + 8;
    v->next = cur_func->locals; cur_func->locals = v;
    cur_func->stack_size = (v->offset + 15) / 16 * 16;
    return v;
}

static Node *clone_ref_path(Node *n, Var *root)
{
    Node *copy = calloc(1, sizeof(*copy));
    if (!copy) fatal("読み取り専用の参照を確保できません");
    *copy = *n;
    if (n->kind == ND_INDEX) copy->lhs = clone_ref_path(n->lhs, root);
    else copy->var = root;
    return copy;
}

static void materialize_const_ref(Node *arg, Var *root)
{
    Node *source = place_root(arg->lhs);
    Var *v = hidden_local(root->ty, root->dims, 1, arg->tok);
    arg->rhs = type_is_array(root->ty) ? copy_array(source, root->dims, 1) : source;
    arg->ref_temp = v;
    arg->lhs = clone_ref_path(arg->lhs, v);
}
