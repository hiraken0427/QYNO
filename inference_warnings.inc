/* Lexical use is enough: warnings do not claim that a path executes.
 * Limit default warnings to the newly introduced implicit declarations. */
static int similar_local_name(Var *a, Var *b)
{
    int i = 0, n = a->name_len, m = b->name_len;
    const char *x = a->name, *y = b->name;
    if (n < 3 || m < 3 || n - m > 1 || m - n > 1) return 0;
    while (i < n && i < m && x[i] == y[i]) i++;
    if (n == m) {
        if (i == n) return 0;
        if (!memcmp(x + i + 1, y + i + 1, (size_t)(n - i - 1))) return 1;
        return i + 1 < n && x[i] == y[i + 1] && x[i + 1] == y[i] &&
            !memcmp(x + i + 2, y + i + 2, (size_t)(n - i - 2));
    }
    if (n > m) return !memcmp(x + i + 1, y + i, (size_t)(m - i));
    return !memcmp(x + i, y + i + 1, (size_t)(n - i));
}

static void warn_unused_inferred_locals(Ast *ast)
{
    Function *fn;
    Var *v, *candidate;
    for (fn = ast->funcs; fn; fn = fn->next) {
        if (fn->is_internal || fn->template_origin || fn->generic_origin ||
            (fn->is_generic && !fn->generic_instances)) continue;
        for (v = fn->locals; v; v = v->next) {
            if (!v->is_implicit || v->is_used) continue;
            for (candidate = v->vnext; candidate; candidate = candidate->vnext)
                if (similar_local_name(v, candidate)) break;
            if (candidate) {
                warning_tok(v->tok,
                    "型名を省略して宣言した変数 %.*s は未使用です（代入のみ）。既存の変数 %.*s と名前が似ています（%s:%d）",
                    v->name_len, v->name, candidate->name_len, candidate->name,
                    candidate->tok->source->filename, candidate->tok->line);
            } else {
                warning_tok(v->tok, "型名を省略して宣言した変数 %.*s は未使用です（代入のみ）",
                    v->name_len, v->name);
            }
        }
    }
}
