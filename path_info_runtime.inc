/* Path metadata is captured before owned resources close and a value is built. */
static int path_info_open_label;
static void gen_path_info_open(void)
{
    int flags=rt_label("path.info.open.flags"),good=rt_label("path.info.open.good");
    if(target!=TARGET_WINDOWS)return;
    rt_enter(path_info_open_label,0);x64_mov(RCX,RAX);x64_mov_imm(R10,0x02000000);
    x64_test(RDI,RDI);x64_jcc(CC_NZ,flags);x64_mov_imm(R10,0x02200000);x64_bind(flags);
    x64_mov_imm(RDX,0x80);x64_mov_imm(R8,7);x64_xor(R9,R9);file_win_begin();
    x64_store_imm(RSP,32,3);x64_store(RSP,40,R10);x64_store_imm(RSP,48,0);file_win_end(IMP_CREATE_FILE_W);
    rt_cmp(RAX,-1);x64_jcc(CC_NZ,good);file_windows_error();x64_bind(good);rt_leave();
}
static void path_info_handle_call(Import api)
{
    rt_load(RCX,3);x64_load(RCX,RCX,0);x64_lea(RDX,RBP,-256);file_win_begin();file_win_end(api);
}
static void gen_path_info_runtime(void)
{
    int ready=rt_label("path.info.ready"),finish=rt_label("path.info.close");
    int classified=rt_label("path.info.classified"),regular=rt_label("path.info.regular"),directory=rt_label("path.info.directory"),link=rt_label("path.info.link");
    int large=rt_label("path.info.large"),malformed=rt_label("path.info.malformed");
    path_info_open_label=rt_label("path.info.open");gen_path_info_open();
    /* Slots 1..10 contain roots and extracted fields. The 144-byte stat record
     * at -256..-113 is separate from these slots and the Windows tag record. */
    rt_enter(file_labels[BI_PATH_INFO-BI_FILE_OPEN],32);rt_set(3,0);rt_set(4,0);rt_set(5,0);
    x64_load(RAX,RBP,24);x64_store(R14,FILE_PATH,RAX);
    x64_load(RCX,R14,FILE_CONTEXT);x64_store(R14,COLLECTION_CONTEXT,RCX);
    x64_load(RDI,RBP,16);rt_save(2,RDI);x64_xor(RDI,RDI);file_io_call(dir_path_label);rt_save(1,RAX);
    if(target==TARGET_LINUX){
        int retry=rt_label("path.info.stat.retry");x64_bind(retry);x64_mov_imm(RDI,-100);rt_load(RSI,1);x64_lea(RDX,RBP,-256);x64_mov_imm(R10,256);
        rt_load(RAX,2);x64_test(RAX,RAX);x64_jcc(CC_Z,ready);x64_xor(R10,R10);x64_bind(ready);
        x64_mov_imm(RAX,262);x64_syscall();rt_cmp(RAX,-4);x64_jcc(CC_Z,retry);
        x64_test(RAX,RAX);x64_jcc(CC_NS,ready=rt_label("path.info.stat.ready"));dir_os_error(0);rt_leave();x64_bind(ready);
        x64_load(RAX,RBP,-168);rt_save(6,RAX);x64_load(RAX,RBP,-160);rt_save(7,RAX);
        x64_test(RAX,RAX);x64_jcc(CC_S,malformed);rt_cmp(RAX,1000000000);x64_jcc(CC_AE,malformed);
        x64_load(RAX,RBP,-232);x64_and_imm(RAX,0xf000);
        rt_cmp(RAX,0x8000);x64_jcc(CC_Z,regular);rt_cmp(RAX,0x4000);x64_jcc(CC_Z,directory);rt_cmp(RAX,0xa000);x64_jcc(CC_Z,link);x64_jmp(classified);
    }else{
        int normal=rt_label("path.info.normal"),tagged=rt_label("path.info.tagged"),typed=rt_label("path.info.typed");
        dir_control(3);rt_load(RAX,1);rt_load(RDI,2);x64_call(path_info_open_label);
        x64_test(RAX,RAX);x64_jcc(CC_NS,ready);dir_os_error(0);rt_leave();x64_bind(ready);dir_adopt(3);
        path_info_handle_call(IMP_GET_FILE_INFORMATION_BY_HANDLE);x64_test(RAX,RAX);x64_jcc(CC_NZ,ready=rt_label("path.info.metadata.ready"));
        file_windows_error();dir_os_error(0);x64_jmp(finish);x64_bind(ready);
        x64_load(RAX,RBP,-236);rt_save(6,RAX);rt_set(7,0);
        x64_load(RAX,RBP,-256);x64_and_imm(RAX,0x410);x64_mov(RCX,RAX);x64_and_imm(RCX,0x400);x64_test(RCX,RCX);x64_jcc(CC_Z,normal);
        rt_load(RCX,3);x64_load(RCX,RCX,0);x64_mov_imm(RDX,9);x64_lea(R8,RBP,-192);x64_mov_imm(R9,8);
        file_win_begin();file_win_end(IMP_GET_FILE_INFORMATION_BY_HANDLE_EX);x64_test(RAX,RAX);x64_jcc(CC_NZ,tagged);
        file_windows_error();dir_os_error(0);x64_jmp(finish);x64_bind(tagged);
        x64_load(RAX,RBP,-192);x64_mov(RCX,RAX);x64_and_imm(RCX,0x400);x64_test(RCX,RCX);x64_jcc(CC_Z,normal);
        x64_shr_imm(RAX,32);rt_cmp(RAX,0xa000000cLL);x64_jcc(CC_Z,link);rt_cmp(RAX,0xa0000003LL);x64_jcc(CC_Z,link);x64_jmp(classified);
        x64_bind(normal);x64_and_imm(RAX,16);x64_test(RAX,RAX);x64_jcc(CC_NZ,directory);
        x64_xor(RCX,RCX);file_win_begin();file_win_end(IMP_SET_LAST_ERROR);
        rt_load(RCX,3);x64_load(RCX,RCX,0);file_win_begin();file_win_end(IMP_GET_FILE_TYPE);
        x64_test(RAX,RAX);x64_jcc(CC_NZ,typed);file_windows_error();x64_test(RAX,RAX);x64_jcc(CC_Z,classified);dir_os_error(0);x64_jmp(finish);
        x64_bind(typed);rt_cmp(RAX,1);x64_jcc(CC_Z,regular);x64_jmp(classified);
    }
    x64_bind(regular);rt_set(4,1);
    if(target==TARGET_LINUX)x64_load(RAX,RBP,-208);
    else{x64_load(RAX,RBP,-224);x64_mov(RCX,RAX);x64_shl_imm(RAX,32);x64_shr_imm(RCX,32);x64_or(RAX,RCX);}
    x64_test(RAX,RAX);x64_jcc(CC_S,large);rt_save(5,RAX);x64_jmp(classified);
    x64_bind(directory);rt_set(4,2);x64_jmp(classified);
    x64_bind(link);rt_set(4,3);
    x64_bind(classified);x64_jmp(finish);
    x64_bind(large);dir_policy_error(13,"Metadata size exceeds int_64");x64_jmp(finish);
    x64_bind(malformed);dir_policy_error(14,"Malformed metadata timestamp");
    x64_bind(finish);if(target==TARGET_WINDOWS)dir_close(3);io_rt_check();
    rt_load(RAX,6);rt_load(RDI,7);x64_call(target==TARGET_LINUX?time_timespec_label:time_filetime_label);rt_save(8,RAX);
    x64_mov_imm(RDI,40);x64_call(alloc_label);rt_save(9,RAX);x64_store_imm(RAX,0,4);x64_store_imm(RAX,8,4);
    x64_mov_imm(RDI,32);x64_call(alloc_label);rt_save(10,RAX);rt_load(RCX,9);x64_store(RCX,24,RAX);
    rt_load(RCX,4);x64_store(RAX,0,RCX);rt_cmp(RCX,1);x64_setcc(CC_Z,RDX);x64_movzx8(RDX,RDX);x64_store(RAX,8,RDX);
    rt_load(RCX,5);x64_store(RAX,16,RCX);rt_load(RCX,8);x64_store(RAX,24,RCX);rt_load(RAX,9);rt_leave();
}
