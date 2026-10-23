/* 管理領域 [次の領域, ペイロードのバイト数, mark] [ペイロード]。
 * r12 は領域リスト、r13 は前回回収後の確保量。生アドレスを公開しない配列にも、
 * 既存の不変文字列にも使う保守的な mark/sweep。収集時は全ての値レジスタを退避する。
 */
static int rt_sequence;

#ifndef QYNO_GC_INTERVAL
#define QYNO_GC_INTERVAL 1048576
#endif

static int rt_label(const char *kind)
{
    char name[96];
    snprintf(name, sizeof(name), ".L.rt.%d.%s", rt_sequence++, kind);
    return x64_new_label(name);
}

static void rt_enter(int label, int slots)
{
    x64_bind(label);
    x64_push(RBP);
    x64_mov(RBP, RSP);
    if (slots) x64_sub_imm(RSP, (slots * 8 + 15) / 16 * 16);
}

static void rt_leave(void)
{
    x64_mov(RSP, RBP);
    x64_pop(RBP);
    x64_ret();
}

static void rt_load(Reg reg, int slot) { x64_load(reg, RBP, -slot * 8); }
static void rt_save(int slot, Reg reg) { x64_store(RBP, -slot * 8, reg); }
static void rt_set(int slot, long long value) { x64_store_imm(RBP, -slot * 8, value); }
static void rt_cmp(Reg reg, long long value) { x64_mov_imm(R11, value); x64_cmp(reg, R11); }

static void gen_raw_memory(void)
{
    int ok = rt_label("raw.alloc.ok");
    x64_bind(raw_alloc_label);
    if (target == TARGET_LINUX) {
        x64_mov(RSI, RDI);
        x64_xor(RDI, RDI);
        x64_mov_imm(RDX, 3);
        x64_mov_imm(R10, 0x22);
        x64_mov_imm(R8, -1);
        x64_xor(R9, R9);
        x64_mov_imm(RAX, 9);
        x64_syscall();
        rt_cmp(RAX, -4095);
        x64_jcc(CC_B, ok);
        x64_xor(RAX, RAX);
        x64_bind(ok);
        x64_ret();
    } else {
        x64_push(RBP);
        x64_mov(RBP, RSP);
        x64_push(RDI);
        x64_and_imm(RSP, -16);
        x64_sub_imm(RSP, 32);
        x64_call_import(IMP_GET_PROCESS_HEAP);
        x64_mov(RCX, RAX);
        x64_mov_imm(RDX, 8);  /* HEAP_ZERO_MEMORY */
        rt_load(R8, 1);
        x64_call_import(IMP_HEAP_ALLOC);
        rt_leave();
    }
    x64_bind(raw_free_label);
    if (target == TARGET_LINUX) {
        x64_mov_imm(RAX, 11);
        x64_syscall();
        x64_ret();
    } else {
        x64_push(RBP);
        x64_mov(RBP, RSP);
        x64_push(RDI);
        x64_and_imm(RSP, -16);
        x64_sub_imm(RSP, 32);
        x64_call_import(IMP_GET_PROCESS_HEAP);
        x64_mov(RCX, RAX);
        x64_xor(RDX, RDX);
        rt_load(R8, 1);
        x64_call_import(IMP_HEAP_FREE);
        rt_leave();
    }
}

/* 候補 rax が領域内部を指すなら、その領域と含まれる参照をマークする。 */
static void gen_gc_mark(void)
{
    int search = rt_label("mark.search");
    int next = rt_label("mark.next");
    int scan = rt_label("mark.scan");
    int done = rt_label("mark.done");
    rt_enter(gc_mark_label, 3);
    /* 回収対象全体のアドレス範囲外の整数・浮動小数点値を先に除外する。 */
    x64_cmp(RAX, RBX);
    x64_jcc(CC_B, done);
    x64_cmp(RAX, R10);
    x64_jcc(CC_AE, done);
    x64_mov(RDX, R12);
    x64_bind(search);
    x64_test(RDX, RDX);
    x64_jcc(CC_Z, done);
    x64_lea(RCX, RDX, 24);
    x64_cmp(RAX, RCX);
    x64_jcc(CC_B, next);
    x64_load(RDI, RDX, 8);
    x64_add(RCX, RDI);
    x64_cmp(RAX, RCX);
    x64_jcc(CC_AE, next);
    x64_load(RCX, RDX, 16);
    x64_test(RCX, RCX);
    x64_jcc(CC_NZ, done);
    x64_store_imm(RDX, 16, 1);
    rt_save(2, RDX);
    rt_set(3, 0);
    x64_bind(scan);
    rt_load(RDX, 2);
    rt_load(RCX, 3);
    x64_load(RDI, RDX, 8);
    x64_cmp(RCX, RDI);
    x64_jcc(CC_AE, done);
    x64_add(RDX, RCX);
    x64_load(RAX, RDX, 24);
    x64_call(gc_mark_label);
    rt_load(RCX, 3);
    x64_add_imm(RCX, 8);
    rt_save(3, RCX);
    x64_jmp(scan);
    x64_bind(next);
    x64_load(RDX, RDX, 0);
    x64_jmp(search);
    x64_bind(done);
    rt_leave();
}

static void gen_gc_collect(void)
{
    int reset = rt_label("gc.reset");
    int roots = rt_label("gc.roots");
    int root_loop = rt_label("gc.root.loop");
    int sweep = rt_label("gc.sweep");
    int keep = rt_label("gc.keep");
    int head = rt_label("gc.head");
    int free_block = rt_label("gc.free");
    int advance = rt_label("gc.advance");
    int done = rt_label("gc.done");
    int min_done = rt_label("gc.min.done");
    int max_done = rt_label("gc.max.done");
    rt_enter(gc_collect_label, 4);
    rt_save(1, RAX);
    x64_mov_imm(RBX, -1);
    x64_xor(R10, R10);
    x64_mov(RDX, R12);
    x64_bind(reset);
    x64_test(RDX, RDX);
    x64_jcc(CC_Z, roots);
    x64_store_imm(RDX, 16, 0);
    x64_lea(RAX, RDX, 24);
    x64_cmp(RAX, RBX);
    x64_jcc(CC_AE, min_done);
    x64_mov(RBX, RAX);
    x64_bind(min_done);
    x64_load(RCX, RDX, 8);
    x64_add(RAX, RCX);
    x64_cmp(RAX, R10);
    x64_jcc(CC_BE, max_done);
    x64_mov(R10, RAX);
    x64_bind(max_done);
    x64_load(RDX, RDX, 0);
    x64_jmp(reset);
    x64_bind(roots);
    x64_bind(root_loop);
    rt_load(RDX, 1);
    x64_cmp(RDX, R15);
    x64_jcc(CC_AE, sweep);
    x64_load(RAX, RDX, 0);
    x64_add_imm(RDX, 8);
    rt_save(1, RDX);
    x64_call(gc_mark_label);
    x64_jmp(root_loop);
    x64_bind(sweep);
    rt_set(2, 0); /* 前の領域 */
    rt_save(3, R12);
    sweep = rt_label("gc.sweep.loop");
    x64_bind(sweep);
    rt_load(RDX, 3);
    x64_test(RDX, RDX);
    x64_jcc(CC_Z, done);
    x64_load(RAX, RDX, 0);
    rt_save(4, RAX);
    x64_load(RCX, RDX, 16);
    x64_test(RCX, RCX);
    x64_jcc(CC_NZ, keep);
    rt_load(RCX, 2);
    x64_test(RCX, RCX);
    x64_jcc(CC_Z, head);
    x64_store(RCX, 0, RAX);
    x64_jmp(free_block);
    x64_bind(head);
    x64_mov(R12, RAX);
    x64_bind(free_block);
    rt_load(RDI, 3);
    x64_load(RSI, RDI, 8);
    x64_add_imm(RSI, 24);
    x64_call(raw_free_label);
    x64_jmp(advance);
    x64_bind(keep);
    rt_save(2, RDX);
    x64_bind(advance);
    rt_load(RAX, 4);
    rt_save(3, RAX);
    x64_jmp(sweep);
    x64_bind(done);
    x64_xor(R13, R13);
    rt_leave();
}

static void gen_alloc(void)
{
    Reg saved[] = {RAX, RCX, RDX, RBX, RSI, RDI, R8, R9, R10, R11, R14};
    int skip = rt_label("alloc.skip.gc");
    int allocated = rt_label("alloc.ok");
    int oom = rt_label("alloc.oom");
    int i;
    rt_enter(alloc_label, 14);
    for (i = 0; i < 11; i++) rt_save(i + 1, saved[i]);
    rt_cmp(RDI, 0x7FFFFFFFFFFFFFDFLL);
    x64_jcc(CC_A, oom);
    x64_add_imm(RDI, 7);
    x64_and_imm(RDI, -8);
    x64_add_imm(RDI, 24);
    rt_save(12, RDI);
    x64_add(R13, RDI);
    rt_cmp(R13, QYNO_GC_INTERVAL);
    x64_jcc(CC_B, skip);
    x64_mov(RAX, RSP);
    x64_call(gc_collect_label);
    x64_bind(skip);
    rt_load(RDI, 12);
    x64_call(raw_alloc_label);
    x64_test(RAX, RAX);
    x64_jcc(CC_NZ, allocated);
    x64_mov(RAX, RSP);
    x64_call(gc_collect_label);
    rt_load(RDI, 12);
    x64_call(raw_alloc_label);
    x64_test(RAX, RAX);
    x64_jcc(CC_Z, oom);
    x64_bind(allocated);
    x64_store(RAX, 0, R12);
    rt_load(RCX, 12);
    x64_sub_imm(RCX, 24);
    x64_store(RAX, 8, RCX);
    x64_store_imm(RAX, 16, 0);
    x64_mov(R12, RAX);
    x64_add_imm(RAX, 24);
    rt_save(13, RAX);
    for (i = 1; i < 11; i++) rt_load(saved[i], i + 1);
    rt_load(RAX, 13);
    rt_leave();
    x64_bind(oom);
    gen_fail(NULL, "必要なメモリを確保できません（サイズが範囲外か、メモリ不足です）", EX_SOFTWARE);
}
