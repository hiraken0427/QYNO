/* Raw seek: rax=OS handle, rdi=offset, rsi=origin; result position or -OS error. */
static void gen_file_position_runtime(void)
{
    int ok=rt_label("file.raw.seek.ok"),done=rt_label("file.stat.ok");
    rt_enter(file_raw_seek_label,3);
    if(target==TARGET_LINUX){
        int retry=rt_label("file.raw.seek.retry");rt_save(1,RAX);rt_save(2,RDI);rt_save(3,RSI);
        x64_bind(retry);rt_load(RDI,1);rt_load(RSI,2);rt_load(RDX,3);x64_mov_imm(RAX,8);x64_syscall();rt_cmp(RAX,-4);x64_jcc(CC_Z,retry);
    }else{
        x64_mov(RCX,RAX);x64_mov(RDX,RDI);x64_mov(R9,RSI);x64_lea(R8,RBP,-8);
        file_win_begin();file_win_end(IMP_SET_FILE_POINTER_EX);x64_test(RAX,RAX);x64_jcc(CC_NZ,ok);
        file_windows_error();rt_leave();x64_bind(ok);rt_load(RAX,1);
    }
    rt_leave();
    /* stat: rax=control -> rax=size, rdx=mtime seconds/FILETIME, rcx=nsec. */
    rt_enter(file_stat_label,24);rt_save(1,RAX);x64_load(RAX,RAX,0);
    if(target==TARGET_LINUX){
        int retry=rt_label("file.stat.retry");x64_bind(retry);
        rt_load(RDI,1);x64_load(RDI,RDI,0);x64_lea(RSI,RBP,-176);x64_mov_imm(RAX,5);x64_syscall();
        rt_cmp(RAX,-4);x64_jcc(CC_Z,retry);x64_test(RAX,RAX);x64_jcc(CC_NS,done);
        file_os_fail("ファイル情報を取得できません");x64_bind(done);
        x64_load(RAX,RBP,-128);x64_load(RDX,RBP,-88);x64_load(RCX,RBP,-80);
    }else{
        x64_mov(RCX,RAX);x64_lea(RDX,RBP,-176);file_win_begin();file_win_end(IMP_GET_FILE_INFORMATION_BY_HANDLE);
        x64_test(RAX,RAX);x64_jcc(CC_NZ,done);file_windows_error();file_os_fail("ファイル情報を取得できません");x64_bind(done);
        x64_load(RAX,RBP,-144);x64_mov(RCX,RAX);x64_shl_imm(RAX,32);x64_shr_imm(RCX,32);x64_or(RAX,RCX);
        x64_load(RDX,RBP,-156);x64_xor(RCX,RCX);
    }
    {int fits=rt_label("file.size.fits");x64_test(RAX,RAX);x64_jcc(CC_NS,fits);
     x64_lea_data(RAX,prog_add_str(prog,"ファイルサイズがint_64の範囲を超えています",strlen("ファイルサイズがint_64の範囲を超えています")));
     x64_mov_imm(RDI,13);x64_call(io_raise_label);rt_leave();x64_bind(fits);}
    rt_leave();
    rt_enter(file_labels[BI_FILE_TELL-BI_FILE_OPEN],3);
    x64_load(RAX,RBP,16);x64_xor(RDI,RDI);x64_call(file_check_label);rt_save(1,RAX);
    x64_load(RAX,RAX,0);x64_xor(RDI,RDI);x64_mov_imm(RSI,1);x64_call(file_raw_seek_label);
    {int good=rt_label("file.tell.ok");x64_test(RAX,RAX);x64_jcc(CC_NS,good);file_os_fail("位置を取得できません");x64_bind(good);}
    rt_load(RCX,1);x64_load(RDX,RCX,32);x64_load(RCX,RCX,24);x64_sub(RDX,RCX);x64_sub(RAX,RDX);rt_leave();
    rt_enter(file_labels[BI_FILE_SIZE-BI_FILE_OPEN],0);
    x64_load(RAX,RBP,16);x64_xor(RDI,RDI);x64_call(file_check_label);file_io_call(file_stat_label);rt_leave();
    rt_enter(file_labels[BI_FILE_MODIFIED_AT-BI_FILE_OPEN],0);
    x64_load(RAX,RBP,16);x64_xor(RDI,RDI);x64_call(file_check_label);file_io_call(file_stat_label);
    x64_mov(RAX,RDX);x64_mov(RDI,RCX);x64_call(target==TARGET_LINUX?time_timespec_label:time_filetime_label);rt_leave();

    rt_enter(file_labels[BI_FILE_SEEK-BI_FILE_OPEN],6);
    x64_load(RAX,RBP,32);x64_xor(RDI,RDI);x64_call(file_check_label);rt_save(1,RAX);
    x64_load(RCX,RAX,8);x64_and_imm(RCX,4);
    {int binary=rt_label("file.seek.binary");x64_test(RCX,RCX);x64_jcc(CC_NZ,binary);file_fail(EX_SOFTWARE,"seekはバイナリモード専用です");x64_bind(binary);}
    x64_push(RAX);x64_call(file_labels[BI_FILE_SIZE-BI_FILE_OPEN]);x64_add_imm(RSP,8);io_rt_check();rt_save(2,RAX);
    {int current=rt_label("file.seek.current"),end=rt_label("file.seek.end"),base=rt_label("file.seek.base"),bad=rt_label("file.seek.bad"),moved=rt_label("file.seek.moved");
     x64_load(RCX,RBP,16);rt_cmp(RCX,0);x64_jcc(CC_Z,base);rt_cmp(RCX,1);x64_jcc(CC_Z,current);rt_cmp(RCX,2);x64_jcc(CC_Z,end);x64_jmp(bad);
     x64_bind(current);rt_load(RAX,1);x64_push(RAX);x64_call(file_labels[BI_FILE_TELL-BI_FILE_OPEN]);x64_add_imm(RSP,8);io_rt_check();x64_jmp(moved);
     x64_bind(end);rt_load(RAX,2);x64_jmp(moved);x64_bind(base);x64_xor(RAX,RAX);x64_bind(moved);
     x64_load(RCX,RBP,24);x64_add(RAX,RCX);x64_jcc(CC_O,bad);x64_test(RAX,RAX);x64_jcc(CC_S,bad);
     rt_load(RCX,2);x64_cmp(RAX,RCX);x64_jcc(CC_A,bad);x64_mov(RDI,RAX);rt_load(RAX,1);x64_load(RAX,RAX,0);x64_xor(RSI,RSI);x64_call(file_raw_seek_label);
     {int good=rt_label("file.seek.ok");x64_test(RAX,RAX);x64_jcc(CC_NS,good);file_os_fail("seekに失敗しました");x64_bind(good);}
     rt_load(RCX,1);x64_store_imm(RCX,24,0);x64_store_imm(RCX,32,0);rt_leave();
     x64_bind(bad);file_fail(EX_SOFTWARE,"seekの位置が先頭〜EOFまたはint_64の範囲外です");}
}
