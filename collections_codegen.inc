/* 配列 [length, capacity, shape, data, active_element_refs]。shape は各次元について
 * [残りの次元数, 末端要素型, 上限(-1なら無制限)] を並べた読み取り専用データ。
 * 全要素は8バイト。配列の要素は独立した子配列、文字列のバッファは不変。
 */
#define COLLECTION_CONTEXT 256

static void collection_context(Token *tok)
{
    char prefix[1024];
    int n = snprintf(prefix, sizeof(prefix), "QYNO 実行時エラー: %s:%d:%d: ", tok->source->filename, tok->line, tok->col);
    if (n < 0) fatal("配列の診断を作成できません");
    if (n >= (int)sizeof(prefix)) n = (int)sizeof(prefix) - 1;
    x64_lea_data(R11, prog_add_str(prog, prefix, (size_t)n));
    x64_store(R14, COLLECTION_CONTEXT, R11);
}

static void collection_fail(const char *reason)
{
    x64_load(RAX, R14, COLLECTION_CONTEXT);
    x64_load(RDX, RAX, 0);
    x64_lea(RSI, RAX, 8);
    x64_call(write_err_label);
    x64_lea_data(RSI, prog_add_data(prog, reason, strlen(reason)));
    x64_mov_imm(RDX, (long long)strlen(reason));
    x64_mov_imm(R8, EX_SOFTWARE);
    x64_jmp(runtime_error_label);
}

/* rax=配列。要素参照が生存する間は構造・所有元を変更しない。 */
static void array_require_unborrowed(void)
{
    int ok = rt_label("array.unborrowed");
    x64_load(RCX, RAX, 32);
    x64_test(RCX, RCX);
    x64_jcc(CC_Z, ok);
    collection_fail("要素を ref で参照している配列は追加・削除・再代入できません\n");
    x64_bind(ok);
}

static void collection_shape(Type type, ArrayDim *dims, Reg to)
{
    Buffer shape;
    int rank = array_rank(type);
    Type base = array_base(type);
    buf_init(&shape);
    for (; rank > 0; rank--) {
        buf_add_u64(&shape, (uint64_t)rank);
        buf_add_u64(&shape, (uint64_t)base);
        buf_add_u64(&shape, dims ? (uint64_t)dims->limit : UINT64_MAX);
        if (dims) dims = dims->next;
    }
    x64_lea_data(to, prog_add_data(prog, (const char *)shape.data, shape.len));
    buf_free(&shape);
}

static void array_bounds(void)
{
    int bad = rt_label("array.bounds.bad");
    int ok = rt_label("array.bounds.ok");
    x64_test(RDI, RDI);
    x64_jcc(CC_S, bad);
    x64_load(RCX, RAX, 0);
    x64_cmp(RDI, RCX);
    x64_jcc(CC_B, ok);
    x64_bind(bad);
    collection_fail("配列の添字が範囲外です\n");
    x64_bind(ok);
}

static void gen_array_access_runtime(void)
{
    x64_bind(array_slot_label);
    array_bounds();
    x64_load(RAX, RAX, 24);
    x64_mov(RCX, RDI);
    x64_shl_imm(RCX, 3);
    x64_add(RAX, RCX);
    x64_ret();
    x64_bind(array_get_label);
    x64_call(array_slot_label);
    x64_load(RAX, RAX, 0);
    x64_ret();
}

/* rax=コピー元(0は初期配列)、rdi=代入先shape、rsi=不足分を埋めるか。 */
static void gen_array_copy_runtime(void)
{
    int source = rt_label("copy.source");
    int limit = rt_label("copy.limit");
    int unbounded = rt_label("copy.unbounded");
    int length_ok = rt_label("copy.length.ok");
    int allocate = rt_label("copy.allocate");
    int loop = rt_label("copy.loop");
    int missing = rt_label("copy.missing");
    int default_char = rt_label("copy.default.char");
    int value = rt_label("copy.value");
    int scalar = rt_label("copy.scalar");
    int store = rt_label("copy.store");
    int done = rt_label("copy.done");
    rt_enter(array_copy_label, 10);
    rt_save(1, RAX); rt_save(2, RDI); rt_save(8, RSI);
    x64_load(RCX, R14, COLLECTION_CONTEXT); rt_save(10, RCX);
    x64_xor(RCX, RCX);
    x64_test(RAX, RAX);
    x64_jcc(CC_NZ, source);
    rt_set(8, 1); /* 初期値なしの配列 */
    x64_jmp(limit);
    x64_bind(source);
    x64_load(RCX, RAX, 0);
    x64_bind(limit);
    rt_save(7, RCX); /* 元の長さ */
    x64_load(RDX, RDI, 16);
    x64_test(RDX, RDX);
    x64_jcc(CC_S, unbounded);
    x64_cmp(RCX, RDX);
    x64_jcc(CC_BE, length_ok);
    collection_fail("配列の要素数が代入先の上限を超えています\n");
    x64_bind(length_ok);
    rt_load(RSI, 8);
    x64_test(RSI, RSI);
    x64_jcc(CC_Z, unbounded);
    x64_mov(RCX, RDX);
    x64_bind(unbounded);
    rt_cmp(RCX, 0x0FFFFFFFFFFFFFF7LL);
    x64_jcc(CC_BE, allocate);
    collection_fail("配列に必要な領域が int の範囲を超えます\n");
    x64_bind(allocate);
    rt_save(3, RCX);
    x64_mov_imm(RDI, 40);
    x64_call(alloc_label);
    rt_save(4, RAX);
    x64_store_imm(RAX, 24, 0);
    x64_store_imm(RAX, 32, 0);
    rt_load(RCX, 3);
    x64_store(RAX, 0, RCX);
    x64_store(RAX, 8, RCX);
    rt_load(RDX, 2);
    x64_store(RAX, 16, RDX);
    x64_test(RCX, RCX);
    x64_jcc(CC_Z, done);
    x64_mov(RDI, RCX);
    x64_shl_imm(RDI, 3);
    x64_call(alloc_label);
    rt_save(6, RAX);
    rt_load(RDX, 4);
    x64_store(RDX, 24, RAX);
    rt_set(5, 0);
    x64_bind(loop);
    rt_load(RCX, 5); rt_load(RDX, 3);
    x64_cmp(RCX, RDX);
    x64_jcc(CC_AE, done);
    rt_load(RDX, 7);
    x64_cmp(RCX, RDX);
    x64_jcc(CC_AE, missing);
    rt_load(RAX, 1);
    x64_load(RAX, RAX, 24);
    x64_shl_imm(RCX, 3);
    x64_add(RAX, RCX);
    x64_load(RAX, RAX, 0);
    x64_jmp(value);
    x64_bind(missing);
    x64_xor(RAX, RAX);
    rt_load(RDX, 2);
    x64_load(RCX, RDX, 0);
    rt_cmp(RCX, 1);
    x64_jcc(CC_NZ, value);
    x64_load(RCX, RDX, 8);
    rt_cmp(RCX, TY_STR);
    x64_jcc(CC_NZ, default_char);
    x64_lea_data(RAX, empty_str_offset);
    x64_jmp(value);
    x64_bind(default_char);
    rt_cmp(RCX, TY_CHAR);
    x64_jcc(CC_NZ, value);
    x64_lea_data(RAX, zero_char_offset);
    x64_bind(value);
    rt_load(RDI, 2);
    x64_load(RCX, RDI, 0);
    rt_cmp(RCX, 1);
    x64_jcc(CC_Z, scalar);
    x64_add_imm(RDI, 24);
    rt_load(RSI, 8);
    rt_load(R11, 10); x64_store(R14, COLLECTION_CONTEXT, R11);
    x64_call(array_copy_label);
    x64_jmp(store);
    x64_bind(scalar);
    x64_load(RCX, RDI, 8);
    { Model *m;
      for (m = emitting_models; m; m = m->next) {
          int next_model = rt_label("copy.not.model");
          rt_cmp(RCX, m->ty); x64_jcc(CC_NZ, next_model);
          { int existing = rt_label("copy.model.existing");
            rt_load(RCX, 5); rt_load(RDX, 7); x64_cmp(RCX, RDX); x64_jcc(CC_B, existing);
            rt_load(RAX, 10); x64_push(RAX); x64_call(m->default_builder->label); x64_add_imm(RSP, 8); x64_jmp(store);
            x64_bind(existing); x64_call(m->copy_label); x64_jmp(store);
          }
          x64_bind(next_model);
      }
    }
    x64_bind(store);
    rt_load(RDX, 6); rt_load(RCX, 5);
    x64_shl_imm(RCX, 3);
    x64_add(RDX, RCX);
    x64_store(RDX, 0, RAX);
    rt_load(RCX, 5);
    x64_add_imm(RCX, 1);
    rt_save(5, RCX);
    x64_jmp(loop);
    x64_bind(done);
    rt_load(RAX, 4);
    rt_leave();
}

/* rax=配列、rdi=追加する値。子配列のコピー・領域確保の成功後にだけ変更する。 */
static void gen_array_add_runtime(void)
{
    int allowed = rt_label("add.allowed");
    int scalar = rt_label("add.scalar");
    int capacity = rt_label("add.capacity");
    int grow = rt_label("add.grow");
    int limited = rt_label("add.limited");
    int allocate = rt_label("add.allocate");
    int append = rt_label("add.append");
    int model_done = rt_label("add.model.done");
    rt_enter(array_add_label, 6);
    array_require_unborrowed();
    rt_save(1, RAX); rt_save(2, RDI);
    x64_load(RCX, RAX, 0);
    x64_load(RDX, RAX, 16);
    x64_load(RSI, RDX, 16);
    x64_test(RSI, RSI);
    x64_jcc(CC_S, allowed);
    x64_cmp(RCX, RSI);
    x64_jcc(CC_B, allowed);
    collection_fail("配列の追加が最大要素数を超えています\n");
    x64_bind(allowed);
    x64_load(RCX, RDX, 0);
    rt_cmp(RCX, 1);
    x64_jcc(CC_Z, scalar);
    rt_load(RAX, 2);
    x64_lea(RDI, RDX, 24);
    x64_xor(RSI, RSI);
    x64_call(array_copy_label);
    rt_save(2, RAX);
    x64_bind(scalar);
    rt_load(RAX, 1); x64_load(RDX, RAX, 16); x64_load(RCX, RDX, 8);
    x64_load(RSI, RDX, 0); rt_cmp(RSI, 1); x64_jcc(CC_NZ, model_done);
    { Model *m;
      for (m = emitting_models; m; m = m->next) {
          int next_model = rt_label("add.not.model");
          rt_cmp(RCX, m->ty); x64_jcc(CC_NZ, next_model);
          rt_load(RAX, 2); x64_call(m->copy_label); rt_save(2, RAX);
          x64_bind(next_model);
          rt_load(RAX, 1); x64_load(RDX, RAX, 16); x64_load(RCX, RDX, 8);
      }
    }
    x64_bind(model_done);
    rt_load(RAX, 1);
    x64_load(RCX, RAX, 0);
    x64_load(RDX, RAX, 8);
    x64_cmp(RCX, RDX);
    x64_jcc(CC_B, append);
    x64_test(RDX, RDX);
    x64_jcc(CC_NZ, grow);
    x64_mov_imm(RDX, 4);
    x64_jmp(limited);
    x64_bind(grow);
    rt_cmp(RDX, 0x07FFFFFFFFFFFFFBLL);
    x64_jcc(CC_BE, capacity);
    collection_fail("配列の容量が int の範囲を超えます\n");
    x64_bind(capacity);
    x64_add(RDX, RDX);
    x64_bind(limited);
    x64_load(RSI, RAX, 16);
    x64_load(RSI, RSI, 16);
    x64_test(RSI, RSI);
    x64_jcc(CC_S, allocate);
    x64_cmp(RDX, RSI);
    x64_jcc(CC_BE, allocate);
    x64_mov(RDX, RSI);
    x64_bind(allocate);
    rt_save(3, RDX);
    x64_mov(RDI, RDX);
    x64_shl_imm(RDI, 3);
    x64_call(alloc_label);
    rt_save(4, RAX);
    rt_load(RDX, 1);
    x64_load(RSI, RDX, 24);
    x64_load(RCX, RDX, 0);
    x64_shl_imm(RCX, 3);
    x64_mov(RDI, RAX);
    x64_rep_movsb();
    rt_load(RAX, 1); rt_load(RDX, 4);
    x64_store(RAX, 24, RDX);
    rt_load(RDX, 3);
    x64_store(RAX, 8, RDX);
    x64_bind(append);
    rt_load(RAX, 1);
    x64_load(RCX, RAX, 0);
    x64_load(RDX, RAX, 24);
    x64_mov(RSI, RCX);
    x64_shl_imm(RSI, 3);
    x64_add(RDX, RSI);
    rt_load(RDI, 2);
    x64_store(RDX, 0, RDI);
    x64_add_imm(RCX, 1);
    x64_store(RAX, 0, RCX);
    x64_xor(RAX, RAX);
    rt_leave();
}

static void gen_array_delete_runtime(void)
{
    int loop = rt_label("remove.loop");
    int done = rt_label("remove.done");
    int clear_loop = rt_label("clear.loop");
    int clear_done = rt_label("clear.done");
    x64_bind(array_remove_label);
    array_require_unborrowed();
    array_bounds();
    x64_load(RDX, RAX, 24);
    x64_shl_imm(RDI, 3);
    x64_add(RDX, RDI);
    x64_shl_imm(RCX, 3);
    x64_load(RSI, RAX, 24);
    x64_add(RSI, RCX);
    x64_sub_imm(RSI, 8);
    x64_bind(loop);
    x64_cmp(RDX, RSI);
    x64_jcc(CC_AE, done);
    x64_load(RCX, RDX, 8);
    x64_store(RDX, 0, RCX);
    x64_add_imm(RDX, 8);
    x64_jmp(loop);
    x64_bind(done);
    x64_store_imm(RDX, 0, 0);
    x64_load(RCX, RAX, 0);
    x64_sub_imm(RCX, 1);
    x64_store(RAX, 0, RCX);
    x64_xor(RAX, RAX);
    x64_ret();
    x64_bind(array_clear_label);
    array_require_unborrowed();
    x64_load(RCX, RAX, 0);
    x64_load(RDX, RAX, 24);
    x64_store_imm(RAX, 0, 0);
    x64_bind(clear_loop);
    x64_test(RCX, RCX);
    x64_jcc(CC_Z, clear_done);
    x64_store_imm(RDX, 0, 0);
    x64_add_imm(RDX, 8);
    x64_sub_imm(RCX, 1);
    x64_jmp(clear_loop);
    x64_bind(clear_done);
    x64_xor(RAX, RAX);
    x64_ret();
}

static void gen_array_equal_runtime(void)
{
    int loop = rt_label("equal.loop");
    int scalar = rt_label("equal.scalar");
    int text_value = rt_label("equal.text");
    int float_value = rt_label("equal.float");
    int double_value = rt_label("equal.double");
    int real_compare = rt_label("equal.real");
    int next = rt_label("equal.next");
    int yes = rt_label("equal.yes");
    int no = rt_label("equal.no");
    int done = rt_label("equal.done");
    rt_enter(array_equal_label, 8);
    rt_save(1, RAX); rt_save(2, RDI);
    x64_load(RCX, RAX, 0); x64_load(RDX, RDI, 0);
    x64_cmp(RCX, RDX);
    x64_jcc(CC_NZ, no);
    rt_save(4, RCX);
    x64_load(RDX, RAX, 16);
    x64_load(RCX, RDX, 0); rt_save(5, RCX);
    x64_load(RCX, RDX, 8); rt_save(6, RCX);
    rt_set(3, 0);
    x64_bind(loop);
    rt_load(RCX, 3); rt_load(RDX, 4);
    x64_cmp(RCX, RDX);
    x64_jcc(CC_AE, yes);
    x64_shl_imm(RCX, 3);
    rt_load(RAX, 1); x64_load(RAX, RAX, 24); x64_add(RAX, RCX); x64_load(RAX, RAX, 0); rt_save(7, RAX);
    rt_load(RDI, 2); x64_load(RDI, RDI, 24); x64_add(RDI, RCX); x64_load(RDI, RDI, 0); rt_save(8, RDI);
    rt_load(RCX, 5); rt_cmp(RCX, 1);
    x64_jcc(CC_Z, scalar);
    x64_call(array_equal_label);
    x64_test(RAX, RAX); x64_jcc(CC_Z, no); x64_jmp(next);
    x64_bind(scalar);
    rt_load(RCX, 6);
    { Model *m;
      for (m = emitting_models; m; m = m->next) {
          int next_model = rt_label("equal.not.model");
          rt_cmp(RCX, m->ty); x64_jcc(CC_NZ, next_model);
          x64_call(m->equal_label); x64_test(RAX, RAX); x64_jcc(CC_Z, no); x64_jmp(next);
          x64_bind(next_model);
      }
    }
    { int scalar_value = rt_label("equal.not.complex"), complex_value = rt_label("equal.complex");
      rt_cmp(RCX, TY_C64); x64_jcc(CC_Z, complex_value); rt_cmp(RCX, TY_C128); x64_jcc(CC_NZ, scalar_value);
      x64_bind(complex_value); x64_call(complex_equal_label); x64_test(RAX, RAX); x64_jcc(CC_Z, no); x64_jmp(next);
      x64_bind(scalar_value);
    }
    rt_cmp(RCX, TY_STR); x64_jcc(CC_Z, text_value);
    rt_cmp(RCX, TY_CHAR); x64_jcc(CC_Z, text_value);
    rt_cmp(RCX, TY_FLOAT); x64_jcc(CC_Z, float_value);
    rt_cmp(RCX, TY_DOUBLE); x64_jcc(CC_Z, double_value);
    x64_cmp(RAX, RDI); x64_jcc(CC_NZ, no); x64_jmp(next);
    x64_bind(text_value);
    x64_call(str_eq_label);
    x64_test(RAX, RAX); x64_jcc(CC_Z, no); x64_jmp(next);
    x64_bind(float_value);
    x64_movd_to_xmm(0, RAX); x64_cvtss2sd(0, 0);
    x64_movd_to_xmm(1, RDI); x64_cvtss2sd(1, 1);
    x64_jmp(real_compare);
    x64_bind(double_value);
    x64_movq_to_xmm(0, RAX); x64_movq_to_xmm(1, RDI);
    x64_bind(real_compare);
    x64_ucomisd(0, 1); x64_jcc(CC_P, no); x64_jcc(CC_NZ, no);
    x64_bind(next);
    rt_load(RCX, 3); x64_add_imm(RCX, 1); rt_save(3, RCX); x64_jmp(loop);
    x64_bind(yes); x64_mov_imm(RAX, 1); x64_jmp(done);
    x64_bind(no); x64_xor(RAX, RAX);
    x64_bind(done); rt_leave();
}

static void gen_array_ref_runtime(void)
{
    int loop = rt_label("ref.shape.loop");
    int next = rt_label("ref.shape.next");
    int bad = rt_label("ref.shape.bad");
    int done = rt_label("ref.shape.done");
    rt_enter(array_ref_label, 1);
    rt_save(1, RAX);
    x64_test(RAX, RAX); x64_jcc(CC_Z, done);
    x64_load(RDX, RAX, 0); x64_load(RDX, RDX, 16);
    x64_load(RCX, RDI, 0);
    x64_bind(loop);
    x64_test(RCX, RCX); x64_jcc(CC_Z, done);
    x64_load(R8, RDI, 16);
    x64_test(R8, R8); x64_jcc(CC_S, next);
    x64_load(R9, RDX, 16);
    x64_test(R9, R9); x64_jcc(CC_S, bad);
    x64_cmp(R9, R8); x64_jcc(CC_A, bad);
    x64_bind(next);
    x64_add_imm(RDI, 24); x64_add_imm(RDX, 24); x64_sub_imm(RCX, 1); x64_jmp(loop);
    x64_bind(bad);
    collection_fail("ref 配列の上限が宣言された条件を満たしません\n");
    x64_bind(done); rt_load(RAX, 1); rt_leave();
}

static void gen_string_get_runtime(void)
{
    int valid = rt_label("string.get.valid");
    int found = rt_label("string.get.found");
    rt_enter(string_get_label, 3);
    x64_test(RDI, RDI); x64_jcc(CC_NS, valid);
    collection_fail("文字列の添字が範囲外です\n");
    x64_bind(valid);
    x64_call(string_scan_label);
    x64_test(R8, R8); x64_jcc(CC_Z, found);
    collection_fail("不正な UTF-8 文字列です\n");
    x64_bind(found);
    found = rt_label("string.get.present");
    x64_test(RDX, RDX); x64_jcc(CC_NZ, found);
    collection_fail("文字列の添字が範囲外です\n");
    x64_bind(found);
    rt_save(1, RDX); rt_save(2, RCX);
    x64_lea(RDI, RCX, 8); x64_call(alloc_label); rt_save(3, RAX);
    rt_load(RCX, 2); x64_store(RAX, 0, RCX);
    x64_lea(RDI, RAX, 8); rt_load(RSI, 1); x64_rep_movsb();
    rt_load(RAX, 3); rt_leave();
}

/* rax=元の str、rdi=添字、rsi=1スカラー値の置換文字列。 */
static void gen_string_replace_runtime(void)
{
    int valid = rt_label("string.replace.valid");
    int found = rt_label("string.replace.found");
    int size_ok = rt_label("string.replace.size.ok");
    rt_enter(string_replace_label, 9);
    rt_save(1, RAX); rt_save(2, RDI); rt_save(3, RSI);
    x64_test(RDI, RDI); x64_jcc(CC_NS, valid);
    collection_fail("文字列の添字が範囲外です\n");
    x64_bind(valid);
    x64_call(string_scan_label);
    x64_test(R8, R8); x64_jcc(CC_Z, found);
    collection_fail("不正な UTF-8 文字列です\n");
    x64_bind(found);
    found = rt_label("string.replace.present");
    x64_test(RDX, RDX); x64_jcc(CC_NZ, found);
    collection_fail("文字列の添字が範囲外です\n");
    x64_bind(found);
    rt_save(4, RDX); rt_save(5, RCX);
    rt_load(RAX, 1); x64_load(RAX, RAX, 0);
    x64_sub(RAX, RCX);
    rt_load(RSI, 3); x64_load(RCX, RSI, 0); x64_add(RAX, RCX);
    rt_cmp(RAX, 0x7FFFFFFFFFFFFFD0LL); x64_jcc(CC_BE, size_ok);
    collection_fail("置換後の文字列が大きすぎます\n");
    x64_bind(size_ok);
    rt_save(6, RAX); x64_lea(RDI, RAX, 8); x64_call(alloc_label); rt_save(7, RAX);
    rt_load(RCX, 6); x64_store(RAX, 0, RCX);
    x64_lea(RDI, RAX, 8);
    rt_load(RSI, 1); x64_add_imm(RSI, 8);
    rt_load(RCX, 4); x64_sub(RCX, RSI);
    x64_rep_movsb();
    rt_load(RSI, 3); x64_load(RCX, RSI, 0); x64_add_imm(RSI, 8); x64_rep_movsb();
    rt_load(RSI, 4); rt_load(RCX, 5); x64_add(RSI, RCX);
    rt_load(RDX, 1); x64_load(RCX, RDX, 0); x64_add_imm(RDX, 8); x64_add(RDX, RCX);
    x64_mov(RCX, RDX); x64_sub(RCX, RSI); x64_rep_movsb();
    rt_load(RAX, 7); rt_leave();
}

static void gen_quoted_string_runtime(void)
{
    int loop = rt_label("quoted.loop");
    int next = rt_label("quoted.next");
    int done = rt_label("quoted.done");
    int special[6];
    int chars[] = {'"', '\\', '\n', '\r', '\t', 0};
    const char *escaped[] = {"\\\"", "\\\\", "\\n", "\\r", "\\t", "\\0"};
    int i;
    for (i = 0; i < 6; i++) special[i] = rt_label("quoted.escape");
    rt_enter(quoted_string_label, 3);
    rt_save(1, RAX); x64_load(RCX, RAX, 0); rt_save(2, RCX); rt_set(3, 0);
    gen_write_const("\"");
    x64_bind(loop);
    rt_load(RCX, 3); rt_load(RDX, 2); x64_cmp(RCX, RDX); x64_jcc(CC_AE, done);
    rt_load(RSI, 1); x64_add_imm(RSI, 8); x64_add(RSI, RCX); x64_load8(RAX, RSI, 0);
    for (i = 0; i < 6; i++) { rt_cmp(RAX, chars[i]); x64_jcc(CC_Z, special[i]); }
    x64_mov_imm(RDX, 1); x64_call(write_label); x64_jmp(next);
    for (i = 0; i < 6; i++) { x64_bind(special[i]); gen_write_const(escaped[i]); x64_jmp(next); }
    x64_bind(next); rt_load(RCX, 3); x64_add_imm(RCX, 1); rt_save(3, RCX); x64_jmp(loop);
    x64_bind(done); gen_write_const("\""); rt_leave();
}

static void gen_array_print_runtime(void)
{
    int loop = rt_label("print.array.loop");
    int value = rt_label("print.array.value");
    int scalar = rt_label("print.array.scalar");
    int next = rt_label("print.array.next");
    int done = rt_label("print.array.done");
    int primitive[TY_CHAR + 1];
    int i;
    for (i = TY_I64; i <= TY_CHAR; i++) primitive[i] = rt_label("print.array.primitive");
    rt_enter(array_print_label, 5);
    rt_save(1, RAX); rt_set(2, 0);
    x64_load(RDX, RAX, 16);
    x64_load(RCX, RDX, 0); rt_save(3, RCX);
    x64_load(RCX, RDX, 8); rt_save(4, RCX);
    gen_write_const("[");
    x64_bind(loop);
    rt_load(RAX, 1); x64_load(RDX, RAX, 0); rt_load(RCX, 2);
    x64_cmp(RCX, RDX); x64_jcc(CC_AE, done);
    x64_test(RCX, RCX); x64_jcc(CC_Z, value); gen_write_const(", ");
    x64_bind(value);
    rt_load(RAX, 1); x64_load(RAX, RAX, 24); rt_load(RCX, 2); x64_shl_imm(RCX, 3); x64_add(RAX, RCX); x64_load(RAX, RAX, 0);
    rt_load(RCX, 3); rt_cmp(RCX, 1); x64_jcc(CC_Z, scalar);
    x64_call(array_print_label); x64_jmp(next);
    x64_bind(scalar); rt_load(RCX, 4);
    { Model *m;
      for (m = emitting_models; m; m = m->next) {
          int next_model = rt_label("print.not.model");
          rt_cmp(RCX, m->ty); x64_jcc(CC_NZ, next_model);
          x64_call(m->print_label); x64_jmp(next);
          x64_bind(next_model);
      }
    }
    for (i = TY_I64; i <= TY_CHAR; i++) { rt_cmp(RCX, i); x64_jcc(CC_Z, primitive[i]); }
    x64_jmp(next);
    for (i = TY_I64; i <= TY_CHAR; i++) {
        x64_bind(primitive[i]);
        if (i == TY_FLOAT) { x64_movd_to_xmm(0, RAX); x64_cvtss2sd(0, 0); x64_movq_from_xmm(RAX, 0); }
        x64_call(type_is_complex((Type)i) ? complex_print_labels[i == TY_C128] : type_is_integer((Type)i) ? (integer_unsigned((Type)i) ? print_uint_label : print_int_label) : i == TY_BOOL ? print_bool_label :
                 i == TY_FLOAT ? print_float_label : i == TY_DOUBLE ? print_double_label : quoted_string_label);
        x64_jmp(next);
    }
    x64_bind(next); rt_load(RCX, 2); x64_add_imm(RCX, 1); rt_save(2, RCX); x64_jmp(loop);
    x64_bind(done); gen_write_const("]"); rt_leave();
}
