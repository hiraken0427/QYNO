/* Components use the existing decimal reader, with an optional exponent and
 * an i terminator confined to this reader. Ordinary real input is unchanged. */
static void gen_complex_component_reader(void)
{
    int leading = rt_label("complex.number.leading");
    int scan = rt_label("complex.number.scan"), advance = rt_label("complex.number.advance");
    int exponent = rt_label("complex.number.exponent"), mantissa = rt_label("complex.number.mantissa");
    int capped = rt_label("complex.number.capped"), after = rt_label("complex.number.after");
    int sign = rt_label("complex.number.sign"), digits = rt_label("complex.number.digits");
    int loop = rt_label("complex.number.exp.loop"), next = rt_label("complex.number.exp.next");
    int exp_done = rt_label("complex.number.exp.done"), power = rt_label("complex.number.power");
    int exp_positive = rt_label("complex.number.exp.positive"), valid = rt_label("complex.number.valid");
    int bad = rt_label("complex.number.bad"), range = rt_label("complex.number.range"), out = rt_label("complex.number.out");
    rt_enter(complex_real_label, 6); rt_save(1, RDX); rt_save(2, RSI); rt_set(4, 0);
    x64_mov(R8, RSI); x64_cmp(R8, RDX); x64_jcc(CC_AE, bad); x64_load8(RAX, R8, 0);
    rt_cmp(RAX, '+'); x64_jcc(CC_Z, leading); rt_cmp(RAX, '-'); x64_jcc(CC_NZ, scan);
    x64_bind(leading); x64_add_imm(R8, 1);
    x64_bind(scan); x64_cmp(R8, RDX); x64_jcc(CC_AE, mantissa);
    x64_load8(RAX, R8, 0);
    rt_cmp(RAX, 'e'); x64_jcc(CC_Z, exponent); rt_cmp(RAX, 'E'); x64_jcc(CC_Z, exponent);
    rt_cmp(RAX, 'i'); x64_jcc(CC_Z, capped);
    rt_cmp(RAX, '.'); x64_jcc(CC_Z, advance);
    x64_sub_imm(RAX, '0'); rt_cmp(RAX, 10); x64_jcc(CC_AE, mantissa);
    x64_bind(advance); x64_add_imm(R8, 1); x64_jmp(scan);
    x64_bind(exponent); rt_save(4, R8);
    x64_bind(capped); x64_mov(RDX, R8);
    x64_bind(mantissa); x64_call(parse_real_label); x64_test(RCX, RCX); x64_jcc(CC_NZ, out);
    rt_save(5, RAX); rt_load(R8, 4); x64_test(R8, R8); x64_jcc(CC_Z, valid);
    x64_cmp(RSI, R8); x64_jcc(CC_NZ, bad);
    x64_lea(RSI, R8, 1); rt_load(RDX, 1); rt_set(6, 0);
    x64_cmp(RSI, RDX); x64_jcc(CC_AE, bad); x64_load8(RAX, RSI, 0);
    rt_cmp(RAX, '-'); x64_jcc(CC_NZ, sign); rt_set(6, 1); x64_add_imm(RSI, 1); x64_jmp(digits);
    x64_bind(sign); rt_cmp(RAX, '+'); x64_jcc(CC_NZ, digits); x64_add_imm(RSI, 1);
    x64_bind(digits); rt_save(4, RSI); x64_xor(RCX, RCX);
    x64_bind(loop); x64_cmp(RSI, RDX); x64_jcc(CC_AE, exp_done);
    x64_load8(RAX, RSI, 0); x64_sub_imm(RAX, '0'); rt_cmp(RAX, 10); x64_jcc(CC_AE, exp_done);
    rt_cmp(RCX, 10000); x64_jcc(CC_A, next); x64_mov_imm(R10, 10); x64_imul(RCX, R10); x64_add(RCX, RAX);
    x64_bind(next); x64_add_imm(RSI, 1); x64_jmp(loop);
    x64_bind(exp_done); rt_load(R8, 4); x64_cmp(RSI, R8); x64_jcc(CC_Z, bad);
    rt_save(3, RSI); rt_save(4, RCX); rt_load(RAX, 5); x64_btr(RAX, 63); x64_test(RAX, RAX); x64_jcc(CC_NZ, power);
    rt_load(RAX, 5); x64_jmp(valid);
    x64_bind(power); rt_cmp(RCX, 10000); x64_jcc(CC_A, range); x64_mov(RDI, RCX);
    rt_load(RAX, 6); x64_test(RAX, RAX); x64_jcc(CC_Z, exp_positive); x64_neg(RDI);
    x64_bind(exp_positive); x64_mov_imm(RAX, double_bits(10)); x64_call(pow_double_label);
    x64_movq_to_xmm(0, RAX); rt_load(RAX, 5); x64_movq_to_xmm(1, RAX); x64_sse_sd(SSE_MUL, 0, 1); x64_movq_from_xmm(RAX, 0);
    x64_mov(R10, RAX); x64_btr(R10, 63); rt_cmp(R10, double_bits(INFINITY)); x64_jcc(CC_AE, range);
    rt_load(RSI, 3);
    x64_bind(valid); rt_save(5, RAX); rt_load(RDX, 1);
    x64_cmp(RSI, RDX); x64_jcc(CC_AE, after);
    x64_load8(RAX, RSI, 0); rt_cmp(RAX, 'i'); x64_jcc(CC_Z, after);
    x64_call(word_follows_label); x64_test(RAX, RAX); x64_jcc(CC_NZ, bad);
    x64_bind(after); rt_load(RAX, 5); x64_xor(RCX, RCX); x64_jmp(out);
    x64_bind(bad); x64_mov_imm(RCX, 1); rt_load(RSI, 2); x64_jmp(out);
    x64_bind(range); x64_mov_imm(RCX, 2); rt_load(RSI, 2);
    x64_bind(out); rt_load(RDX, 1); rt_leave();
}
static void gen_complex_input_runtime(void)
{
    int mode;
    gen_complex_component_reader();
    for (mode = 0; mode < 2; mode++) {
        int positive = rt_label("complex.input.positive"), initial = rt_label("complex.input.initial");
        int prefix = rt_label("complex.input.prefix"), first_suffix = rt_label("complex.input.first.suffix");
        int minus = rt_label("complex.input.minus"), second = rt_label("complex.input.second");
        int second_prefix = rt_label("complex.input.second.prefix"), coefficient = rt_label("complex.input.coefficient");
        int imag = rt_label("complex.input.imag"), sign_done = rt_label("complex.input.sign.done");
        int real_only = rt_label("complex.input.real.only"), finish = rt_label("complex.input.finish");
        int bad = rt_label("complex.input.bad"), range = rt_label("complex.input.range"), out = rt_label("complex.input.out");
        rt_enter(complex_parse_labels[mode], 8); rt_save(1, RDX); rt_save(2, RSI); rt_save(8, RSI);
        rt_set(3, 0); rt_set(4, 0); rt_set(6, 0);
        x64_cmp(RSI, RDX); x64_jcc(CC_AE, bad); x64_load8(RAX, RSI, 0);
        rt_cmp(RAX, '-'); x64_jcc(CC_NZ, positive); rt_set(6, 1); x64_add_imm(RSI, 1); x64_call(skip_ws_label); x64_jmp(initial);
        x64_bind(positive); rt_cmp(RAX, '+'); x64_jcc(CC_NZ, initial); x64_add_imm(RSI, 1); x64_call(skip_ws_label);
        x64_bind(initial); x64_cmp(RSI, RDX); x64_jcc(CC_AE, bad); x64_load8(RAX, RSI, 0);
        rt_cmp(RAX, 'i'); x64_jcc(CC_Z, prefix);
        rt_load(RSI, 8); rt_set(6, 0); x64_call(complex_real_label); x64_test(RCX, RCX); x64_jcc(CC_NZ, out);
        rt_save(3, RAX); rt_save(5, RSI);
        x64_cmp(RSI, RDX); x64_jcc(CC_AE, real_only); x64_load8(RAX, RSI, 0);
        rt_cmp(RAX, 'i'); x64_jcc(CC_Z, first_suffix);
        x64_call(skip_ws_label); x64_cmp(RSI, RDX); x64_jcc(CC_AE, real_only); x64_load8(RAX, RSI, 0);
        rt_cmp(RAX, '+'); x64_jcc(CC_NZ, minus); x64_jmp(second);
        x64_bind(minus); rt_cmp(RAX, '-'); x64_jcc(CC_NZ, real_only); rt_set(6, 1);
        x64_bind(second); x64_add_imm(RSI, 1); x64_call(skip_ws_label); x64_cmp(RSI, RDX); x64_jcc(CC_AE, real_only);
        x64_load8(RAX, RSI, 0); rt_cmp(RAX, 'i'); x64_jcc(CC_Z, second_prefix);
        rt_cmp(RAX, '+'); x64_jcc(CC_Z, real_only); rt_cmp(RAX, '-'); x64_jcc(CC_Z, real_only);
        x64_call(complex_real_label); x64_test(RCX, RCX); x64_jcc(CC_NZ, real_only); rt_save(4, RAX);
        x64_cmp(RSI, RDX); x64_jcc(CC_AE, real_only); x64_load8(RAX, RSI, 0); rt_cmp(RAX, 'i'); x64_jcc(CC_NZ, real_only);
        x64_add_imm(RSI, 1); rt_load(RAX, 4); x64_jmp(imag);
        x64_bind(first_suffix); rt_load(RAX, 3); rt_set(3, 0); x64_add_imm(RSI, 1); x64_jmp(imag);
        x64_bind(prefix); x64_add_imm(RSI, 1); x64_jmp(coefficient);
        x64_bind(second_prefix); x64_add_imm(RSI, 1);
        x64_bind(coefficient); x64_cmp(RSI, RDX); x64_jcc(CC_AE, bad); x64_load8(RAX, RSI, 0);
        rt_cmp(RAX, '+'); x64_jcc(CC_Z, bad); rt_cmp(RAX, '-'); x64_jcc(CC_Z, bad);
        x64_call(complex_real_label); x64_test(RCX, RCX); x64_jcc(CC_NZ, out);
        x64_bind(imag); rt_load(R10, 6); x64_test(R10, R10); x64_jcc(CC_Z, sign_done); x64_btc(RAX, 63);
        x64_bind(sign_done); rt_save(4, RAX);
        x64_call(word_follows_label); x64_test(RAX, RAX); x64_jcc(CC_NZ, bad); x64_jmp(finish);
        x64_bind(real_only); rt_load(RSI, 5); rt_set(4, 0);
        x64_bind(finish); rt_save(7, RSI);
        if (!mode) {
            int part;
            for (part = 3; part <= 4; part++) {
                rt_load(RAX, part); gen_convert(TY_DOUBLE, TY_FLOAT); gen_convert(TY_FLOAT, TY_DOUBLE);
                x64_mov(R10, RAX); x64_btr(R10, 63); rt_cmp(R10, double_bits(INFINITY)); x64_jcc(CC_AE, range);
                rt_save(part, RAX);
            }
        }
        rt_load(RAX, 3); rt_load(RDI, 4); x64_call(complex_make_labels[mode]); rt_load(RSI, 7); x64_xor(RCX, RCX); x64_jmp(out);
        x64_bind(bad); x64_mov_imm(RCX, 1); rt_load(RSI, 2); x64_jmp(out);
        x64_bind(range); x64_mov_imm(RCX, 2); rt_load(RSI, 2);
        x64_bind(out); rt_load(RDX, 1); rt_leave();
    }
}
