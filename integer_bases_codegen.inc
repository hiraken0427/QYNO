/* RSI..RDX token, RDI=0/2/8/10/16, R11=integer Type -> RAX=value, RSI=end, RCX=status.
 * RDX is preserved. This parser stops at a delimiter; whole-string callers
 * additionally require only whitespace after the token. */
static void gen_parse_int(void)
{
    int sign = rt_label("integer.sign"), prefix = rt_label("integer.prefix"), digits = rt_label("integer.digits");
    int pref[4], pref_done = rt_label("integer.pref_done"), pref_match = rt_label("integer.pref_match");
    int base_ready = rt_label("integer.base"), limit_ready = rt_label("integer.limit");
    int loop = rt_label("integer.loop"), digit = rt_label("integer.digit"), accumulate = rt_label("integer.accumulate");
    int done = rt_label("integer.done"), positive = rt_label("integer.positive"), fail = rt_label("integer.bad"), range = rt_label("integer.range"), out = rt_label("integer.out");
    int i; const int bases[4] = {2,8,10,16}; const int letters[4] = {'b','o','d','x'};
    for (i = 0; i < 4; i++) pref[i] = rt_label("integer.prefix_kind");
    rt_enter(parse_int_label, 6); rt_save(1, RSI); rt_save(2, RDX); rt_save(6, R11);
    x64_xor(R8, R8);
    x64_cmp(RSI, RDX); x64_jcc(CC_AE, fail);
    x64_load8(RAX, RSI, 0); rt_cmp(RAX, '-'); x64_jcc(CC_NZ, sign);
    x64_mov_imm(R8, 1); x64_add_imm(RSI, 1); x64_jmp(prefix);
    x64_bind(sign); rt_cmp(RAX, '+'); x64_jcc(CC_NZ, prefix); x64_add_imm(RSI, 1);
    x64_bind(prefix); x64_lea(RCX, RSI, 1); x64_cmp(RCX, RDX); x64_jcc(CC_AE, digits);
    x64_load8(RAX, RSI, 0); rt_cmp(RAX, '0'); x64_jcc(CC_NZ, digits);
    x64_load8(RAX, RSI, 1); x64_mov_imm(RCX, 32); x64_or(RAX, RCX);
    for (i = 0; i < 4; i++) { rt_cmp(RAX, letters[i]); x64_jcc(CC_Z, pref[i]); }
    x64_jmp(digits);
    for (i = 0; i < 4; i++) { x64_bind(pref[i]); x64_mov_imm(RAX, bases[i]); x64_jmp(pref_done); }
    x64_bind(pref_done); x64_test(RDI, RDI); x64_jcc(CC_Z, pref_match);
    x64_cmp(RDI, RAX); x64_jcc(CC_NZ, fail);
    x64_bind(pref_match); x64_mov(RDI, RAX); x64_add_imm(RSI, 2);
    x64_bind(digits); x64_test(RDI, RDI); x64_jcc(CC_NZ, base_ready); x64_mov_imm(RDI, 10);
    x64_bind(base_ready);
    { int ty;
      for (ty = TY_I64; ty <= TY_U64; ty++) {
          int next = rt_label("integer.limit.next");
          rt_load(RCX, 6); rt_cmp(RCX, ty); x64_jcc(CC_NZ, next);
          if (integer_unsigned((Type)ty)) { x64_test(R8, R8); x64_jcc(CC_NZ, range); }
          x64_mov_imm(RAX, integer_width((Type)ty) == 64 ? -1 : integer_bits((UINT64_C(1) << integer_width((Type)ty)) - 1));
          if (!integer_unsigned((Type)ty)) {
              rt_cmp(RDI, 10); x64_jcc(CC_NZ, limit_ready);
              x64_shr_imm(RAX, 1); x64_add(RAX, R8);
          }
          x64_jmp(limit_ready); x64_bind(next);
      }
    }
    x64_bind(limit_ready); x64_xor(RDX, RDX); x64_div(RDI); rt_save(3, RAX); rt_save(4, RDX); rt_load(RDX, 2);
    x64_xor(R9, R9); x64_xor(R10, R10);
    x64_bind(loop); x64_cmp(RSI, RDX); x64_jcc(CC_AE, done);
    x64_load8(RAX, RSI, 0); x64_sub_imm(RAX, '0'); rt_cmp(RAX, 10); x64_jcc(CC_B, digit);
    x64_load8(RAX, RSI, 0); x64_mov_imm(RCX, 32); x64_or(RAX, RCX); x64_sub_imm(RAX, 'a');
    rt_cmp(RAX, 6); x64_jcc(CC_AE, done); x64_add_imm(RAX, 10);
    x64_bind(digit); x64_cmp(RAX, RDI); x64_jcc(CC_AE, done);
    rt_load(RCX, 3); x64_cmp(R9, RCX); x64_jcc(CC_A, range); x64_jcc(CC_B, accumulate);
    rt_load(RCX, 4); x64_cmp(RAX, RCX); x64_jcc(CC_A, range);
    x64_bind(accumulate); x64_imul(R9, RDI); x64_add(R9, RAX); x64_add_imm(R10, 1); x64_add_imm(RSI, 1); x64_jmp(loop);
    x64_bind(done); x64_test(R10, R10); x64_jcc(CC_Z, fail);
    x64_call(word_follows_label); x64_test(RAX, RAX); x64_jcc(CC_NZ, fail);
    x64_mov(RAX, R9); x64_test(R8, R8); x64_jcc(CC_Z, positive); x64_neg(RAX);
    x64_bind(positive);
    { int ty; for (ty = TY_I8; ty <= TY_U32; ty++) {
        int next = rt_label("integer.extend.next");
        rt_load(RCX, 6); rt_cmp(RCX, ty); x64_jcc(CC_NZ, next);
        gen_integer_normalize((Type)ty); x64_bind(next);
    } }
    x64_xor(RCX, RCX); x64_jmp(out);
    x64_bind(fail); x64_mov_imm(RCX, 1); rt_load(RSI, 1); x64_jmp(out);
    x64_bind(range); x64_mov_imm(RCX, 2); rt_load(RSI, 1);
    x64_bind(out); rt_load(RDX, 2); rt_leave();
}

/* RAX=int, RDI=base, RSI=prefix -> new immutable str in RAX. */
static void gen_integer_format_runtime(void)
{
    int magnitude = rt_label("format.magnitude"), loop = rt_label("format.loop"), decimal = rt_label("format.decimal");
    int store = rt_label("format.store"), prefix = rt_label("format.prefix"), prefix_done = rt_label("format.prefix_done");
    int octal = rt_label("format.octal"), dec_prefix = rt_label("format.dec_prefix"), hex = rt_label("format.hex");
    int sign = rt_label("format.sign"), allocate = rt_label("format.allocate"), copy = rt_label("format.copy"), done = rt_label("format.done");
    rt_enter(integer_format_label, 16); rt_save(1, RDI); rt_save(2, RSI); rt_set(3, 0);
    x64_test(R11, R11); x64_jcc(CC_NZ, magnitude);
    rt_cmp(RDI, 10); x64_jcc(CC_NZ, magnitude); x64_test(RAX, RAX); x64_jcc(CC_NS, magnitude);
    rt_set(3, 1); x64_neg(RAX);
    x64_bind(magnitude); x64_lea(R10, RBP, -49); x64_xor(R9, R9);
    x64_bind(loop); x64_xor(RDX, RDX); x64_div(RDI); rt_cmp(RDX, 10); x64_jcc(CC_B, decimal);
    x64_add_imm(RDX, 'a' - 10); x64_jmp(store);
    x64_bind(decimal); x64_add_imm(RDX, '0');
    x64_bind(store); x64_store8(R10, 0, RDX); x64_sub_imm(R10, 1); x64_add_imm(R9, 1);
    x64_test(RAX, RAX); x64_jcc(CC_NZ, loop);
    rt_load(RAX, 2); x64_test(RAX, RAX); x64_jcc(CC_Z, sign);
    rt_cmp(RDI, 8); x64_jcc(CC_Z, octal); rt_cmp(RDI, 10); x64_jcc(CC_Z, dec_prefix); rt_cmp(RDI, 16); x64_jcc(CC_Z, hex);
    x64_mov_imm(RAX, 'b'); x64_jmp(prefix);
    x64_bind(octal); x64_mov_imm(RAX, 'o'); x64_jmp(prefix);
    x64_bind(dec_prefix); x64_mov_imm(RAX, 'd'); x64_jmp(prefix);
    x64_bind(hex); x64_mov_imm(RAX, 'x');
    x64_bind(prefix); x64_store8(R10, 0, RAX); x64_sub_imm(R10, 1); x64_mov_imm(RAX, '0');
    x64_store8(R10, 0, RAX); x64_sub_imm(R10, 1); x64_add_imm(R9, 2); x64_jmp(prefix_done);
    x64_bind(prefix_done); x64_bind(sign); rt_load(RAX, 3); x64_test(RAX, RAX); x64_jcc(CC_Z, allocate);
    x64_mov_imm(RAX, '-'); x64_store8(R10, 0, RAX); x64_sub_imm(R10, 1); x64_add_imm(R9, 1);
    x64_bind(allocate); x64_lea(RSI, R10, 1); rt_save(4, RSI); rt_save(5, R9);
    x64_lea(RDI, R9, 8); x64_call(alloc_label); rt_load(RCX, 5); x64_store(RAX, 0, RCX);
    rt_load(RSI, 4); x64_lea(RDI, RAX, 8);
    x64_bind(copy); x64_test(RCX, RCX); x64_jcc(CC_Z, done);
    x64_load8(RDX, RSI, 0); x64_store8(RDI, 0, RDX); x64_add_imm(RSI, 1); x64_add_imm(RDI, 1); x64_sub_imm(RCX, 1); x64_jmp(copy);
    x64_bind(done); rt_leave();
}

static void gen_integer_conversion(Node *node)
{
    Node *arg; int base, ok = new_label("integer.base.ok"), bad = new_label("integer.parse.bad"), done = new_label("integer.conversion.done");
    int is_format = node->kind == ND_INT_FORMAT;
    x64_sub_imm(RSP, 32); depth += 4; base = emitting_function->stack_size + depth * 8;
    x64_store_imm(RBP, -base, 0); x64_store_imm(RBP, -base + 8, is_format ? 10 : 0); x64_store_imm(RBP, -base + 16, 0);
    x64_store_imm(RBP, -base + 24, 0);
    for (arg = node->lhs; arg; arg = arg->next) { gen_expr(arg); if(arg->option_index==1 && type_is_wide(arg->ty))gen_convert(arg->ty,TY_I64); x64_store(RBP, -base + arg->option_index * 8, RAX); }
    x64_load(RDI, RBP, -base + 8);
    if (!is_format) { rt_cmp(RDI, 0); x64_jcc(CC_Z, ok); }
    rt_cmp(RDI, 2); x64_jcc(CC_Z, ok); rt_cmp(RDI, 8); x64_jcc(CC_Z, ok);
    rt_cmp(RDI, 10); x64_jcc(CC_Z, ok); rt_cmp(RDI, 16); x64_jcc(CC_Z, ok);
    gen_fail(node->tok, "整数変換の base が不正です", EX_SOFTWARE);
    x64_bind(ok); x64_load(RAX, RBP, -base);
    if (is_format) { x64_load(RSI, RBP, -base + 16); gen_format_integer(node->lhs->ty); }
    else {
        x64_load(RDX, RAX, 0); x64_lea(RSI, RAX, 8); x64_add(RDX, RSI); x64_call(skip_ws_label);
        gen_parse_integer(node->ty); x64_test(RCX, RCX); x64_jcc(CC_NZ, bad);
        x64_store(RBP, -base + 16, RAX); x64_call(skip_ws_label); x64_cmp(RSI, RDX); x64_jcc(CC_NZ, bad);
        x64_load(RAX, RBP, -base + 16); x64_jmp(done);
        x64_bind(bad); gen_fail(node->tok, "文字列を指定された基数の int として読み取れません", EX_DATAERR);
    }
    x64_bind(done); x64_add_imm(RSP, 32); depth -= 4;
}
