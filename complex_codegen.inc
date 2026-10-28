/* Immutable two-component boxes keep the existing eight-byte scalar ABI.
 * A null box is the default value 0+0i. GC scans boxes and aggregate slots. */
static void complex_xmm_copy(int to, int from)
{ x64_movq_from_xmm(R10, from); x64_movq_to_xmm(to, R10); }
static void complex_output_components(int mode)
{
    x64_movq_from_xmm(RAX, 0); x64_movq_from_xmm(RDI, 1);
    x64_jmp(complex_make_labels[mode]);
}
static void gen_complex_constructor(Node *node)
{
    Node *arg; int base;
    x64_sub_imm(RSP, 16); depth += 2; base = emitting_function->stack_size + depth * 8;
    x64_store_imm(RBP, -base, 0); x64_store_imm(RBP, -base + 8, 0);
    for (arg = node->lhs; arg; arg = arg->next) {
        gen_expr(arg); gen_convert(arg->ty, complex_component(node->ty));
        gen_convert(complex_component(node->ty), TY_DOUBLE);
        x64_store(RBP, -base + arg->option_index * 8, RAX);
    }
    x64_load(RAX, RBP, -base); x64_load(RDI, RBP, -base + 8);
    x64_call(complex_make_labels[node->ty == TY_C128]);
    x64_add_imm(RSP, 16); depth -= 2;
}
static void gen_complex_runtime(void)
{
    int mode, op;
    for (mode = 0; mode < 2; mode++) {
        rt_enter(complex_make_labels[mode], 2);
        if (!mode) { gen_convert(TY_DOUBLE, TY_FLOAT); gen_convert(TY_FLOAT, TY_DOUBLE); }
        rt_save(1, RAX); x64_mov(RAX, RDI);
        if (!mode) { gen_convert(TY_DOUBLE, TY_FLOAT); gen_convert(TY_FLOAT, TY_DOUBLE); }
        rt_save(2, RAX); x64_mov_imm(RDI, 16); x64_call(alloc_label);
        rt_load(RDX, 1); x64_store(RAX, 0, RDX); rt_load(RDX, 2); x64_store(RAX, 8, RDX); rt_leave();
        for (op = 0; op < 4; op++) {
            x64_bind(complex_op_labels[mode][op]);
            gen_complex_unpack(RAX, 0, 1); gen_complex_unpack(RDI, 2, 3);
            if (op < 2) {
                x64_sse_sd(op ? SSE_SUB : SSE_ADD, 0, 2); x64_sse_sd(op ? SSE_SUB : SSE_ADD, 1, 3);
            } else if (op == 2) {
                complex_xmm_copy(4, 0); x64_sse_sd(SSE_MUL, 4, 2);
                complex_xmm_copy(5, 1); x64_sse_sd(SSE_MUL, 5, 3); x64_sse_sd(SSE_SUB, 4, 5);
                complex_xmm_copy(5, 0); x64_sse_sd(SSE_MUL, 5, 3);
                complex_xmm_copy(6, 1); x64_sse_sd(SSE_MUL, 6, 2); x64_sse_sd(SSE_ADD, 5, 6);
                complex_xmm_copy(0, 4); complex_xmm_copy(1, 5);
            } else {
                int scale = rt_label("complex.div.scale"), nonzero = rt_label("complex.div.nonzero");
                x64_movq_from_xmm(R10, 2); x64_btr(R10, 63); x64_movq_to_xmm(4, R10);
                x64_movq_from_xmm(R10, 3); x64_btr(R10, 63); x64_movq_to_xmm(5, R10);
                x64_ucomisd(4, 5); x64_jcc(CC_AE, scale); complex_xmm_copy(4, 5);
                x64_bind(scale); x64_xor(R10, R10); x64_movq_to_xmm(5, R10);
                x64_ucomisd(4, 5); x64_jcc(CC_P, nonzero); x64_jcc(CC_NZ, nonzero);
                collection_fail("0 の複素数では割れません\n"); x64_bind(nonzero);
                x64_sse_sd(SSE_DIV, 0, 4); x64_sse_sd(SSE_DIV, 1, 4);
                x64_sse_sd(SSE_DIV, 2, 4); x64_sse_sd(SSE_DIV, 3, 4);
                complex_xmm_copy(4, 2); x64_sse_sd(SSE_MUL, 4, 2);
                complex_xmm_copy(5, 3); x64_sse_sd(SSE_MUL, 5, 3); x64_sse_sd(SSE_ADD, 4, 5);
                complex_xmm_copy(5, 0); x64_sse_sd(SSE_MUL, 5, 2);
                complex_xmm_copy(6, 1); x64_sse_sd(SSE_MUL, 6, 3); x64_sse_sd(SSE_ADD, 5, 6);
                complex_xmm_copy(6, 1); x64_sse_sd(SSE_MUL, 6, 2);
                complex_xmm_copy(7, 0); x64_sse_sd(SSE_MUL, 7, 3); x64_sse_sd(SSE_SUB, 6, 7);
                x64_sse_sd(SSE_DIV, 5, 4); x64_sse_sd(SSE_DIV, 6, 4);
                complex_xmm_copy(0, 5); complex_xmm_copy(1, 6);
            }
            complex_output_components(mode);
        }
        { int positive = rt_label("complex.power.positive"), loop = rt_label("complex.power.loop");
          int square = rt_label("complex.power.square"), done = rt_label("complex.power.done");
          rt_enter(complex_pow_labels[mode], 3); rt_save(1, RAX); rt_save(2, RDI);
          x64_mov_imm(RAX, double_bits(1)); x64_xor(RDI, RDI); x64_call(complex_make_labels[mode]); rt_save(3, RAX);
          rt_load(RDI, 2); x64_test(RDI, RDI); x64_jcc(CC_NS, positive);
          x64_neg(RDI); rt_save(2, RDI); rt_load(RDI, 1); x64_call(complex_op_labels[mode][3]); rt_save(1, RAX);
          x64_bind(positive); x64_bind(loop); rt_load(RDI, 2); x64_test(RDI, RDI); x64_jcc(CC_Z, done);
          x64_and_imm(RDI, 1); x64_jcc(CC_Z, square);
          rt_load(RAX, 3); rt_load(RDI, 1); x64_call(complex_op_labels[mode][2]); rt_save(3, RAX);
          x64_bind(square); rt_load(RDI, 2); x64_shr_imm(RDI, 1); rt_save(2, RDI); x64_test(RDI, RDI); x64_jcc(CC_Z, done);
          rt_load(RAX, 1); x64_mov(RDI, RAX); x64_call(complex_op_labels[mode][2]); rt_save(1, RAX); x64_jmp(loop);
          x64_bind(done); rt_load(RAX, 3); rt_leave();
        }
        { int scale = rt_label("complex.sqrt.scale"), zero = rt_label("complex.sqrt.zero");
          int real_inf = rt_label("complex.sqrt.real.inf"), imag_inf = rt_label("complex.sqrt.imag.inf");
          int negative_inf = rt_label("complex.sqrt.negative.inf"), nan = rt_label("complex.sqrt.nan");
          int negative = rt_label("complex.sqrt.negative"), signed_imag = rt_label("complex.sqrt.signed.imag"), done = rt_label("complex.sqrt.done");
          rt_enter(complex_sqrt_labels[mode], 2); gen_complex_unpack(RAX, 0, 1);
          x64_movq_from_xmm(RAX, 0); rt_save(1, RAX); x64_btr(RAX, 63);
          x64_movq_from_xmm(RDI, 1); rt_save(2, RDI); x64_btr(RDI, 63);
          rt_cmp(RDI, double_bits(INFINITY)); x64_jcc(CC_Z, imag_inf); x64_jcc(CC_A, nan);
          rt_cmp(RAX, double_bits(INFINITY)); x64_jcc(CC_A, nan); x64_jcc(CC_Z, real_inf);
          x64_cmp(RAX, RDI); x64_jcc(CC_AE, scale); x64_mov(RAX, RDI);
          x64_bind(scale); x64_test(RAX, RAX); x64_jcc(CC_Z, zero); x64_movq_to_xmm(2, RAX);
          complex_xmm_copy(3, 0); x64_sse_sd(SSE_DIV, 3, 2);
          complex_xmm_copy(4, 1); x64_sse_sd(SSE_DIV, 4, 2);
          complex_xmm_copy(5, 3); x64_sse_sd(SSE_MUL, 5, 3); x64_sse_sd(SSE_MUL, 4, 4); x64_sse_sd(SSE_ADD, 5, 4); x64_sqrtsd(5, 5);
          x64_movq_from_xmm(R10, 3); x64_btr(R10, 63); x64_movq_to_xmm(6, R10); x64_sse_sd(SSE_ADD, 5, 6);
          x64_mov_imm(R10, double_bits(0.5)); x64_movq_to_xmm(6, R10); x64_sse_sd(SSE_MUL, 5, 6); x64_sqrtsd(5, 5);
          x64_sqrtsd(2, 2); x64_sse_sd(SSE_MUL, 5, 2);
          rt_load(RAX, 1); x64_test(RAX, RAX); x64_jcc(CC_S, negative);
          x64_sse_sd(SSE_DIV, 1, 5); x64_sse_sd(SSE_MUL, 1, 6); x64_movq_from_xmm(RDI, 1); x64_movq_from_xmm(RAX, 5); x64_jmp(done);
          x64_bind(negative); x64_movq_from_xmm(RAX, 1); x64_btr(RAX, 63); x64_movq_to_xmm(0, RAX);
          x64_sse_sd(SSE_DIV, 0, 5); x64_sse_sd(SSE_MUL, 0, 6); x64_movq_from_xmm(RAX, 0); x64_movq_from_xmm(RDI, 5);
          rt_load(R10, 2); x64_test(R10, R10); x64_jcc(CC_NS, signed_imag); x64_btc(RDI, 63);
          x64_bind(signed_imag); x64_jmp(done);
          x64_bind(zero); x64_xor(RAX, RAX); rt_load(RDI, 2); x64_jmp(done);
          x64_bind(imag_inf); x64_mov_imm(RAX, double_bits(INFINITY)); rt_load(RDI, 2); x64_jmp(done);
          x64_bind(real_inf); rt_load(RAX, 1); x64_test(RAX, RAX); x64_jcc(CC_S, negative_inf);
          x64_mov_imm(RAX, double_bits(INFINITY)); rt_load(RDI, 2); x64_shr_imm(RDI, 63); x64_shl_imm(RDI, 63); x64_jmp(done);
          x64_bind(negative_inf); x64_xor(RAX, RAX); rt_load(RDI, 2); x64_shr_imm(RDI, 63); x64_shl_imm(RDI, 63); x64_mov_imm(R10, double_bits(INFINITY)); x64_or(RDI, R10); x64_jmp(done);
          x64_bind(nan); x64_mov_imm(RAX, double_bits(NAN)); x64_mov(RDI, RAX);
          x64_bind(done); x64_mov(RSP, RBP); x64_pop(RBP); x64_jmp(complex_make_labels[mode]);
        }
        { int negative = rt_label("complex.print.negative"), imag = rt_label("complex.print.imag");
          rt_enter(complex_print_labels[mode], 2); gen_complex_unpack(RAX, 0, 1);
          x64_movq_from_xmm(RAX, 0); rt_save(1, RAX); x64_movq_from_xmm(RAX, 1); rt_save(2, RAX);
          rt_load(RAX, 1); x64_call(mode ? print_double_label : print_float_label);
          rt_load(RAX, 2); x64_test(RAX, RAX); x64_jcc(CC_S, negative);
          gen_write_const(" + i"); x64_jmp(imag);
          x64_bind(negative); x64_btr(RAX, 63); rt_save(2, RAX); gen_write_const(" - i");
          x64_bind(imag); rt_load(RAX, 2); x64_call(mode ? print_double_label : print_float_label); rt_leave();
        }
    }
    { int no = rt_label("complex.equal.no");
      x64_bind(complex_equal_label); gen_complex_unpack(RAX, 0, 1); gen_complex_unpack(RDI, 2, 3);
      x64_ucomisd(0, 2); x64_jcc(CC_P, no); x64_jcc(CC_NZ, no);
      x64_ucomisd(1, 3); x64_jcc(CC_P, no); x64_jcc(CC_NZ, no);
      x64_mov_imm(RAX, 1); x64_ret(); x64_bind(no); x64_xor(RAX, RAX); x64_ret();
    }
    { int infinite = rt_label("complex.abs.inf"), nan = rt_label("complex.abs.nan"), scale = rt_label("complex.abs.scale"), done = rt_label("complex.abs.done");
      x64_bind(complex_abs_label); gen_complex_unpack(RAX, 0, 1);
      x64_movq_from_xmm(RAX, 0); x64_btr(RAX, 63); x64_movq_to_xmm(0, RAX);
      x64_movq_from_xmm(RDI, 1); x64_btr(RDI, 63); x64_movq_to_xmm(1, RDI);
      rt_cmp(RAX, double_bits(INFINITY)); x64_jcc(CC_Z, infinite);
      rt_cmp(RDI, double_bits(INFINITY)); x64_jcc(CC_Z, infinite);
      x64_ucomisd(0, 1); x64_jcc(CC_P, nan); x64_jcc(CC_AE, scale);
      complex_xmm_copy(2, 0); complex_xmm_copy(0, 1); complex_xmm_copy(1, 2);
      x64_bind(scale); x64_xor(R10, R10); x64_movq_to_xmm(2, R10); x64_ucomisd(0, 2); x64_jcc(CC_Z, done);
      x64_sse_sd(SSE_DIV, 1, 0); x64_sse_sd(SSE_MUL, 1, 1);
      x64_mov_imm(R10, double_bits(1)); x64_movq_to_xmm(2, R10); x64_sse_sd(SSE_ADD, 1, 2);
      x64_sqrtsd(1, 1); x64_sse_sd(SSE_MUL, 0, 1); x64_jmp(done);
      x64_bind(nan); x64_sse_sd(SSE_ADD, 0, 1); x64_jmp(done);
      x64_bind(infinite); x64_mov_imm(R10, double_bits(INFINITY)); x64_movq_to_xmm(0, R10);
      x64_bind(done); x64_movq_from_xmm(RAX, 0); x64_ret();
    }
}
