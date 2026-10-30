/* Regular-file operations: exclusive creation, no-follow source handles,
 * bounded byte transfers, and OS-enforced no-replace rename. */
static int mutation_path_label,mutation_error_label,mutation_open_label;
static int mutation_kind_label,mutation_destination_label;
static void mutation_error(void){x64_call(mutation_error_label);}
static void mutation_close(int slot)
{rt_load(RAX,slot);x64_test(RAX,RAX);{int done=rt_label("mutation.close.empty");x64_jcc(CC_Z,done);x64_call(file_bulk_close_label);x64_bind(done);}}
static void gen_mutation_error(void)
{
    int usual=rt_label("mutation.error.usual");
    rt_enter(mutation_error_label,0);rt_cmp(RAX,target==TARGET_LINUX?-18:-17);x64_jcc(CC_NZ,usual);
    x64_neg(RAX);x64_store(R14,FILE_OS_ERROR,RAX);x64_mov_imm(RDI,12);
    x64_lea_data(RAX,prog_add_str(prog,"Moving across filesystems is unsupported",strlen("Moving across filesystems is unsupported")));x64_call(io_raise_label);rt_leave();
    x64_bind(usual);file_os_fail("File operation failed");
}
/* rax=original -> native path; source/destination strings stay in caller slots. */
static void gen_mutation_path(void)
{
    int bad=rt_label("mutation.path.bad"),ready=rt_label("mutation.path.ready");
    rt_enter(mutation_path_label,1);x64_store(R14,FILE_PATH,RAX);x64_load(RCX,R14,FILE_CONTEXT);x64_store(R14,COLLECTION_CONTEXT,RCX);directory_path_call(9);rt_save(1,RAX);
    x64_load(RCX,RAX,0);x64_test(RCX,RCX);x64_jcc(CC_Z,bad);
    x64_lea(RDX,RAX,7);x64_add(RDX,RCX);x64_load8(RDI,RDX,0);rt_cmp(RDI,'/');x64_jcc(CC_Z,bad);
    directory_path_call(1);x64_load(RCX,RAX,0);x64_test(RCX,RCX);x64_jcc(CC_Z,bad);
    rt_cmp(RCX,2);x64_jcc(CC_A,ready);x64_load8(RDX,RAX,8);rt_cmp(RDX,'.');x64_jcc(CC_NZ,ready);
    rt_cmp(RCX,1);x64_jcc(CC_Z,bad);x64_load8(RDX,RAX,9);rt_cmp(RDX,'.');x64_jcc(CC_NZ,ready);
    x64_bind(bad);file_fail(EX_SOFTWARE,"Invalid file path: empty, root, dot, or trailing separator");
    x64_bind(ready);rt_load(RAX,1);file_io_call(file_path_label);rt_leave();
}
/* rax=native pathname, rdi=0 copy source / 1 destination / 2 delete/rename.
 * Returns an OS handle or a negative native error. */
static void gen_mutation_open(void)
{
    int dest=rt_label("mutation.open.dest"),call=rt_label("mutation.open.call"),good=rt_label("mutation.open.good");
    rt_enter(mutation_open_label,3);rt_save(1,RAX);rt_save(2,RDI);
    if(target==TARGET_LINUX){
        int retry=rt_label("mutation.open.retry");
        x64_mov_imm(RDX,0xa0800);rt_cmp(RDI,1);x64_jcc(CC_NZ,call);
        x64_bind(dest);x64_mov_imm(RDX,0xa00c1);x64_bind(call);rt_save(3,RDX);
        x64_bind(retry);x64_mov_imm(RDI,-100);rt_load(RSI,1);rt_load(RDX,3);x64_mov_imm(R10,0666);x64_mov_imm(RAX,257);x64_syscall();
        rt_cmp(RAX,-4);x64_jcc(CC_Z,retry);
    }else{
        int remove=rt_label("mutation.open.remove");
        x64_mov_imm(RDX,0x80000080LL);x64_mov_imm(R10,3);x64_mov_imm(RAX,0x02200000);
        rt_cmp(RDI,1);x64_jcc(CC_Z,dest);rt_cmp(RDI,2);x64_jcc(CC_Z,remove);x64_jmp(call);
        x64_bind(dest);x64_mov_imm(RDX,0x40000080);x64_mov_imm(R10,1);x64_mov_imm(RAX,0x00200080);x64_jmp(call);
        x64_bind(remove);x64_mov_imm(RDX,0x10080);x64_bind(call);
        rt_load(RCX,1);x64_mov_imm(R8,7);x64_xor(R9,R9);file_win_begin();
        x64_store(RSP,32,R10);x64_store(RSP,40,RAX);x64_store_imm(RSP,48,0);file_win_end(IMP_CREATE_FILE_W);
        rt_cmp(RAX,-1);x64_jcc(CC_NZ,good);file_windows_error();x64_bind(good);
    }
    rt_leave();
}
/* rax=opened Windows handle, or Linux native source path -> validated kind.
 * Windows handles are opened without following final reparse points. */
static void gen_mutation_kind(void)
{
    int known=rt_label("mutation.kind.known"),link=rt_label("mutation.kind.link"),wrong=rt_label("mutation.kind.wrong");
    int done=rt_label("mutation.kind.done");
    rt_enter(mutation_kind_label,1);rt_save(1,RAX);
    if(target==TARGET_LINUX)x64_mov_imm(RDI,256);
    x64_call(dir_meta_label);x64_test(RAX,RAX);x64_jcc(CC_NS,known);mutation_error();rt_leave();
    x64_bind(known);
    if(target==TARGET_LINUX){rt_cmp(RAX,0xa000);x64_jcc(CC_Z,link);rt_cmp(RAX,0x8000);x64_jcc(CC_NZ,wrong);}
    else{
        x64_mov(RCX,RAX);x64_and_imm(RCX,0x400);x64_test(RCX,RCX);x64_jcc(CC_NZ,link);
        x64_and_imm(RAX,16);x64_test(RAX,RAX);x64_jcc(CC_NZ,wrong);
        rt_load(RCX,1);file_win_begin();file_win_end(IMP_GET_FILE_TYPE);
        {int typed=rt_label("mutation.kind.typed");x64_test(RAX,RAX);x64_jcc(CC_NZ,typed);file_windows_error();mutation_error();rt_leave();x64_bind(typed);}
        rt_cmp(RAX,1);x64_jcc(CC_NZ,wrong);
    }
    x64_jmp(done);x64_bind(link);dir_policy_error(12,"File links are unsupported");rt_leave();
    x64_bind(wrong);dir_policy_error(5,"Target is not a regular file");x64_bind(done);rt_leave();
}
/* rax=native Windows destination -> fail if occupied, no handle acquired.
 * Exclusive creation and no-replace rename still decide concurrent races. */
static void gen_mutation_destination(void)
{
    int absent=rt_label("mutation.destination.absent"),failed=rt_label("mutation.destination.failed");
    if(target!=TARGET_WINDOWS)return;
    rt_enter(mutation_destination_label,0);x64_mov(RCX,RAX);file_win_begin();file_win_end(IMP_GET_FILE_ATTRIBUTES_W);
    rt_cmp(RAX,0xffffffffLL);x64_jcc(CC_Z,failed);dir_policy_error(3,"Destination already exists");rt_leave();
    x64_bind(failed);file_windows_error();rt_cmp(RAX,-2);x64_jcc(CC_Z,absent);rt_cmp(RAX,-3);x64_jcc(CC_Z,absent);
    mutation_error();x64_bind(absent);rt_leave();
}
static void gen_mutation_remove(void)
{
    int finish=rt_label("mutation.remove.close"),good=rt_label("mutation.remove.good");
    rt_enter(file_labels[BI_FILE_REMOVE-BI_FILE_OPEN],4);x64_load(RAX,RBP,16);rt_save(1,RAX);
    file_io_call(mutation_path_label);rt_save(2,RAX);
    if(target==TARGET_WINDOWS){
        dir_control(3);rt_load(RAX,2);x64_mov_imm(RDI,2);x64_call(mutation_open_label);
        x64_test(RAX,RAX);x64_jcc(CC_NS,good);mutation_error();rt_leave();x64_bind(good);dir_adopt(3);
    }
    x64_call(mutation_kind_label);bulk_pending(finish);
    if(target==TARGET_LINUX){
        x64_mov_imm(RDI,-100);rt_load(RSI,2);x64_xor(RDX,RDX);x64_mov_imm(RAX,263);x64_syscall();
        x64_test(RAX,RAX);x64_jcc(CC_NS,finish);
    }else{
        rt_load(RCX,3);x64_load(RCX,RCX,0);x64_mov_imm(RDX,4);rt_set(4,1);x64_lea(R8,RBP,-32);x64_mov_imm(R9,1);
        file_win_begin();file_win_end(IMP_SET_FILE_INFORMATION_BY_HANDLE);x64_test(RAX,RAX);x64_jcc(CC_NZ,finish);file_windows_error();
    }
    mutation_error();x64_bind(finish);if(target==TARGET_WINDOWS)mutation_close(3);x64_xor(RAX,RAX);rt_leave();
}
static void mutation_paths(void)
{
    x64_load(RAX,RBP,24);rt_save(1,RAX);file_io_call(mutation_path_label);rt_save(3,RAX);
    x64_load(RAX,RBP,16);rt_save(2,RAX);file_io_call(mutation_path_label);rt_save(4,RAX);
}
static void mutation_original_path(int slot)
{rt_load(RAX,slot);x64_store(R14,FILE_PATH,RAX);}
static void gen_mutation_copy(void)
{
    int opened=rt_label("mutation.copy.opened"),finish=rt_label("mutation.copy.close"),loop=rt_label("mutation.copy.loop");
    int regular=rt_label("mutation.copy.regular"),created=rt_label("mutation.copy.created");
    /* original source/dest, native source/dest, owned source/dest. */
    rt_enter(file_labels[BI_FILE_COPY-BI_FILE_OPEN],6);rt_set(5,0);rt_set(6,0);mutation_paths();
    mutation_original_path(1);dir_control(5);
    x64_mov_imm(RDI,FILE_BUFFER_SIZE);x64_call(alloc_label);rt_load(RCX,5);x64_store(RCX,40,RAX);
    if(target==TARGET_LINUX){rt_load(RAX,3);file_io_call(mutation_kind_label);}
    rt_load(RAX,3);x64_xor(RDI,RDI);x64_call(mutation_open_label);
    x64_test(RAX,RAX);x64_jcc(CC_NS,opened);mutation_error();rt_leave();
    x64_bind(opened);dir_adopt(5);rt_load(RAX,5);x64_store_imm(RAX,8,5);x64_load(RAX,RAX,0);
    if(target==TARGET_LINUX){
        x64_call(file_kind_label);x64_test(RAX,RAX);x64_jcc(CC_NS,regular);mutation_error();x64_jmp(finish);
        x64_bind(regular);x64_test(RAX,RAX);x64_jcc(CC_NZ,regular=rt_label("mutation.copy.valid"));dir_policy_error(5,"Source is not a regular file");x64_jmp(finish);x64_bind(regular);
    }else{x64_call(mutation_kind_label);bulk_pending(finish);}
    mutation_original_path(2);
    if(target==TARGET_WINDOWS){rt_load(RAX,4);x64_call(mutation_destination_label);bulk_pending(finish);}
    dir_control(6);rt_load(RAX,4);x64_mov_imm(RDI,1);x64_call(mutation_open_label);
    x64_test(RAX,RAX);x64_jcc(CC_NS,created);mutation_error();x64_jmp(finish);
    x64_bind(created);dir_adopt(6);rt_load(RAX,6);x64_store_imm(RAX,8,6);
    x64_bind(loop);mutation_original_path(1);rt_load(RAX,5);x64_call(file_getc_label);bulk_pending(finish);
    rt_cmp(RAX,-1);x64_jcc(CC_Z,finish);
    rt_load(RAX,5);x64_load(RCX,RAX,24);x64_sub_imm(RCX,1);x64_load(RDX,RAX,32);x64_sub(RDX,RCX);
    x64_load(RSI,RAX,40);x64_add(RSI,RCX);mutation_original_path(2);rt_load(RAX,6);x64_call(file_write_all_label);bulk_pending(finish);
    rt_load(RAX,5);x64_load(RCX,RAX,32);x64_store(RAX,24,RCX);x64_jmp(loop);
    x64_bind(finish);mutation_close(6);mutation_close(5);x64_xor(RAX,RAX);rt_leave();
}
static void gen_mutation_move(void)
{
    int opened=rt_label("mutation.move.opened"),finish=rt_label("mutation.move.close"),done=rt_label("mutation.move.done");
    rt_enter(file_labels[BI_FILE_MOVE-BI_FILE_OPEN],8);rt_set(5,0);mutation_paths();mutation_original_path(1);
    if(target==TARGET_LINUX){
        rt_load(RAX,3);file_io_call(mutation_kind_label);mutation_original_path(2);
        x64_mov_imm(RDI,-100);rt_load(RSI,3);x64_mov_imm(RDX,-100);rt_load(R10,4);x64_mov_imm(R8,1);x64_mov_imm(RAX,316);x64_syscall();
        x64_test(RAX,RAX);x64_jcc(CC_NS,done);mutation_error();x64_bind(done);x64_xor(RAX,RAX);rt_leave();return;
    }
    dir_control(5);rt_load(RAX,3);x64_mov_imm(RDI,2);x64_call(mutation_open_label);
    x64_test(RAX,RAX);x64_jcc(CC_NS,opened);mutation_error();rt_leave();x64_bind(opened);dir_adopt(5);
    x64_call(mutation_kind_label);bulk_pending(finish);
    mutation_original_path(2);rt_load(RAX,4);x64_call(mutation_destination_label);bulk_pending(finish);
    /* FILE_RENAME_INFO: ReplaceIfExists=FALSE, RootDirectory=NULL,
     * FileNameLength at 16 and the absolute UTF-16 FileName at 20. */
    rt_load(RAX,2);x64_push(RAX);x64_call(directory_labels[2]);x64_add_imm(RSP,8);bulk_pending(finish);
    x64_call(file_path_label);bulk_pending(finish);rt_save(4,RAX);rt_set(6,0);
    {int scan=rt_label("mutation.rename.length"),measured=rt_label("mutation.rename.measured");
     x64_mov(RSI,RAX);x64_xor(RCX,RCX);x64_bind(scan);x64_load8(RDX,RSI,0);x64_load8(RDI,RSI,1);x64_or(RDX,RDI);x64_test(RDX,RDX);x64_jcc(CC_Z,measured);
     x64_add_imm(RSI,2);x64_add_imm(RCX,2);x64_jmp(scan);x64_bind(measured);rt_save(6,RCX);}
    rt_load(RDI,6);rt_cmp(RDI,INT32_MAX-24);
    {int fits=rt_label("mutation.rename.fits");x64_jcc(CC_BE,fits);file_fail(EX_SOFTWARE,"Rename path is too long");x64_bind(fits);}
    x64_add_imm(RDI,24);x64_call(alloc_label);rt_save(7,RAX);rt_load(RCX,6);x64_store(RAX,16,RCX);
    /* Store the 32-bit length before copying: bytes 20..23 overlap FileName. */
    x64_lea(RDI,RAX,20);rt_load(RSI,4);x64_rep_movsb();x64_store8_imm(RDI,0,0);x64_store8_imm(RDI,1,0);rt_load(RCX,5);x64_load(RCX,RCX,0);
    x64_mov_imm(RDX,3);rt_load(R8,7);rt_load(R9,6);x64_add_imm(R9,24);file_win_begin();file_win_end(IMP_SET_FILE_INFORMATION_BY_HANDLE);
    x64_test(RAX,RAX);x64_jcc(CC_NZ,finish);file_windows_error();mutation_error();
    x64_bind(finish);mutation_close(5);x64_xor(RAX,RAX);rt_leave();
}
static void gen_file_mutation_runtime(void)
{
    mutation_path_label=rt_label("mutation.path");mutation_error_label=rt_label("mutation.error");mutation_open_label=rt_label("mutation.open");
    mutation_kind_label=rt_label("mutation.kind");mutation_destination_label=rt_label("mutation.destination");
    gen_mutation_path();gen_mutation_error();gen_mutation_open();gen_mutation_kind();gen_mutation_destination();
    gen_mutation_remove();gen_mutation_copy();gen_mutation_move();
}
