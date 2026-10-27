/* Model values are nominal, copied aggregates. Field accesses lower to the
 * existing checked slot path, keeping ref borrowing and single evaluation. */
static void check_call(Node *node);
static void check_model_init(Node *node, Type expected);
static Node *copy_model(Node *source)
{
    Node *copy = calloc(1, sizeof(*copy));
    if (!copy) fatal("モデルのコピーを確保できません");
    copy->kind = ND_MODEL_COPY; copy->lhs = source; copy->ty = source->ty; copy->tok = source->tok;
    copy->access_model = source->access_model;
    return copy;
}
static Node *model_default(Type ty, ArrayDim *dims, Token *tok)
{
    Node *n = calloc(1, sizeof(*n));
    if (!n) fatal("モデルの初期値を確保できません");
    n->tok = tok; n->ty = ty;
    if (type_is_model(ty)) n->kind = ND_MODEL_NEW;
    else if (type_is_array(ty)) { n->kind = ND_ARRAY_NEW; n->dims = dims; }
    else if (type_is_text(ty)) { n->kind = ND_STRLIT; n->str = ""; n->str_len = ty == TY_CHAR; }
    else if (ty == TY_FLOAT || ty == TY_DOUBLE) n->kind = ND_FNUM;
    else n->kind = ND_NUM;
    return n;
}
static ModelField *find_model_field(Model *m, const char *name)
{
    ModelField *f;
    for (f = m->fields; f; f = f->next) if (strcmp(f->name, name) == 0) return f;
    return NULL;
}
/* Use source ownership, never the currently checked function: default ASTs
 * are cached and reused at construction sites outside their declaring model. */
static void check_member_access(Node *use, Model *owner, int is_private,
                                const char *name, Token *declaration)
{
    if (is_private && use->access_model != owner)
        error_tok(use->tok, "%s.%.*s は private です。この位置からはアクセスできません（宣言: %s:%d:%d）",
                  owner->name, declaration->len, name, declaration->source->filename, declaration->line, declaration->col);
}
static void check_model_init(Node *node, Type expected)
{
    Node *item, **end; ModelField *f; Model *m;
    if (!type_is_model(expected)) error_tok(node->tok, "{フィールド=値} にはモデル型の文脈が必要です");
    /* Inferred arrays revisit their typed elements. Do not reinterpret the
     * omitted defaults appended below as explicit user-written field names. */
    if (node->ty != TY_NONE) {
        if (node->ty != expected) error_tok(node->tok, "モデルの初期化先は同じ型にしてください");
        return;
    }
    m = type_model(expected);
    for (item = node->lhs; item; item = item->next) {
        Node *before;
        f = find_model_field(m, item->name);
        if (!f) error_tok(item->tok, "モデル %s にフィールド %s はありません", m->name, item->name);
        for (before = node->lhs; before != item; before = before->next)
            if (before->field == f) error_tok(item->tok, "フィールド %s の初期化が重複しています", f->name);
        check_member_access(item, f->model, f->is_private, f->name, f->tok);
        item->field = f;
        check_expected(item->lhs, f->ty);
        check_value_transfer(item->lhs);
        item->lhs = coerce(item->lhs, f->ty, item->tok, "フィールドの初期値");
        if (type_is_array(f->ty)) item->lhs = copy_array(item->lhs, f->dims, 1);
        if (type_is_model(f->ty)) item->lhs = copy_model(item->lhs);
    }
    end = &node->lhs; while (*end) end = &(*end)->next;
    for (f = m->fields; f; f = f->next) {
        for (item = node->lhs; item && item->field != f; item = item->next) { }
        if (item) continue;
        item = calloc(1, sizeof(*item)); if (!item) fatal("フィールドの初期値を確保できません");
        item->kind = ND_FIELD; item->field = f; item->tok = f->tok; item->name = f->name;
        item->lhs = f->initial ? f->initial : model_default(f->ty, f->dims, f->tok);
        if (f->initial && type_is_model(f->ty)) item->lhs = copy_model(item->lhs);
        if (f->initial && type_is_array(f->ty)) item->lhs = copy_array(item->lhs, f->dims, 1);
        *end = item; end = &item->next;
    }
    node->ty = expected;
}
static void check_model_field(Node *node)
{
    ModelField *f; Node *index;
    check_expr(node->lhs);
    if (!type_is_model(node->lhs->ty)) error_tok(node->tok, "%s にフィールド %s はありません", type_name(node->lhs->ty), node->name);
    f = find_model_field(type_model(node->lhs->ty), node->name);
    if (!f) error_tok(node->tok, "モデル %s にフィールド %s はありません", type_name(node->lhs->ty), node->name);
    check_member_access(node, f->model, f->is_private, f->name, f->tok);
    index = calloc(1, sizeof(*index)); if (!index) fatal("フィールド番号を確保できません");
    index->kind = ND_NUM; index->ty = TY_INT; index->val = f->index; index->tok = node->tok;
    node->kind = ND_INDEX; node->rhs = index; node->field = f; node->ty = f->ty;
}
static void check_model_method(Node *node)
{
    Function *fn; Node *self; int allow = allow_void;
    allow_void = 0; check_expr(node->lhs); allow_void = allow;
    if (!type_is_model(node->lhs->ty)) error_tok(node->tok, "%s にはこのメソッドはありません", type_name(node->lhs->ty));
    for (fn = cur_ast->funcs; fn; fn = fn->next)
        if (fn->model == type_model(node->lhs->ty) && same_name(fn->name, fn->name_len, node->tok->loc, node->tok->len)) break;
    if (!fn) error_tok(node->tok, "モデル %s にメソッド %.*s はありません", type_name(node->lhs->ty), node->tok->len, node->tok->loc);
    check_member_access(node, fn->model, fn->is_private, fn->name, fn->tok);
    if (readonly_place(node->lhs) && !fn->is_readonly)
        error_tok(node->tok, "const / readonly の %s から呼べません。このメソッドは readonly ではありません", fn->model->name);
    self = calloc(1, sizeof(*self)); if (!self) fatal("メソッドの対象を確保できません");
    { Node *root = node->lhs;
      while (root->kind == ND_INDEX) root = root->lhs;
      if (root->kind != ND_VAR) {
          Var *v, *other; Node *temp; int offset = 0;
          if (!cur_func) error_tok(node->tok, "一時モデルのメソッドは関数の中で呼んでください");
          v = calloc(1, sizeof(*v)); temp = calloc(1, sizeof(*temp));
          if (!v || !temp) fatal("一時モデルを確保できません");
          for (other = cur_func->locals; other; other = other->next) if (other->offset > offset) offset = other->offset;
          v->name = ""; v->ty = node->lhs->ty; v->tok = node->tok; v->offset = offset + 8;
          v->next = cur_func->locals; cur_func->locals = v;
          cur_func->stack_size = (v->offset + 15) / 16 * 16;
          temp->kind = ND_VAR; temp->var = v; temp->tok = node->tok;
          self->rhs = node->lhs; node->lhs = temp;
      }
    }
    self->kind = ND_REFARG; self->tok = node->tok; self->lhs = node->lhs; self->next = node->rhs;
    node->kind = ND_CALL; node->func = fn; node->lhs = self; node->rhs = NULL; node->nargs++;
    check_call(node);
}
static int model_constant_default(Node *n)
{
    Node *item;
    if (n->kind == ND_MODEL_COPY || n->kind == ND_ARRAY_COPY) return model_constant_default(n->lhs);
    if (n->kind == ND_MODEL_INIT || n->kind == ND_ARRAY_LITERAL) {
        for (item = n->lhs; item; item = item->next)
            if (!model_constant_default(n->kind == ND_MODEL_INIT ? item->lhs : item)) return 0;
        return 1;
    }
    return n->kind == ND_MODEL_NEW || n->kind == ND_ARRAY_NEW || is_const_expr(n);
}
static int model_definition_depth;
static void check_model_definition(Model *m)
{
    ModelField *f, *other;
    if (m->state == 2) return;
    if (m->state == 1) error_tok(m->tok, "モデル %s のフィールド定義が循環しています", m->name);
    if (++model_definition_depth > MAX_PLACE_DEPTH) error_tok(m->tok, "モデルの入れ子が深すぎます");
    m->state = 1;
    for (f = m->fields; f; f = f->next) {
        Function *fn;
        for (fn = cur_ast->funcs; fn; fn = fn->next) if (fn->model == m && same_name(fn->name, fn->name_len, f->name, (int)strlen(f->name))) error_tok(f->tok, "フィールドとメソッドの名前 %s が重複しています", f->name);
        for (other = m->fields; other != f; other = other->next)
            if (!strcmp(other->name, f->name)) error_tok(f->tok, "フィールド %s が重複しています", f->name);
        if (type_is_model(array_base(f->ty))) check_model_definition(type_model(array_base(f->ty)));
        check_dims(f->dims);
        if (f->initial) {
            check_expected(f->initial, f->ty);
            f->initial = coerce(f->initial, f->ty, f->tok, "フィールドの既定値");
            if (!model_constant_default(f->initial)) error_tok(f->tok, "フィールドの既定値はコンパイル時に確定する式にしてください");
        }
    }
    m->state = 2; model_definition_depth--;
}
