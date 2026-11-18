/* Conversion arguments remain in source order. option_index only selects the
 * destination slot; it must never reorder side effects. */
static int check_integer_conversion(Node *n)
{
    Node *arg, *base = NULL; int formatting, count = 0, next_slot = 0, named = 0, used[3] = {0};
    if (!n->lhs || n->lhs->arg_name) error_tok(n->tok, QMSG_0240);
    check_expr(n->lhs);
    formatting = n->val == BI_STR;
    if (!(formatting ? type_is_integer(n->lhs->ty) : n->lhs->ty == TY_STR)) return 0;
    for (arg = n->lhs; arg; arg = arg->next) {
        int slot;
        if (arg->arg_name) { slot = arg->option_index; named = 1; }
        else { if (named) error_tok(arg->tok, QMSG_0509); slot = next_slot++; }
        if (slot >= (formatting ? 3 : 2) || used[slot]) error_tok(arg->tok, QMSG_0242);
        used[slot] = 1; arg->option_index = slot; count++;
        check_expr(arg);
        if (slot == 1) {
            if (!type_is_integer(arg->ty)) error_tok(arg->tok, QMSG_0102);
            base = arg;
        } else if (slot == 2 && arg->ty != TY_BOOL) error_tok(arg->tok, QMSG_0306);
        if (arg->kind == ND_REFARG) error_tok(arg->tok, QMSG_0660);
    }
    if (base && is_const_expr(base) && !integer_base_valid((type_is_wide(base->ty)?wide_calculate(base,W_CONVERT,eval_const(base),(Value){0},TY_I64,0):eval_const(base)).i, !formatting))
        error_tok(base->tok, QMSG_0101, formatting ? "" : "0 / ");
    n->kind = formatting ? ND_INT_FORMAT : ND_INT_PARSE; n->ty = formatting ? TY_STR : n->cmp_ty ? n->cmp_ty : TY_INT;
    n->nargs = count;
    if (is_const_expr(n)) (void)eval_const(n);
    return 1;
}
static Value eval_integer_conversion(Node *n)
{
    Node *arg; Value v = {0}, slots[3] = {{0}}; int base, prefix;
    slots[1].i = n->kind == ND_INT_FORMAT ? 10 : 0;
    for (arg = n->lhs; arg; arg = arg->next) { slots[arg->option_index] = eval_const(arg);if(arg->option_index==1&&type_is_wide(arg->ty))slots[1]=wide_calculate(arg,W_CONVERT,slots[1],(Value){0},TY_I64,0); }
    base = (int)slots[1].i; prefix = slots[2].i != 0; v.ty = n->ty;
    if (!integer_base_valid(slots[1].i, n->kind == ND_INT_PARSE)) const_error(n, QMSG_0664);
    if(n->kind==ND_INT_PARSE&&type_is_wide(n->ty))return wide_read_value(n,slots[0],n->ty,base);
    if(n->kind==ND_INT_FORMAT&&type_is_wide(n->lhs->ty)) { WideRequest r={0};r.op=W_FORMAT;r.ty=wide_tag(n->lhs->ty);r.a=value_bits(slots[0]);r.base=base;r.aux=prefix;v.s=malloc(256);if(!v.s)fatal(QMSG_0653);r.buffer=v.s;wide_execute(&r);if(r.status)const_error(n,QMSG_0654);v.len=(int)r.out.lo;return v; }
    if (n->kind == ND_INT_FORMAT) v.s = integer_format_typed(slots[0].i, base, prefix, n->lhs->ty, &v.len);
    else if (integer_read_typed(slots[0].s, (size_t)slots[0].len, base, n->ty, &v.i)) const_error(n, QMSG_0680);
    return v;
}
