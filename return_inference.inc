/* Check a body once. Return expressions retain their original concrete types
 * until every return has contributed; only then insert conversions and copies. */
static Type common_return_type(Type a, Type b, Token *site)
{
    if (a == TY_NONE || a == b) return b;
    if (a == TY_VOID || b == TY_VOID)
        error_tok(site, "値を返す return と値のない return は混在できません");
    if (is_numeric(a) && is_numeric(b)) {
        if (a == TY_DOUBLE || b == TY_DOUBLE) return TY_DOUBLE;
        if (a == TY_FLOAT || b == TY_FLOAT) return TY_FLOAT;
        { Type common = integer_common(a, b); if (common != TY_NONE) return common; }
    }
    if (type_is_text(a) && type_is_text(b)) return TY_STR;
    error_tok(site, "戻り値の型 %s と %s は統一できません。戻り値型または変換を明示してください",
        type_name(a), type_name(b));
    return TY_NONE;
}

static void infer_returns(Node *list, Type *result)
{
    Node *n;
    for (n = list; n; n = n->next) {
        if (n->kind == ND_RETURN)
            *result = common_return_type(*result, n->lhs ? n->lhs->ty : TY_VOID, n->tok);
        else if (n->kind == ND_IF) {
            infer_returns(n->then, result); infer_returns(n->els, result);
        } else if (n->kind == ND_FOR || n->kind == ND_WHILE) infer_returns(n->then, result);
    }
}

static void convert_inferred_returns(Node *list, Function *fn)
{
    Node *n;
    for (n = list; n; n = n->next) {
        if (n->kind == ND_RETURN && n->lhs) {
            n->lhs = coerce(n->lhs, fn->ret, n->lhs->tok, "戻り値");
            if (type_is_array(fn->ret)) n->lhs = copy_array(n->lhs, fn->return_dims, 0);
            if (type_is_model(fn->ret)) n->lhs = copy_model(n->lhs);
        } else if (n->kind == ND_IF) {
            convert_inferred_returns(n->then, fn); convert_inferred_returns(n->els, fn);
        } else if (n->kind == ND_FOR || n->kind == ND_WHILE) convert_inferred_returns(n->then, fn);
    }
}

static void ensure_function(Function *fn, Token *site)
{
    Function *saved_func;
    Node *saved_interface;
    int saved_allow;
    Var *p;
    if (fn->check_state == 2 || fn->is_internal) return;
    if (fn->check_state == 1)
        error_tok(site, "関数 %.*s の戻り値型の推論が循環しています。再帰する関数の戻り値型を明示してください",
            fn->name_len, fn->name);
    if (++function_check_depth > MAX_FUNCTION_DEPTH)
        error_tok(site, "戻り値型の推論の依存は %d 段以下にしてください", MAX_FUNCTION_DEPTH);
    saved_func = cur_func; saved_allow = allow_void;
    saved_interface = allowed_interface_use;
    cur_func = fn; allow_void = 0; allowed_interface_use = NULL; fn->check_state = 1;
    check_dims(fn->return_dims);
    for (p = fn->params; p; p = p->next) check_dims(p->dims);
    check_stmts(fn->body);
    if (fn->infer_return) {
        Type result = TY_NONE;
        ArrayDim **end = &fn->return_dims;
        int rank;
        infer_returns(fn->body, &result);
        fn->ret = result == TY_NONE ? TY_VOID : result;
        for (rank = array_rank(fn->ret); rank; rank--) {
            ArrayDim *dim = calloc(1, sizeof(*dim));
            if (!dim) fatal("推論した戻り値の次元を確保できません");
            dim->limit = -1; dim->checked = 1; dim->tok = fn->tok;
            *end = dim; end = &dim->next;
        }
        convert_inferred_returns(fn->body, fn);
    }
    if (fn->ret != TY_VOID && (fn->model || !same_name(fn->name, fn->name_len, "main", 4)) && !stmts_return(fn->body))
        error_tok(fn->tok, "関数 %.*s には、最後まで進んだときの return がありません（すべての道筋で値を返してください）",
            fn->name_len, fn->name);
    fn->check_state = 2;
    cur_func = saved_func; allow_void = saved_allow; allowed_interface_use = saved_interface; function_check_depth--;
}
