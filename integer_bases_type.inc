/* Conversion arguments remain in source order. option_index only selects the
 * destination slot; it must never reorder side effects. */
static int check_integer_conversion(Node *n)
{
    Node *arg, *base = NULL; int formatting, count = 0, next_slot = 0, named = 0, used[3] = {0};
    if (!n->lhs || n->lhs->arg_name) error_tok(n->tok, "int / str には最初に変換する値が必要です");
    check_expr(n->lhs);
    formatting = n->val == BI_STR;
    if (!(formatting ? type_is_integer(n->lhs->ty) : n->lhs->ty == TY_STR)) return 0;
    for (arg = n->lhs; arg; arg = arg->next) {
        int slot;
        if (arg->arg_name) { slot = arg->option_index; named = 1; }
        else { if (named) error_tok(arg->tok, "位置引数は名前付き引数より先に書いてください"); slot = next_slot++; }
        if (slot >= (formatting ? 3 : 2) || used[slot]) error_tok(arg->tok, "int / str の引数が多すぎるか重複しています");
        used[slot] = 1; arg->option_index = slot; count++;
        check_expr(arg);
        if (slot == 1) {
            if (!type_is_integer(arg->ty)) error_tok(arg->tok, "base は int にしてください");
            base = arg;
        } else if (slot == 2 && arg->ty != TY_BOOL) error_tok(arg->tok, "prefix は bool にしてください");
        if (arg->kind == ND_REFARG) error_tok(arg->tok, "整数の変換に ref は使えません");
    }
    if (base && is_const_expr(base) && !integer_base_valid(eval_const(base).i, !formatting))
        error_tok(base->tok, "base は %s2 / 8 / 10 / 16 にしてください", formatting ? "" : "0 / ");
    n->kind = formatting ? ND_INT_FORMAT : ND_INT_PARSE; n->ty = formatting ? TY_STR : n->cmp_ty ? n->cmp_ty : TY_INT;
    n->nargs = count;
    if (is_const_expr(n)) (void)eval_const(n);
    return 1;
}
static Value eval_integer_conversion(Node *n)
{
    Node *arg; Value v = {0}, slots[3] = {{0}}; int base, prefix;
    slots[1].i = n->kind == ND_INT_FORMAT ? 10 : 0;
    for (arg = n->lhs; arg; arg = arg->next) slots[arg->option_index] = eval_const(arg);
    base = (int)slots[1].i; prefix = slots[2].i != 0; v.ty = n->ty;
    if (!integer_base_valid(slots[1].i, n->kind == ND_INT_PARSE)) const_error(n, "整数変換の base が不正です");
    if (n->kind == ND_INT_FORMAT) v.s = integer_format_typed(slots[0].i, base, prefix, n->lhs->ty, &v.len);
    else if (integer_read_typed(slots[0].s, (size_t)slots[0].len, base, n->ty, &v.i)) const_error(n, "文字列を指定された基数の int として読み取れません");
    return v;
}
