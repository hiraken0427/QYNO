/* Complex constructors retain argument source order, including named slots. */
static void check_complex_constructor(Node *node)
{
    Node *arg; int next = 0, named = 0, used[2] = {0};
    node->ty = node->cmp_ty;
    if (node->nargs > 2) error_tok(node->tok, "complex の引数は値1個または real / imag の2個までです");
    if (node->nargs == 1 && !node->lhs->arg_name) {
        check_expr(node->lhs);
        if (type_is_complex(node->lhs->ty)) { node->kind = ND_CAST; return; }
    }
    for (arg = node->lhs; arg; arg = arg->next) {
        int slot;
        if (arg->arg_name) { slot = arg->option_index; named = 1; }
        else { if (named) error_tok(arg->tok, "位置引数は名前付き引数より先に書いてください"); slot = next++; }
        if (slot > 1 || used[slot]) error_tok(arg->tok, "complex の引数が重複しています");
        used[slot] = 1; arg->option_index = slot;
        check_expr(arg);
        if (!is_numeric(arg->ty)) error_tok(arg->tok, "complex の real / imag は実数にしてください");
    }
}
static Value complex_rounded(Value v)
{
    if (v.ty == TY_C64) { v.d = (double)(float)v.d; v.imag = (double)(float)v.imag; }
    return v;
}
static Value complex_calculate(Node *node, Value a, Value b, NodeKind op)
{
    Value v = {0}; v.ty = a.ty;
    if (op == ND_ADD) { v.d = a.d + b.d; v.imag = a.imag + b.imag; }
    else if (op == ND_SUB) { v.d = a.d - b.d; v.imag = a.imag - b.imag; }
    else if (op == ND_MUL) { v.d = a.d*b.d - a.imag*b.imag; v.imag = a.d*b.imag + a.imag*b.d; }
    else {
        double scale = fmax(fabs(b.d), fabs(b.imag)), c, d, ar, ai, den;
        if (b.d == 0 && b.imag == 0) const_error(node, "0 の複素数では割れません");
        c = b.d / scale; d = b.imag / scale; ar = a.d / scale; ai = a.imag / scale;
        den = c*c + d*d; v.d = (ar*c + ai*d)/den; v.imag = (ai*c - ar*d)/den;
    }
    return complex_rounded(v);
}

/* Keep compile-time evaluation in the same operation order as the runtime. */
static double complex_magnitude(double a, double b)
{
    double ratio;
    a = fabs(a); b = fabs(b);
    if (isinf(a) || isinf(b)) return INFINITY;
    if (isnan(a) || isnan(b)) return NAN;
    if (b > a) { double swap = a; a = b; b = swap; }
    if (a == 0) return 0;
    ratio = b/a; return a*sqrt(ratio*ratio + 1.0);
}
static Value complex_square_root(Value v)
{
    double a = v.d, b = v.imag, scale = fmax(fabs(a), fabs(b)), ar, ai, t;
    if (isinf(b)) { v.d = INFINITY; v.imag = b; }
    else if (isnan(a) || isnan(b)) { v.d = NAN; v.imag = NAN; }
    else if (isinf(a)) { v.d = a > 0 ? INFINITY : 0; v.imag = copysign(a > 0 ? 0 : INFINITY, b); }
    else if (scale == 0) { v.d = 0; v.imag = b; }
    else {
        ar = a/scale; ai = b/scale;
        t = sqrt((sqrt(ar*ar + ai*ai) + fabs(ar))*0.5)*sqrt(scale);
        if (signbit(a)) { v.d = (fabs(b)/t)*0.5; v.imag = copysign(t,b); }
        else { v.d = t; v.imag = (b/t)*0.5; }
    }
    return complex_rounded(v);
}
