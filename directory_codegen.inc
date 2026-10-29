/* OS directory operations share the public io.Error context with file I/O. */
static void gen_directory_builtin(Node *node)
{
    Function fn={0};Node call=*node;
    fn.label=directory_labels[node->val-BI_DIR_CURRENT];call.func=&fn;gen_call(&call);
}
static void directory_path_call(int operation)
{
    x64_xor(RDI,RDI);x64_mov_imm(R8,operation);x64_call(path_call_label);
}
static void directory_os_failure(void)
{
    int ordinary=rt_label("directory.os.error");
    rt_cmp(RAX,target==TARGET_LINUX?-20:-267);x64_jcc(CC_NZ,ordinary);
    x64_neg(RAX);x64_store(R14,FILE_OS_ERROR,RAX);x64_mov_imm(RDI,15);
    x64_lea_data(RAX,prog_add_str(prog,"対象がディレクトリではありません",strlen("対象がディレクトリではありません")));
    x64_call(io_raise_label);rt_leave();
    x64_bind(ordinary);file_os_fail("作業ディレクトリ操作に失敗しました");
}
static void directory_windows_conversion_failure(void)
{
    int os=rt_label("directory.encoding.os.error");
    file_windows_error();rt_cmp(RAX,-1113);x64_jcc(CC_NZ,os);
    file_fail(EX_DATAERR,"作業ディレクトリが正しいUnicodeではありません");
    x64_bind(os);directory_os_failure();
}
static void directory_windows_encode(int output)
{
    x64_mov_imm(RCX,65001);x64_mov_imm(RDX,0x80);
    rt_load(R8,1);rt_load(R9,3);
    if(output){rt_load(RAX,4);x64_add_imm(RAX,8);rt_load(R10,5);}
    else{x64_xor(RAX,RAX);x64_xor(R10,R10);}
    file_win_begin();x64_store(RSP,32,RAX);x64_store(RSP,40,R10);
    x64_store_imm(RSP,48,0);x64_store_imm(RSP,56,0);file_win_end(IMP_WIDE_CHAR_TO_MULTI_BYTE);
}
static void directory_current_result(void)
{
    int absolute=rt_label("directory.current.absolute");
    directory_path_call(7);rt_save(4,RAX);directory_path_call(5);
    x64_test(RAX,RAX);x64_jcc(CC_NZ,absolute);
    x64_lea_data(RAX,prog_add_str(prog,"作業ディレクトリが現在のルートから到達できません",strlen("作業ディレクトリが現在のルートから到達できません")));
    x64_mov_imm(RDI,14);x64_call(io_raise_label);rt_leave();
    x64_bind(absolute);rt_load(RAX,4);rt_leave();
}
static void gen_directory_runtime(void)
{
    int retry=rt_label("directory.current.retry"),read=rt_label("directory.current.read"),good=rt_label("directory.current.good");
    int grow=rt_label("directory.current.grow"),room=rt_label("directory.current.room");
    rt_enter(directory_labels[0],6);rt_set(2,128);
    x64_bind(retry);rt_load(RDI,2);
    if(target==TARGET_WINDOWS)x64_shl_imm(RDI,1);else x64_add_imm(RDI,8);
    x64_call(alloc_label);rt_save(1,RAX);x64_bind(read);
    if(target==TARGET_LINUX){
        rt_load(RDI,1);x64_add_imm(RDI,8);rt_load(RSI,2);x64_mov_imm(RAX,79);x64_syscall();
        rt_cmp(RAX,-4);x64_jcc(CC_Z,read);rt_cmp(RAX,-34);x64_jcc(CC_Z,grow);
        x64_test(RAX,RAX);x64_jcc(CC_NS,good);directory_os_failure();
        x64_bind(good);x64_sub_imm(RAX,1);rt_load(RCX,1);x64_store(RCX,0,RAX);x64_mov(RAX,RCX);
        x64_call(file_validate_label);directory_current_result();
        x64_bind(grow);rt_load(RAX,2);x64_mov_imm(RCX,(INT64_MAX-8)/2);x64_cmp(RAX,RCX);x64_jcc(CC_BE,room);
        file_fail(EX_SOFTWARE,"作業ディレクトリのパスが長すぎます");
        x64_bind(room);x64_shl_imm(RAX,1);rt_save(2,RAX);x64_jmp(retry);
    }else{
        rt_load(RCX,2);rt_load(RDX,1);file_win_begin();file_win_end(IMP_GET_CURRENT_DIRECTORY_W);
        x64_test(RAX,RAX);x64_jcc(CC_NZ,good);file_windows_error();directory_os_failure();
        x64_bind(good);rt_load(RCX,2);x64_cmp(RAX,RCX);x64_jcc(CC_AE,grow);
        rt_save(3,RAX);directory_windows_encode(0);
        x64_test(RAX,RAX);x64_jcc(CC_NZ,room);directory_windows_conversion_failure();
        x64_bind(room);rt_save(5,RAX);x64_mov(RDI,RAX);x64_add_imm(RDI,8);x64_call(alloc_label);rt_save(4,RAX);
        rt_load(RCX,5);x64_store(RAX,0,RCX);directory_windows_encode(1);
        {int converted=rt_label("directory.current.converted");x64_test(RAX,RAX);x64_jcc(CC_NZ,converted);directory_windows_conversion_failure();x64_bind(converted);}
        rt_load(RAX,4);directory_current_result();
        x64_bind(grow);x64_mov_imm(RCX,INT32_MAX);x64_cmp(RAX,RCX);
        {int fits=rt_label("directory.current.fits");x64_jcc(CC_BE,fits);file_fail(EX_SOFTWARE,"作業ディレクトリのパスが長すぎます");x64_bind(fits);}
        rt_save(2,RAX);x64_jmp(retry);
    }

    rt_enter(directory_labels[1],2);x64_load(RAX,RBP,16);x64_store(R14,FILE_PATH,RAX);
    directory_path_call(9);rt_save(1,RAX);
    {int nonempty=rt_label("directory.change.nonempty");x64_load(RCX,RAX,0);x64_test(RCX,RCX);x64_jcc(CC_NZ,nonempty);file_fail(EX_SOFTWARE,"移動先のパスを空にできません");x64_bind(nonempty);}
    file_io_call(file_path_label);rt_save(2,RAX);
    if(target==TARGET_LINUX){
        int attempt=rt_label("directory.change.attempt"),done=rt_label("directory.change.done");
        x64_bind(attempt);rt_load(RDI,2);x64_mov_imm(RAX,80);x64_syscall();rt_cmp(RAX,-4);x64_jcc(CC_Z,attempt);
        x64_test(RAX,RAX);x64_jcc(CC_NS,done);directory_os_failure();x64_bind(done);
    }else{
        int done=rt_label("directory.change.done");
        x64_mov(RCX,RAX);file_win_begin();file_win_end(IMP_SET_CURRENT_DIRECTORY_W);
        x64_test(RAX,RAX);x64_jcc(CC_NZ,done);file_windows_error();directory_os_failure();x64_bind(done);
    }
    x64_xor(RAX,RAX);rt_leave();

    rt_enter(directory_labels[2],2);x64_load(RAX,RBP,16);x64_store(R14,FILE_PATH,RAX);
    directory_path_call(9);rt_save(1,RAX);directory_path_call(5);
    {int absolute=rt_label("directory.absolute.ready");x64_test(RAX,RAX);x64_jcc(CC_NZ,absolute);
     file_io_call(directory_labels[0]);rt_load(RDI,1);x64_mov_imm(R8,0);x64_call(path_call_label);rt_save(1,RAX);
     x64_bind(absolute);rt_load(RAX,1);directory_path_call(7);rt_leave();}
}
