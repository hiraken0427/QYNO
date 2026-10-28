/* Byte arrays use ordinary eight-byte scalar slots. File data is packed only
 * in finite OS buffers. Source arrays are borrowed, never cloned or mutated. */
static void gen_file_binary_runtime(void)
{
    int loop = rt_label("file.bytes.read.loop"), done = rt_label("file.bytes.read.done");
    int valid = rt_label("file.bytes.count.valid");
    rt_enter(file_labels[BI_FILE_READ_BYTES - BI_FILE_OPEN], 4);
    x64_load(RAX, RBP, 24); rt_save(1, RAX);
    x64_mov_imm(RDI, 5); x64_call(file_check_label);
    x64_load(RAX, RBP, 16); x64_test(RAX, RAX); x64_jcc(CC_NS, valid);
    file_fail(EX_SOFTWARE, "read_bytes の count は非負にしてください");
    x64_bind(valid); rt_save(2, RAX);
    x64_load(RCX, R14, FILE_CONTEXT); x64_store(R14, COLLECTION_CONTEXT, RCX);
    x64_mov_imm(RDI, 40); x64_call(alloc_label); rt_save(3, RAX);
    x64_store_imm(RAX, 0, 0); x64_store_imm(RAX, 8, 0);
    collection_shape(array_type(TY_U8), NULL, RDI); x64_store(RAX, 16, RDI);
    x64_store_imm(RAX, 24, 0); x64_store_imm(RAX, 32, 0);
    x64_bind(loop); rt_load(RAX, 2); x64_test(RAX, RAX); x64_jcc(CC_Z, done);
    rt_load(RAX, 1); file_io_call(file_getc_label); rt_cmp(RAX, -1); x64_jcc(CC_Z, done);
    x64_mov(RDI, RAX); rt_load(RAX, 3); x64_call(array_add_label);
    rt_load(RAX, 2); x64_sub_imm(RAX, 1); rt_save(2, RAX); x64_jmp(loop);
    x64_bind(done); rt_load(RAX, 3); rt_leave();

    loop = rt_label("file.bytes.write.loop"); done = rt_label("file.bytes.write.done");
    { int pack = rt_label("file.bytes.pack"), packed = rt_label("file.bytes.packed");
      int limited = rt_label("file.bytes.write.limited");
      rt_enter(file_labels[BI_FILE_WRITE_BYTES - BI_FILE_OPEN], 6);
      x64_load(RAX, RBP, 24); rt_save(1, RAX); x64_mov_imm(RDI, 6); x64_call(file_check_label);
      x64_load(RAX, RBP, 16); rt_save(2, RAX); rt_set(3, 0);
      x64_load(RCX, RAX, 0); x64_test(RCX, RCX); x64_jcc(CC_Z, done);
      x64_mov_imm(RDI, FILE_BUFFER_SIZE); x64_call(alloc_label); rt_save(4, RAX);
      x64_bind(loop); rt_load(RAX, 2); x64_load(RDX, RAX, 0); rt_load(RCX, 3);
      x64_cmp(RCX, RDX); x64_jcc(CC_AE, done); x64_sub(RDX, RCX);
      rt_cmp(RDX, FILE_BUFFER_SIZE); x64_jcc(CC_BE, limited); x64_mov_imm(RDX, FILE_BUFFER_SIZE);
      x64_bind(limited); rt_save(5, RDX);
      x64_load(RSI, RAX, 24); x64_shl_imm(RCX, 3); x64_add(RSI, RCX);
      rt_load(RDI, 4); x64_xor(RCX, RCX);
      x64_bind(pack); x64_cmp(RCX, RDX); x64_jcc(CC_AE, packed);
      x64_load(R8, RSI, 0); x64_store8(RDI, 0, R8); x64_add_imm(RSI, 8); x64_add_imm(RDI, 1);
      x64_add_imm(RCX, 1); x64_jmp(pack);
      x64_bind(packed); rt_load(RAX, 1); rt_load(RSI, 4); rt_load(RDX, 5); file_io_call(file_write_all_label);
      rt_load(RCX, 3); x64_add(RAX, RCX); rt_save(3, RAX); x64_jmp(loop);
      x64_bind(done); rt_load(RAX, 3); rt_leave(); }
}
