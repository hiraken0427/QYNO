/* io.file: managed handle [OS handle, mode(0=closed,1=r,2=w,3=a,bit2=binary), path, pos, len, buffer].
 * Each handle owns a 4096-byte read buffer. Values and array copies share the
 * managed handle. Allocation flag bit 1 identifies finalizable handles.
 */
#define FILE_CONTEXT 264
#define FILE_PATH 272
#define FILE_OS_ERROR 280
#define OUTPUT_SINK 288
#ifndef QYNO_FILE_BUFFER_SIZE
#define QYNO_FILE_BUFFER_SIZE 4096
#endif
#define FILE_BUFFER_SIZE QYNO_FILE_BUFFER_SIZE

static void file_io_call(int label)
{
    int ok=rt_label("io.runtime.ok");
    x64_call(label);x64_load(R11,R14,IO_PENDING);x64_test(R11,R11);x64_jcc(CC_Z,ok);
    rt_leave();x64_bind(ok);
}

static void file_win_begin(void)
{
    x64_push(RBP); x64_mov(RBP, RSP);
    x64_and_imm(RSP, -16); x64_sub_imm(RSP, 64);
}

static void file_win_end(Import imp)
{
    x64_call_import(imp); x64_mov(RSP, RBP); x64_pop(RBP);
}

static void file_fail(int code, const char *reason)
{
    x64_lea_data(RAX, prog_add_str(prog, reason, strlen(reason)));
    if(code==EX_IOERR){
        x64_mov_imm(RDI,11);x64_call(io_raise_label);rt_leave();
    }else {x64_mov_imm(R8, code); x64_jmp(file_error_label);}
}

/* Raw OS helpers return negative OS error numbers. */
static void file_os_fail(const char *reason)
{
    x64_neg(RAX); x64_store(R14, FILE_OS_ERROR, RAX);
    file_fail(EX_IOERR, reason);
}

static void file_windows_error(void)
{
    file_win_begin(); file_win_end(IMP_GET_LAST_ERROR); x64_neg(RAX);
}

static void file_emit_err(const char *text_value)
{
    x64_lea_data(RSI, prog_add_data(prog, text_value, strlen(text_value)));
    x64_mov_imm(RDX, (long long)strlen(text_value)); x64_call(write_err_label);
}

static void gen_file_error_runtime(void)
{
    int no_context = rt_label("file.error.no.context");
    int no_path = rt_label("file.error.no.path");
    int no_os = rt_label("file.error.no.os");
    int digits = rt_label("file.error.digits");
    x64_bind(file_error_label);
    x64_push(R8); x64_call(io_report_label); x64_pop(RAX);x64_jmp(exit_label);
    rt_enter(io_report_label, 8);
    rt_save(1, RAX); rt_save(2, R8);
    x64_load(RAX, R14, FILE_CONTEXT); x64_test(RAX, RAX);
    x64_jcc(CC_Z, no_context);
    x64_load(RDX, RAX, 0); x64_lea(RSI, RAX, 8); x64_call(write_err_label);
    x64_bind(no_context);
    rt_load(RAX, 1); x64_load(RDX, RAX, 0); x64_lea(RSI, RAX, 8); x64_call(write_err_label);
    x64_load(RAX, R14, FILE_PATH); x64_test(RAX, RAX); x64_jcc(CC_Z, no_path);
    file_emit_err(" [path: ");
    x64_load(RAX, R14, FILE_PATH); x64_load(RDX, RAX, 0); x64_lea(RSI, RAX, 8); x64_call(write_err_label);
    file_emit_err("]");
    x64_bind(no_path);
    x64_load(RAX, R14, FILE_OS_ERROR); x64_test(RAX, RAX); x64_jcc(CC_Z, no_os);
    file_emit_err(" (OS error ");
    x64_load(RAX, R14, FILE_OS_ERROR); x64_lea(RDI, RBP, -16);
    x64_mov_imm(RCX, 10);
    x64_bind(digits);
    x64_xor(RDX, RDX); x64_div(RCX); x64_add_imm(RDX, '0');
    x64_sub_imm(RDI, 1); x64_store8(RDI, 0, RDX);
    x64_test(RAX, RAX); x64_jcc(CC_NZ, digits);
    x64_mov(RSI, RDI); x64_lea(RDX, RBP, -16); x64_sub(RDX, RDI); x64_call(write_err_label);
    file_emit_err(")");
    x64_bind(no_os);
    rt_load(R8, 2); x64_lea_data(RSI, newline_offset); x64_mov_imm(RDX, 1);
    x64_call(write_err_label);rt_leave();
}

/* rax=str -> same str, strictly validated. */
static void gen_file_validate_runtime(void)
{
    int valid = rt_label("file.utf8.valid");
    rt_enter(file_validate_label, 1); rt_save(1, RAX);
    x64_mov_imm(RDI, -1); x64_call(string_scan_label);
    x64_test(R8, R8); x64_jcc(CC_Z, valid);
    file_fail(EX_DATAERR, "不正な UTF-8 です");
    x64_bind(valid); rt_load(RAX, 1); rt_leave();
}

/* rax=str -> NUL terminated UTF-8 (Linux) / UTF-16 (Windows). */
static void gen_file_path_runtime(void)
{
    int loop = rt_label("file.path.scan");
    int checked = rt_label("file.path.checked");
    int no_nul = rt_label("file.path.no.nul");
    rt_enter(file_path_label, 4); rt_save(1, RAX);
    x64_call(file_validate_label);
    x64_load(RDX, RAX, 0); x64_lea(RSI, RAX, 8); x64_xor(RCX, RCX);
    x64_bind(loop); x64_cmp(RCX, RDX); x64_jcc(CC_AE, checked);
    x64_mov(RDI, RSI); x64_add(RDI, RCX); x64_load8(R8, RDI, 0);
    x64_test(R8, R8); x64_jcc(CC_NZ, no_nul);
    file_fail(EX_SOFTWARE, "パスに NUL は使用できません");
    x64_bind(no_nul); x64_add_imm(RCX, 1); x64_jmp(loop);
    x64_bind(checked);
    if (target == TARGET_LINUX) {
        x64_mov(RDI, RDX); x64_add_imm(RDI, 1); x64_call(alloc_label); rt_save(2, RAX);
        rt_load(RSI, 1); x64_load(RCX, RSI, 0); x64_add_imm(RSI, 8); x64_mov(RDI, RAX);
        x64_rep_movsb(); x64_store8_imm(RDI, 0, 0); rt_load(RAX, 2);
    } else {
        int length_ok = rt_label("file.path.length");
        int converted = rt_label("file.path.converted");
        rt_cmp(RDX, 0x7FFFFFFF); x64_jcc(CC_BE, length_ok);
        file_fail(EX_SOFTWARE, "パスが長すぎます");
        x64_bind(length_ok);
        x64_mov_imm(RCX, 65001); x64_mov_imm(RDX, 8); rt_load(R8, 1);
        x64_load(R9, R8, 0); x64_add_imm(R8, 8);
        file_win_begin(); x64_store_imm(RSP, 32, 0); x64_store_imm(RSP, 40, 0);
        file_win_end(IMP_MULTI_BYTE_TO_WIDE_CHAR);
        x64_test(RAX, RAX); x64_jcc(CC_NZ, converted);
        file_windows_error(); file_os_fail("パスを UTF-16 に変換できません");
        x64_bind(converted); rt_save(3, RAX);
        x64_mov(RDI, RAX); x64_shl_imm(RDI, 1); x64_add_imm(RDI, 2); x64_call(alloc_label); rt_save(2, RAX);
        x64_mov_imm(RCX, 65001); x64_mov_imm(RDX, 8); rt_load(R8, 1);
        x64_load(R9, R8, 0); x64_add_imm(R8, 8);
        rt_load(RAX, 2); rt_load(R10, 3);
        file_win_begin(); x64_store(RSP, 32, RAX); x64_store(RSP, 40, R10);
        file_win_end(IMP_MULTI_BYTE_TO_WIDE_CHAR);
        converted = rt_label("file.path.converted.again");
        x64_test(RAX, RAX); x64_jcc(CC_NZ, converted);
        file_windows_error(); file_os_fail("パスを UTF-16 に変換できません");
        x64_bind(converted); rt_load(RAX, 2); rt_load(RCX, 3); x64_shl_imm(RCX, 1); x64_add(RCX, RAX);
        x64_store8_imm(RCX, 0, 0); x64_store8_imm(RCX, 1, 0);
    }
    rt_leave();
}

/* rax=OS handle -> 0 or -error. Does not retry close after EINTR. */
static void gen_file_raw_close(void)
{
    x64_bind(file_raw_close_label);
    if (target == TARGET_LINUX) {
        x64_mov(RDI, RAX); x64_mov_imm(RAX, 3); x64_syscall(); x64_ret();
    } else {
        int good = rt_label("file.close.raw.good");
        x64_mov(RCX, RAX); file_win_begin(); file_win_end(IMP_CLOSE_HANDLE);
        x64_test(RAX, RAX); x64_jcc(CC_NZ, good);
        file_windows_error(); x64_ret();
        x64_bind(good); x64_xor(RAX, RAX); x64_ret();
    }
}

/* rax=handle control -> close quietly, used by sweep and exit. */
static void gen_file_finalizer(void)
{
    int done = rt_label("file.finalize.done");
    rt_enter(file_finalize_label, 0);
    x64_load(RCX, RAX, 8); x64_test(RCX, RCX); x64_jcc(CC_Z, done);
    x64_store_imm(RAX, 8, 0); x64_load(RAX, RAX, 0); x64_call(file_raw_close_label);
    x64_bind(done); rt_leave();
    {
        int loop = rt_label("file.cleanup.loop");
        int next = rt_label("file.cleanup.next");
        int end = rt_label("file.cleanup.end");
        rt_enter(file_cleanup_label, 1); rt_save(1, R12);
        x64_bind(loop); rt_load(RAX, 1); x64_test(RAX, RAX); x64_jcc(CC_Z, end);
        x64_load(RCX, RAX, 16); x64_and_imm(RCX, 2); x64_test(RCX, RCX); x64_jcc(CC_Z, next);
        x64_add_imm(RAX, 24); x64_call(file_finalize_label);
        x64_bind(next); rt_load(RAX, 1); x64_load(RAX, RAX, 0); rt_save(1, RAX); x64_jmp(loop);
        x64_bind(end); rt_leave();
    }
}

/* rax=OS handle -> 1 regular file, 0 other, or -error. */
static void gen_file_kind_runtime(void)
{
    int done = rt_label("file.kind.done");
    int regular = rt_label("file.kind.regular");
    rt_enter(file_kind_label, 22); rt_save(1, RAX);
    if (target == TARGET_LINUX) {
        int retry = rt_label("file.kind.retry");
        x64_bind(retry); rt_load(RDI, 1); x64_lea(RSI, RBP, -176); x64_mov_imm(RAX, 5); x64_syscall();
        rt_cmp(RAX, -4); x64_jcc(CC_Z, retry);
        x64_test(RAX, RAX); x64_jcc(CC_S, done);
        x64_load(RCX, RBP, -152); x64_and_imm(RCX, 0xF000); rt_cmp(RCX, 0x8000);
        x64_setcc(CC_Z, RAX); x64_movzx8(RAX, RAX);
    } else {
        x64_mov(RCX, RAX); file_win_begin(); file_win_end(IMP_GET_FILE_TYPE);
        rt_cmp(RAX, 1); x64_jcc(CC_NZ, regular);
        rt_load(RCX, 1); x64_lea(RDX, RBP, -176);
        file_win_begin(); file_win_end(IMP_GET_FILE_INFORMATION_BY_HANDLE);
        x64_test(RAX, RAX);
        { int ok = rt_label("file.kind.information"); x64_jcc(CC_NZ, ok);
          file_windows_error(); x64_jmp(done); x64_bind(ok); }
        x64_load(RCX, RBP, -176); x64_and_imm(RCX, 16); x64_test(RCX, RCX);
        x64_setcc(CC_Z, RAX); x64_movzx8(RAX, RAX); x64_jmp(done);
        x64_bind(regular);
        { int other = rt_label("file.kind.other"); x64_test(RAX, RAX); x64_jcc(CC_NZ, other);
          file_windows_error(); x64_jmp(done); x64_bind(other); }
        x64_xor(RAX, RAX);
    }
    x64_bind(done); rt_leave();
}

/* rax=OS pathname, rdi=mode (1=r,2=w,3=a,0=Windows exists). */
static void gen_file_raw_open(void)
{
    int read = rt_label("file.open.raw.read");
    int append = rt_label("file.open.raw.append");
    int call = rt_label("file.open.raw.call");
    int good = rt_label("file.open.raw.good");
    int retry = rt_label("file.open.raw.retry");
    rt_enter(file_raw_open_label, 3); rt_save(1, RAX); rt_save(2, RDI);
    if (target == TARGET_LINUX) {
        rt_cmp(RDI, 1); x64_jcc(CC_Z, read);
        rt_cmp(RDI, 3); x64_jcc(CC_Z, append);
        x64_mov_imm(RDX, 0x80000 | 0x800 | 1 | 64 | 512); x64_jmp(call);
        x64_bind(append); x64_mov_imm(RDX, 0x80000 | 0x800 | 1 | 64 | 1024); x64_jmp(call);
        x64_bind(read); x64_mov_imm(RDX, 0x80000 | 0x800);
        x64_bind(call); rt_save(3, RDX);
        x64_bind(retry); x64_mov_imm(RDI, -100); rt_load(RSI, 1); rt_load(RDX, 3);
        x64_mov_imm(R10, 0666); x64_mov_imm(RAX, 257); x64_syscall();
        rt_cmp(RAX, -4); x64_jcc(CC_Z, retry);
    } else {
        int exists = rt_label("file.open.raw.exists");
        rt_cmp(RDI, 0); x64_jcc(CC_Z, exists);
        rt_cmp(RDI, 1); x64_jcc(CC_Z, read);
        rt_cmp(RDI, 3); x64_jcc(CC_Z, append);
        x64_mov_imm(RDX, 0x40000000); x64_mov_imm(R10, 2); x64_jmp(call);
        x64_bind(append); x64_mov_imm(RDX, 4); x64_mov_imm(R10, 4); x64_jmp(call);
        x64_bind(read); x64_mov_imm(RDX, 0x80000000LL); x64_mov_imm(R10, 3); x64_jmp(call);
        x64_bind(exists); x64_mov_imm(RDX, 128); x64_mov_imm(R10, 3);
        x64_bind(call); rt_load(RCX, 1); x64_mov_imm(R8, 7); x64_xor(R9, R9);
        file_win_begin(); x64_store(RSP, 32, R10);
        x64_store_imm(RSP, 40, 0x02000080); x64_store_imm(RSP, 48, 0);
        file_win_end(IMP_CREATE_FILE_W);
        rt_cmp(RAX, -1); x64_jcc(CC_NZ, good); file_windows_error();
        x64_bind(good);
    }
    rt_leave();
}

static void gen_file_open_runtime(void)
{
    int mode_length = rt_label("file.open.mode.length");
    int r = rt_label("file.open.mode.r");
    int w = rt_label("file.open.mode.w");
    int a = rt_label("file.open.mode.a");
    int mode = rt_label("file.open.mode.ok");
    int opened = rt_label("file.open.success");
    int regular = rt_label("file.open.regular");
    rt_enter(file_labels[BI_FILE_OPEN - BI_FILE_OPEN], 6);
    rt_set(6, 0);
    x64_load(RAX, RBP, 24); rt_save(1, RAX); x64_store(R14, FILE_PATH, RAX);
    x64_load(RAX, RBP, 16); x64_load(RCX, RAX, 0); rt_cmp(RCX, 1); x64_jcc(CC_Z, mode_length);
    { int binary = rt_label("file.open.mode.binary");
      rt_cmp(RCX, 2); x64_jcc(CC_NZ, binary);
      x64_load8(RCX, RAX, 9); rt_cmp(RCX, 'b'); x64_jcc(CC_NZ, binary);
      rt_set(6, 4); x64_jmp(mode_length);
      x64_bind(binary); }
    file_fail(EX_SOFTWARE, "モードは r・w・a・rb・wb・ab のいずれかです");
    x64_bind(mode_length); x64_load8(RCX, RAX, 8);
    rt_cmp(RCX, 'r'); x64_jcc(CC_Z, r); rt_cmp(RCX, 'w'); x64_jcc(CC_Z, w); rt_cmp(RCX, 'a'); x64_jcc(CC_Z, a);
    file_fail(EX_SOFTWARE, "モードは r・w・a・rb・wb・ab のいずれかです");
    x64_bind(r); rt_set(2, 1); x64_jmp(mode);
    x64_bind(w); rt_set(2, 2); x64_jmp(mode);
    x64_bind(a); rt_set(2, 3);
    x64_bind(mode); rt_load(RAX, 2); rt_load(RCX, 6); x64_add(RAX, RCX); rt_save(2, RAX); rt_load(RAX, 1); file_io_call(file_path_label); rt_save(3, RAX);
    x64_mov_imm(RDI, 48); x64_call(alloc_label); rt_save(4, RAX); x64_store_imm(RAX, -8, 2);
    rt_load(RCX, 1); x64_store(RAX, 16, RCX);
    x64_mov_imm(RDI, FILE_BUFFER_SIZE); x64_call(alloc_label);
    rt_load(RCX, 4); x64_store(RCX, 40, RAX);
    rt_load(RAX, 3); rt_load(RDI, 2); x64_and_imm(RDI, 3); x64_call(file_raw_open_label);
    x64_test(RAX, RAX); x64_jcc(CC_NS, opened); file_os_fail("ファイルを開けません");
    x64_bind(opened); rt_save(5, RAX); x64_call(file_kind_label);
    rt_cmp(RAX, 1); x64_jcc(CC_Z, regular);
    x64_test(RAX, RAX);
    {
        int no_error = rt_label("file.open.kind.no.error");
        x64_jcc(CC_NS, no_error); x64_neg(RAX); x64_store(R14, FILE_OS_ERROR, RAX);
        x64_bind(no_error);
    }
    rt_load(RAX, 5); x64_call(file_raw_close_label);
    x64_lea_data(RAX,prog_add_str(prog,"通常ファイルではありません、またはファイル情報を取得できません",strlen("通常ファイルではありません、またはファイル情報を取得できません")));
    x64_mov_imm(RDI,5);x64_call(io_raise_label);rt_leave();
    x64_bind(regular); rt_load(RAX, 4); rt_load(RCX, 5); x64_store(RAX, 0, RCX);
    rt_load(RCX, 2); x64_store(RAX, 8, RCX);
    {int not_append=rt_label("file.open.not.append"),append_ok=rt_label("file.open.append.ok");
     x64_and_imm(RCX,3);rt_cmp(RCX,3);x64_jcc(CC_NZ,not_append);
     x64_load(RAX,RAX,0);x64_xor(RDI,RDI);x64_mov_imm(RSI,2);x64_call(file_raw_seek_label);
     x64_test(RAX,RAX);x64_jcc(CC_NS,append_ok);x64_neg(RAX);x64_store(R14,FILE_OS_ERROR,RAX);
     rt_load(RAX,4);x64_store_imm(RAX,8,0);x64_load(RAX,RAX,0);x64_call(file_raw_close_label);
     file_fail(EX_IOERR,"追記の初期位置を取得できません");x64_bind(append_ok);x64_bind(not_append);}
    rt_load(RAX,4);rt_leave();
}

/* rax=control, rdi=required mode(0 any,1 text read,2 text write,5 binary read,6 binary write). */
static void gen_file_check_runtime(void)
{
    int nonnull = rt_label("file.check.nonnull");
    int opened = rt_label("file.check.open");
    int good = rt_label("file.check.good");
    int writing = rt_label("file.check.writing");
    x64_bind(file_check_label);
    x64_test(RAX, RAX); x64_jcc(CC_NZ, nonnull); file_fail(EX_SOFTWARE, "file が未オープンです");
    x64_bind(nonnull); x64_load(RCX, RAX, 16); x64_store(R14, FILE_PATH, RCX);
    x64_load(RCX, RAX, 8); x64_test(RCX, RCX); x64_jcc(CC_NZ, opened);
    file_fail(EX_SOFTWARE, "file は閉じられています");
    x64_bind(opened); x64_test(RDI, RDI); x64_jcc(CC_Z, good);
    rt_cmp(RDI, 2); x64_jcc(CC_Z, writing); rt_cmp(RDI, 6); x64_jcc(CC_Z, writing);
    x64_cmp(RCX, RDI); x64_jcc(CC_Z, good); file_fail(EX_SOFTWARE, "この file は指定モードの読み取り用ではありません");
    x64_bind(writing); x64_cmp(RCX, RDI); x64_jcc(CC_Z, good);
    x64_mov(R8, RDI); x64_add_imm(R8, 1); x64_cmp(RCX, R8); x64_jcc(CC_Z, good);
    file_fail(EX_SOFTWARE, "この file は指定モードの書き込み用ではありません");
    x64_bind(good); x64_ret();
}

static void gen_file_close_runtime(void)
{
    int done = rt_label("file.close.done");
    int good = rt_label("file.close.success");
    rt_enter(file_labels[BI_FILE_CLOSE - BI_FILE_OPEN], 0);
    x64_load(RAX, RBP, 16); x64_test(RAX, RAX); x64_jcc(CC_Z, done);
    x64_load(RCX, RAX, 16); x64_store(R14, FILE_PATH, RCX);
    x64_load(RCX, RAX, 8); x64_test(RCX, RCX); x64_jcc(CC_Z, done);
    x64_store_imm(RAX, 8, 0); x64_load(RAX, RAX, 0); x64_call(file_raw_close_label);
    x64_test(RAX, RAX); x64_jcc(CC_NS, good); file_os_fail("close に失敗しました");
    x64_bind(good); x64_bind(done); x64_xor(RAX, RAX); rt_leave();
    {
        int end = rt_label("file.is.open.done");
        rt_enter(file_labels[BI_FILE_IS_OPEN - BI_FILE_OPEN], 0);
        x64_load(RAX, RBP, 16); x64_test(RAX, RAX); x64_jcc(CC_Z, end);
        x64_load(RAX, RAX, 8); x64_test(RAX, RAX); x64_setcc(CC_NZ, RAX); x64_movzx8(RAX, RAX);
        x64_bind(end); rt_leave();
    }
}

/* Mutable formatting/read accumulator [length, capacity, data]. */
static void gen_file_buffer_runtime(void)
{
    int room = rt_label("file.buffer.room");
    int capacity = rt_label("file.buffer.capacity");
    int capacity_ok = rt_label("file.buffer.capacity.ok");
    int length_ok = rt_label("file.buffer.length.ok");
    rt_enter(file_append_label, 6); rt_save(1, RAX); rt_save(2, RSI); rt_save(3, RDX);
    x64_load(RCX, RAX, 0); x64_add(RCX, RDX); x64_jcc(CC_NS, length_ok);
    file_fail(EX_SOFTWARE, "ファイルの文字列が大きすぎます");
    x64_bind(length_ok); rt_save(4, RCX); x64_load(RDX, RAX, 8);
    x64_cmp(RCX, RDX); x64_jcc(CC_BE, room);
    x64_mov(RDI, RDX); x64_add(RDI, RDI); x64_jcc(CC_NS, capacity_ok);
    x64_mov(RDI, RCX);
    x64_bind(capacity_ok); rt_cmp(RDI, 256); x64_jcc(CC_AE, capacity);
    x64_mov_imm(RDI, 256);
    x64_bind(capacity); x64_cmp(RDI, RCX);
    capacity = rt_label("file.buffer.enough"); x64_jcc(CC_AE, capacity); x64_mov(RDI, RCX);
    x64_bind(capacity); rt_save(5, RDI); x64_call(alloc_label); rt_save(6, RAX);
    rt_load(RDX, 1); x64_load(RSI, RDX, 16); x64_load(RCX, RDX, 0); x64_mov(RDI, RAX); x64_rep_movsb();
    rt_load(RAX, 1); rt_load(RCX, 6); x64_store(RAX, 16, RCX); rt_load(RCX, 5); x64_store(RAX, 8, RCX);
    x64_bind(room); rt_load(RAX, 1); x64_load(RDI, RAX, 16); x64_load(RCX, RAX, 0); x64_add(RDI, RCX);
    rt_load(RSI, 2); rt_load(RCX, 3); x64_rep_movsb();
    rt_load(RCX, 4); x64_store(RAX, 0, RCX); rt_leave();
    rt_enter(file_string_label, 2); rt_save(1, RAX);
    x64_load(RDI, RAX, 0); x64_add_imm(RDI, 8); x64_jcc(CC_NS, length_ok = rt_label("file.string.length.ok"));
    file_fail(EX_SOFTWARE, "ファイルの文字列が大きすぎます");
    x64_bind(length_ok); x64_call(alloc_label); rt_save(2, RAX);
    rt_load(RDX, 1); x64_load(RCX, RDX, 0); x64_store(RAX, 0, RCX);
    x64_load(RSI, RDX, 16); x64_lea(RDI, RAX, 8); x64_rep_movsb();
    rt_load(RAX, 2); x64_call(file_validate_label); rt_leave();
}

/* write_label redirect during scalar formatting; preserve Linux write's registers. */
static void gen_file_capture_runtime(void)
{
    Reg regs[] = {RCX, RDX, RSI, RDI, R8, R9, R10, R11};
    int i;
    rt_enter(file_capture_label, 8);
    for (i = 0; i < 8; i++) rt_save(i + 1, regs[i]);
    x64_load(RAX, R14, OUTPUT_SINK); x64_call(file_append_label);
    for (i = 0; i < 8; i++) rt_load(regs[i], i + 1);
    x64_mov(RAX, RDX); rt_leave();
}

/* rax=control -> byte or -1. EOF is not cached. */
static void gen_file_getc_runtime(void)
{
    int available = rt_label("file.getc.available");
    int retry = rt_label("file.getc.retry");
    int read_ok = rt_label("file.getc.read.ok");
    int eof = rt_label("file.getc.eof");
    int done = rt_label("file.getc.done");
    rt_enter(file_getc_label, 3); rt_save(1, RAX);
    x64_load(RCX, RAX, 24); x64_load(RDX, RAX, 32); x64_cmp(RCX, RDX); x64_jcc(CC_B, available);
    x64_bind(retry); rt_load(RAX, 1);
    if (target == TARGET_LINUX) {
        x64_load(RDI, RAX, 0); x64_load(RSI, RAX, 40); x64_mov_imm(RDX, FILE_BUFFER_SIZE);
        x64_xor(RAX, RAX); x64_syscall(); rt_cmp(RAX, -4); x64_jcc(CC_Z, retry);
    } else {
        x64_load(RCX, RAX, 0); x64_load(RDX, RAX, 40); x64_mov_imm(R8, FILE_BUFFER_SIZE);
        x64_lea(R9, RBP, -24); rt_set(3, 0);
        file_win_begin(); x64_store_imm(RSP, 32, 0); file_win_end(IMP_READ_FILE);
        x64_test(RAX, RAX); x64_jcc(CC_NZ, read_ok); file_windows_error(); x64_jmp(done);
        x64_bind(read_ok); rt_load(RAX, 3);
    }
    x64_bind(done); x64_test(RAX, RAX); x64_jcc(CC_NS, read_ok = rt_label("file.getc.not.error"));
    file_os_fail("read に失敗しました");
    x64_bind(read_ok); rt_load(RDX, 1); x64_store(RDX, 32, RAX); x64_store_imm(RDX, 24, 0);
    x64_test(RAX, RAX); x64_jcc(CC_Z, eof); x64_mov(RAX, RDX); x64_xor(RCX, RCX);
    x64_bind(available); x64_load(RDX, RAX, 40); x64_add(RDX, RCX); x64_load8(RDX, RDX, 0);
    x64_add_imm(RCX, 1); x64_store(RAX, 24, RCX); x64_mov(RAX, RDX); rt_leave();
    x64_bind(eof); x64_mov_imm(RAX, -1); rt_leave();
}

/* rax=control, rdi=line mode -> string in rax, line present in rdx. */
static void gen_file_read_text_runtime(void)
{
    int loop = rt_label("file.read.loop");
    int eof = rt_label("file.read.eof");
    int append = rt_label("file.read.append");
    int cr = rt_label("file.read.cr");
    int finish = rt_label("file.read.finish");
    int no_unread = rt_label("file.read.no.unread");
    rt_enter(file_text_label, 5); rt_save(1, RAX); rt_save(2, RDI);
    rt_set(5, 0); x64_mov_imm(RDI, 24); x64_call(alloc_label); rt_save(3, RAX);
    x64_bind(loop); rt_load(RAX, 1); file_io_call(file_getc_label);
    rt_cmp(RAX, -1); x64_jcc(CC_Z, eof); rt_set(5, 1);
    rt_load(RCX, 2); x64_test(RCX, RCX); x64_jcc(CC_Z, append);
    rt_cmp(RAX, 10); x64_jcc(CC_Z, finish); rt_cmp(RAX, 13); x64_jcc(CC_Z, cr);
    x64_bind(append); x64_store8(RBP, -32, RAX); rt_load(RAX, 3);
    x64_lea(RSI, RBP, -32); x64_mov_imm(RDX, 1); x64_call(file_append_label); x64_jmp(loop);
    x64_bind(cr); rt_load(RAX, 1); file_io_call(file_getc_label);
    rt_cmp(RAX, -1); x64_jcc(CC_Z, no_unread); rt_cmp(RAX, 10); x64_jcc(CC_Z, no_unread);
    rt_load(RAX, 1); x64_load(RCX, RAX, 24); x64_sub_imm(RCX, 1); x64_store(RAX, 24, RCX);
    x64_bind(no_unread); x64_jmp(finish);
    x64_bind(eof); x64_bind(finish); rt_load(RAX, 3); x64_call(file_string_label);
    rt_load(RDX, 5); rt_leave();
    rt_enter(file_labels[BI_FILE_READ - BI_FILE_OPEN], 0);
    x64_load(RAX, RBP, 16); x64_mov_imm(RDI, 1); x64_call(file_check_label);
    x64_xor(RDI, RDI); file_io_call(file_text_label); rt_leave();
    {
        int empty = rt_label("file.read_line.empty");
        rt_enter(file_labels[BI_FILE_READ_LINE - BI_FILE_OPEN], 0);
        x64_load(RAX, RBP, 24); x64_mov_imm(RDI, 1); x64_call(file_check_label);
        x64_mov_imm(RDI, 1); file_io_call(file_text_label);
        x64_test(RDX, RDX); x64_jcc(CC_Z, empty);
        x64_load(RCX, RBP, 16); x64_store(RCX, 0, RAX);
        x64_bind(empty); x64_mov(RAX, RDX); rt_leave();
    }
}

/* rax=control, rsi=raw bytes, rdx=length -> all bytes written.
 * Callers validate mode/content. Shared by text output and packed binary chunks. */
static void gen_file_write_runtime(void)
{
    int loop = rt_label("file.write.loop"), retry = rt_label("file.write.retry");
    int limited = rt_label("file.write.limited"), done = rt_label("file.write.done");
    int good = rt_label("file.write.good"), result = rt_label("file.write.result");
    rt_enter(file_write_all_label, 6);
    rt_save(1, RAX); rt_save(2, RSI); rt_save(3, RDX); rt_set(4, 0);
    x64_bind(loop); rt_load(RDX, 3); rt_load(RCX, 4);
    x64_cmp(RCX, RDX); x64_jcc(CC_AE, done); x64_sub(RDX, RCX);
    rt_cmp(RDX, 0x40000000); x64_jcc(CC_BE, limited); x64_mov_imm(RDX, 0x40000000);
    x64_bind(limited); rt_save(5, RDX);
    x64_bind(retry); rt_load(RSI, 2); rt_load(RCX, 4); x64_add(RSI, RCX); rt_load(RAX, 1);
    if (target == TARGET_LINUX) {
        x64_load(RDI, RAX, 0); rt_load(RDX, 5); x64_mov_imm(RAX, 1); x64_syscall();
        rt_cmp(RAX, -4); x64_jcc(CC_Z, retry);
    } else {
        x64_load(RCX, RAX, 0); x64_mov(RDX, RSI); rt_load(R8, 5); x64_lea(R9, RBP, -48); rt_set(6, 0);
        file_win_begin(); x64_store_imm(RSP, 32, 0); file_win_end(IMP_WRITE_FILE);
        x64_test(RAX, RAX); x64_jcc(CC_NZ, good); file_windows_error(); x64_jmp(result);
        x64_bind(good); rt_load(RAX, 6); x64_bind(result);
    }
    x64_test(RAX, RAX); x64_jcc(CC_NS, good = rt_label("file.write.not.error")); file_os_fail("write に失敗しました");
    x64_bind(good); x64_test(RAX, RAX); x64_jcc(CC_NZ, good = rt_label("file.write.progress"));
    file_fail(EX_IOERR, "write が進みません");
    x64_bind(good); rt_load(RCX, 4); x64_add(RCX, RAX); rt_save(4, RCX); x64_jmp(loop);
    x64_bind(done); rt_load(RAX, 4); rt_leave();

    /* Existing formatting code calls the text wrapper in registers. */
    rt_enter(file_labels[BI_FILE_WRITE - BI_FILE_OPEN], 2); rt_save(1, RAX); rt_save(2, RDI);
    x64_mov_imm(RDI, 2); x64_call(file_check_label);
    rt_load(RAX, 2); x64_call(file_validate_label);
    x64_load(RDX, RAX, 0); x64_lea(RSI, RAX, 8); rt_load(RAX, 1); file_io_call(file_write_all_label); rt_leave();
}

static void gen_file_exists_runtime(void)
{
    int missing = rt_label("file.exists.missing");
    int good = rt_label("file.exists.good");
    int done = rt_label("file.exists.done");
    int retry = rt_label("file.exists.retry");
    rt_enter(file_labels[BI_FILE_EXISTS - BI_FILE_OPEN], 22);
    x64_load(RAX, RBP, 16); x64_store(R14, FILE_PATH, RAX);
    x64_load(RCX, RAX, 0); x64_test(RCX, RCX); x64_jcc(CC_Z, missing);
    file_io_call(file_path_label); rt_save(1, RAX);
    if (target == TARGET_LINUX) {
        x64_bind(retry); x64_mov_imm(RDI, -100); rt_load(RSI, 1); x64_lea(RDX, RBP, -176);
        x64_xor(R10, R10); x64_mov_imm(RAX, 262); x64_syscall();
        rt_cmp(RAX, -4); x64_jcc(CC_Z, retry);
        rt_cmp(RAX, -2); x64_jcc(CC_Z, missing); rt_cmp(RAX, -20); x64_jcc(CC_Z, missing);
        x64_test(RAX, RAX); x64_jcc(CC_NS, good); file_os_fail("exists に失敗しました");
        x64_bind(good); x64_load(RCX, RBP, -152); x64_and_imm(RCX, 0xF000); rt_cmp(RCX, 0x8000);
        x64_setcc(CC_Z, RAX); x64_movzx8(RAX, RAX); x64_jmp(done);
    } else {
        x64_xor(RDI, RDI); x64_call(file_raw_open_label);
        rt_cmp(RAX, -2); x64_jcc(CC_Z, missing); rt_cmp(RAX, -3); x64_jcc(CC_Z, missing);
        rt_cmp(RAX, -267); x64_jcc(CC_Z, missing);
        x64_test(RAX, RAX); x64_jcc(CC_NS, good); file_os_fail("exists に失敗しました");
        x64_bind(good); rt_save(1, RAX); x64_call(file_kind_label); rt_save(2, RAX);
        rt_load(RAX, 1); x64_call(file_raw_close_label); rt_load(RAX, 2);
        { int known = rt_label("file.exists.kind.known"); x64_test(RAX, RAX); x64_jcc(CC_NS, known);
          file_os_fail("exists のファイル情報を取得できません"); x64_bind(known); }
        x64_jmp(done);
    }
    x64_bind(missing); x64_xor(RAX, RAX);
    x64_bind(done); rt_leave();
}

#include "binary_file_runtime.inc"

#include "file_position_runtime.inc"

#include "file_bulk_runtime.inc"

static void gen_file_runtime(void)
{
    gen_file_error_runtime(); gen_file_validate_runtime(); gen_file_path_runtime();
    gen_file_raw_close(); gen_file_finalizer(); gen_file_kind_runtime();
    gen_file_position_runtime();
    gen_file_raw_open(); gen_file_open_runtime(); gen_file_check_runtime();
    gen_file_close_runtime(); gen_file_buffer_runtime(); gen_file_capture_runtime();
    gen_file_getc_runtime(); gen_file_read_text_runtime(); gen_file_write_runtime(); gen_file_exists_runtime();
    gen_file_binary_runtime(); gen_recoverable_runtime(); gen_file_bulk_runtime();
}
