/* Definite completion is checked after typing/lowering, before file analysis.
 * Only the constructor's own this can contain empty aggregate slots. */
typedef struct { unsigned char *ready; int reachable; } InitState;
static Model *completion_model;
static Var *completion_self;
static InitState init_state_copy(InitState s)
{
    InitState copy = s;
    copy.ready = calloc((size_t)completion_model->field_count + 1, 1);
    if (!copy.ready) fatal("初期化状態を確保できません");
    memcpy(copy.ready, s.ready, (size_t)completion_model->field_count);
    return copy;
}
static void require_complete(InitState s, Token *tok)
{
    ModelField *f;
    for (f = completion_model->fields; f; f = f->next)
        if (!s.ready[f->index]) error_tok(tok, "未完了フィールド %s を初期化してから this 全体を使用・成功 return してください（宣言: %s:%d）", f->name, f->tok->source->filename, f->tok->line);
}
static ModelField *own_init_field(Node *n)
{
    if (n && n->kind == ND_INDEX && n->field && n->lhs->kind == ND_VAR && n->lhs->var == completion_self)
        return n->field;
    return NULL;
}
static void init_expression(Node *n, InitState s, int whole_write)
{
    Node *item; InputField *input; ModelField *f;
    if (!n) return;
    f = own_init_field(n);
    if (f) {
        if (!s.ready[f->index] && !whole_write) error_tok(n->tok, "未完了フィールド %s は全体への初回代入以外に使用できません", f->name);
        return;
    }
    if (n->kind == ND_ADDR && n->lhs->kind == ND_VAR && n->lhs->var == completion_self)
        error_tok(n->tok, "init 内では this.addr を取り出せません");
    if (n->kind == ND_VAR && n->var == completion_self) { require_complete(s, n->tok); return; }
    for (item = n->lhs; item; item = item->next) init_expression(item, s, 0);
    for (item = n->rhs; item; item = item->next) init_expression(item, s, 0);
    for (item = n->end; item; item = item->next) init_expression(item, s, 0);
    for (input = n->fields; input; input = input->next) init_expression(input->var, s, 0);
}
static InitState init_statements(Node *list, InitState s, Function *fn)
{
    Node *n;
    for (n = list; n; n = n->next) {
        if (n->kind == ND_IF) {
            InitState a = init_state_copy(s), b = init_state_copy(s); int i;
            init_expression(n->cond, s, 0);
            if (is_const_expr(n->cond)) { if (eval_const(n->cond).i) b.reachable = 0; else a.reachable = 0; }
            a = init_statements(n->then, a, fn); b = init_statements(n->els, b, fn);
            if (a.reachable && b.reachable) for (i = 0; i < completion_model->field_count; i++) s.ready[i] = a.ready[i] && b.ready[i];
            else if (a.reachable) memcpy(s.ready, a.ready, (size_t)completion_model->field_count);
            else if (b.reachable) memcpy(s.ready, b.ready, (size_t)completion_model->field_count);
            s.reachable = a.reachable || b.reachable;
            free(a.ready); free(b.ready);
        } else if (n->kind == ND_FOR || n->kind == ND_WHILE) {
            InitState body = init_state_copy(s);
            if (n->init) init_expression(n->init->lhs, s, 0);
            init_expression(n->cond, s, 0);
            body = init_statements(n->then, body, fn); free(body.ready);
            if (n->step) init_expression(n->step->lhs, s, 0);
        } else if (n->kind == ND_STORE) {
            ModelField *f = own_init_field(n->lhs);
            int whole = f && n->val == UPDATE_SET;
            init_expression(n->lhs, s, whole); init_expression(n->rhs, s, 0);
            if (whole) {
                if (f->pending) {
                    n->init_store = 1;
                    if (!s.ready[f->index] && type_is_array(f->ty)) check_padding(n->rhs, f->ty, f->dims);
                }
                s.ready[f->index] = 1;
            }
        } else if (n->kind == ND_RETURN) {
            init_expression(n->lhs, s, 0);
            if (fn->ret == TY_VOID || !is_const_expr(n->lhs) || eval_const(n->lhs).i) require_complete(s, n->tok);
            s.reachable = 0;
        } else {
            if (n->kind == ND_ASSIGN && n->var == completion_self) error_tok(n->tok, "init 内で this 全体には代入できません");
            init_expression(n->kind == ND_EXPR_STMT ? n->lhs : n, s, 0);
        }
    }
    return s;
}
static void check_initializer_completion(Function *fn)
{
    ModelField *f; InitState s;
    completion_model = fn->model; completion_self = fn->params;
    s.ready = calloc((size_t)completion_model->field_count + 1, 1); s.reachable = 1;
    if (!s.ready) fatal("初期化状態を確保できません");
    for (f = completion_model->fields; f; f = f->next) s.ready[f->index] = !f->pending;
    s = init_statements(fn->body, s, fn);
    if (s.reachable) require_complete(s, fn->tok);
    free(s.ready);
}
