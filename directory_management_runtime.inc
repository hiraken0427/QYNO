/* Owned directory resources, bounded native records, and deterministic names. */
static int dir_path_label,dir_error_label,dir_meta_label,dir_open_label;
static int dir_compare_label,dir_sift_label,dir_sort_label,dir_convert_label;

/* Raw failures retain their OS code; only these APIs add directory mappings. */
static void gen_dir_error(void)
{
    int raise=rt_label("dir.error.raise"),normal=rt_label("dir.error.normal"),done=rt_label("dir.error.done");
    rt_enter(dir_error_label,2);x64_neg(RAX);x64_store(R14,FILE_OS_ERROR,RAX);rt_set(1,0);
    if(target==TARGET_LINUX){
        int other=rt_label("dir.error.other");
        rt_cmp(RAX,20);x64_jcc(CC_NZ,other);rt_set(1,15);x64_jmp(raise);x64_bind(other);
        x64_test(RDI,RDI);x64_jcc(CC_Z,raise);
        rt_cmp(RAX,39);x64_jcc(CC_Z,normal);rt_cmp(RAX,17);x64_jcc(CC_NZ,raise);
        x64_bind(normal);rt_set(1,16);
    }else{
        int notdir=rt_label("dir.error.notdir"),nonempty=rt_label("dir.error.nonempty");
        int busy=rt_label("dir.error.busy"),path=rt_label("dir.error.path");
        rt_cmp(RAX,267);x64_jcc(CC_Z,notdir);rt_cmp(RAX,145);x64_jcc(CC_Z,nonempty);
        rt_cmp(RAX,16);x64_jcc(CC_Z,busy);rt_cmp(RAX,1921);x64_jcc(CC_Z,path);x64_jmp(raise);
        x64_bind(notdir);rt_set(1,15);x64_jmp(raise);
        x64_bind(nonempty);rt_set(1,16);x64_jmp(raise);
        x64_bind(busy);rt_set(1,10);x64_jmp(raise);
        x64_bind(path);rt_set(1,4);
    }
    x64_bind(raise);x64_mov_imm(RDI,11);
    x64_lea_data(RAX,prog_add_str(prog,"Directory operation failed",strlen("Directory operation failed")));x64_call(io_raise_label);
    rt_load(RCX,1);x64_test(RCX,RCX);x64_jcc(CC_Z,done);
    x64_load(RAX,R14,IO_PENDING);x64_load(RAX,RAX,24);x64_store(RAX,0,RCX);
    x64_bind(done);rt_leave();
}
static void dir_os_error(int removing)
{x64_mov_imm(RDI,removing);x64_call(dir_error_label);}
static void dir_policy_error(int code,const char *message)
{
    x64_store_imm(R14,FILE_OS_ERROR,0);x64_mov_imm(RDI,code);
    x64_lea_data(RAX,prog_add_str(prog,message,strlen(message)));x64_call(io_raise_label);
}
/* rax=original str, rdi=remove policy -> native terminated path. */
static void gen_dir_path(void)
{
    int nonempty=rt_label("dir.path.nonempty"),root=rt_label("dir.path.root"),trim=rt_label("dir.path.trim");
    int ready=rt_label("dir.path.ready"),allowed=rt_label("dir.path.allowed"),bad=rt_label("dir.path.bad");
    rt_enter(dir_path_label,3);rt_save(3,RDI);x64_store(R14,FILE_PATH,RAX);
    directory_path_call(9);rt_save(1,RAX);x64_load(RCX,RAX,0);
    x64_test(RCX,RCX);x64_jcc(CC_NZ,nonempty);file_fail(EX_SOFTWARE,"Directory path cannot be empty");
    x64_bind(nonempty);directory_path_call(1);rt_save(2,RAX);x64_load(RCX,RAX,0);
    rt_load(RDX,3);x64_test(RDX,RDX);x64_jcc(CC_Z,allowed);
    x64_test(RCX,RCX);x64_jcc(CC_Z,bad);rt_cmp(RCX,2);x64_jcc(CC_A,allowed);
    x64_load8(RDX,RAX,8);rt_cmp(RDX,'.');x64_jcc(CC_NZ,allowed);rt_cmp(RCX,1);x64_jcc(CC_Z,bad);
    x64_load8(RDX,RAX,9);rt_cmp(RDX,'.');x64_jcc(CC_NZ,allowed);
    x64_bind(bad);file_fail(EX_SOFTWARE,"Cannot remove a root, . or .. directory path");
    x64_bind(allowed);x64_test(RCX,RCX);x64_jcc(CC_Z,root);
    rt_load(RAX,1);x64_load(RCX,RAX,0);
    x64_bind(trim);x64_lea(RDX,RAX,7);x64_add(RDX,RCX);x64_load8(RDI,RDX,0);
    rt_cmp(RDI,'/');x64_jcc(CC_NZ,ready);x64_sub_imm(RCX,1);x64_jmp(trim);
    x64_bind(ready);x64_store(RAX,0,RCX);
    x64_bind(root);rt_load(RAX,1);file_io_call(file_path_label);rt_leave();
}
/* Linux: native path, rdi=AT_SYMLINK_NOFOLLOW -> file type or -errno.
 * Windows: opened handle -> attributes or -GetLastError. */
static void gen_dir_meta(void)
{
    int done=rt_label("dir.meta.done");
    rt_enter(dir_meta_label,20);rt_save(1,RAX);rt_save(2,RDI);
    if(target==TARGET_LINUX){
        int retry=rt_label("dir.meta.retry");x64_bind(retry);
        x64_mov_imm(RDI,-100);rt_load(RSI,1);x64_lea(RDX,RBP,-160);rt_load(R10,2);
        x64_mov_imm(RAX,262);x64_syscall();rt_cmp(RAX,-4);x64_jcc(CC_Z,retry);
        x64_test(RAX,RAX);x64_jcc(CC_S,done);x64_load(RAX,RBP,-136);x64_and_imm(RAX,0xf000);
    }else{
        x64_mov(RCX,RAX);x64_lea(RDX,RBP,-160);file_win_begin();file_win_end(IMP_GET_FILE_INFORMATION_BY_HANDLE);
        x64_test(RAX,RAX);x64_jcc(CC_NZ,done=rt_label("dir.meta.success"));file_windows_error();rt_leave();
        x64_bind(done);x64_load(RAX,RBP,-160);x64_shl_imm(RAX,32);x64_shr_imm(RAX,32);rt_leave();return;
    }
    x64_bind(done);rt_leave();
}
/* Windows native path + operation 0=query,1=list,2=remove -> raw handle/-error. */
static void gen_dir_open(void)
{
    int list=rt_label("dir.open.list"),remove=rt_label("dir.open.remove"),call=rt_label("dir.open.call"),good=rt_label("dir.open.good");
    if(target!=TARGET_WINDOWS)return;
    rt_enter(dir_open_label,0);x64_mov(RCX,RAX);x64_mov_imm(RDX,0x80);x64_mov_imm(R10,0x02000000);
    rt_cmp(RDI,1);x64_jcc(CC_Z,list);rt_cmp(RDI,2);x64_jcc(CC_Z,remove);x64_jmp(call);
    x64_bind(list);x64_mov_imm(RDX,0x81);x64_jmp(call);
    x64_bind(remove);x64_mov_imm(RDX,0x10080);x64_mov_imm(R10,0x02200000);
    x64_bind(call);x64_mov_imm(R8,7);x64_xor(R9,R9);file_win_begin();
    x64_store_imm(RSP,32,3);x64_store(RSP,40,R10);x64_store_imm(RSP,48,0);file_win_end(IMP_CREATE_FILE_W);
    rt_cmp(RAX,-1);x64_jcc(CC_NZ,good);file_windows_error();x64_bind(good);rt_leave();
}
static void dir_control(int slot)
{
    x64_mov_imm(RDI,48);x64_call(alloc_label);rt_save(slot,RAX);x64_store_imm(RAX,-8,2);
    x64_load(RCX,R14,FILE_PATH);x64_store(RAX,16,RCX);
}
static void dir_adopt(int slot)
{rt_load(RCX,slot);x64_store(RCX,0,RAX);x64_store_imm(RCX,8,1);}
static void dir_close(int slot)
{rt_load(RAX,slot);x64_call(file_bulk_close_label);}
static void gen_dir_exists(void)
{
    int missing=rt_label("dir.exists.missing"),good=rt_label("dir.exists.good"),finish=rt_label("dir.exists.close");
    rt_enter(directory_labels[3],4);x64_load(RAX,RBP,16);x64_xor(RDI,RDI);file_io_call(dir_path_label);rt_save(1,RAX);
    if(target==TARGET_WINDOWS){dir_control(2);rt_load(RAX,1);x64_xor(RDI,RDI);x64_call(dir_open_label);}
    else{x64_xor(RDI,RDI);x64_call(dir_meta_label);}
    rt_cmp(RAX,-2);x64_jcc(CC_Z,missing);rt_cmp(RAX,target==TARGET_LINUX?-20:-3);x64_jcc(CC_Z,missing);
    if(target==TARGET_WINDOWS){rt_cmp(RAX,-267);x64_jcc(CC_Z,missing);}
    x64_test(RAX,RAX);x64_jcc(CC_NS,good);dir_os_error(0);rt_leave();
    x64_bind(good);
    if(target==TARGET_WINDOWS){
        dir_adopt(2);x64_call(dir_meta_label);x64_test(RAX,RAX);
        {int known=rt_label("dir.exists.known");x64_jcc(CC_NS,known);dir_os_error(0);x64_jmp(finish);x64_bind(known);}
        x64_and_imm(RAX,16);x64_test(RAX,RAX);x64_setcc(CC_NZ,RAX);
    }else{rt_cmp(RAX,0x4000);x64_setcc(CC_Z,RAX);}
    x64_movzx8(RAX,RAX);rt_save(3,RAX);x64_bind(finish);
    if(target==TARGET_WINDOWS){dir_close(2);io_rt_check();}
    rt_load(RAX,3);rt_leave();x64_bind(missing);x64_xor(RAX,RAX);rt_leave();
}
static void gen_dir_create(void)
{
    int good=rt_label("dir.create.good");
    rt_enter(directory_labels[4],2);x64_load(RAX,RBP,16);x64_xor(RDI,RDI);file_io_call(dir_path_label);rt_save(1,RAX);
    if(target==TARGET_LINUX){
        x64_mov(RSI,RAX);x64_mov_imm(RDI,-100);x64_mov_imm(RDX,0777);x64_mov_imm(RAX,258);x64_syscall();
        x64_test(RAX,RAX);x64_jcc(CC_NS,good);
    }else{
        x64_mov(RCX,RAX);x64_xor(RDX,RDX);file_win_begin();file_win_end(IMP_CREATE_DIRECTORY_W);
        x64_test(RAX,RAX);x64_jcc(CC_NZ,good);file_windows_error();
    }
    dir_os_error(0);rt_leave();x64_bind(good);x64_xor(RAX,RAX);rt_leave();
}
static void gen_dir_remove(void)
{
    int good=rt_label("dir.remove.good"),known=rt_label("dir.remove.known"),link=rt_label("dir.remove.link");
    int wrong=rt_label("dir.remove.type"),finish=rt_label("dir.remove.close");
    rt_enter(directory_labels[6],4);x64_load(RAX,RBP,16);x64_mov_imm(RDI,1);file_io_call(dir_path_label);rt_save(1,RAX);
    if(target==TARGET_WINDOWS){
        dir_control(2);rt_load(RAX,1);x64_mov_imm(RDI,2);x64_call(dir_open_label);
        x64_test(RAX,RAX);x64_jcc(CC_NS,known);dir_os_error(1);rt_leave();x64_bind(known);
        dir_adopt(2);x64_call(dir_meta_label);
    }else{x64_mov_imm(RDI,256);x64_call(dir_meta_label);}
    x64_test(RAX,RAX);x64_jcc(CC_NS,known=rt_label("dir.remove.metadata"));dir_os_error(1);x64_jmp(finish);x64_bind(known);
    if(target==TARGET_WINDOWS){
        x64_mov(RCX,RAX);x64_and_imm(RCX,0x400);x64_test(RCX,RCX);x64_jcc(CC_NZ,link);
        x64_and_imm(RAX,16);x64_test(RAX,RAX);x64_jcc(CC_Z,wrong);
        rt_load(RCX,2);x64_load(RCX,RCX,0);x64_mov_imm(RDX,4);rt_set(3,1);x64_lea(R8,RBP,-24);x64_mov_imm(R9,1);
        file_win_begin();file_win_end(IMP_SET_FILE_INFORMATION_BY_HANDLE);
        x64_test(RAX,RAX);x64_jcc(CC_NZ,good);file_windows_error();
    }else{
        rt_cmp(RAX,0xa000);x64_jcc(CC_Z,link);rt_cmp(RAX,0x4000);x64_jcc(CC_NZ,wrong);
        x64_mov_imm(RDI,-100);rt_load(RSI,1);x64_mov_imm(RDX,512);x64_mov_imm(RAX,263);x64_syscall();
        x64_test(RAX,RAX);x64_jcc(CC_NS,good);
    }
    dir_os_error(1);x64_jmp(finish);
    x64_bind(link);dir_policy_error(12,"Removing a directory link is unsupported");x64_jmp(finish);
    x64_bind(wrong);dir_policy_error(15,"Target is not a directory");x64_jmp(finish);
    x64_bind(good);x64_bind(finish);if(target==TARGET_WINDOWS)dir_close(2);
    x64_xor(RAX,RAX);rt_leave();
}
/* Unsigned UTF-8 byte comparison, independent of locale and terminators. */
static void gen_dir_compare(void)
{
    int loop=rt_label("dir.compare.loop"),end=rt_label("dir.compare.end"),less=rt_label("dir.compare.less"),more=rt_label("dir.compare.more");
    x64_bind(dir_compare_label);x64_load(RDX,RAX,0);x64_load(R8,RDI,0);x64_lea(R10,RAX,8);x64_lea(R9,RDI,8);x64_xor(RCX,RCX);
    x64_bind(loop);x64_cmp(RCX,RDX);x64_jcc(CC_AE,end);x64_cmp(RCX,R8);x64_jcc(CC_AE,end);
    x64_load8(RSI,R10,0);x64_load8(RDI,R9,0);x64_cmp(RSI,RDI);x64_jcc(CC_B,less);x64_jcc(CC_A,more);
    x64_add_imm(R10,1);x64_add_imm(R9,1);x64_add_imm(RCX,1);x64_jmp(loop);
    x64_bind(end);x64_cmp(RDX,R8);x64_jcc(CC_B,less);x64_jcc(CC_A,more);x64_xor(RAX,RAX);x64_ret();
    x64_bind(less);x64_mov_imm(RAX,-1);x64_ret();x64_bind(more);x64_mov_imm(RAX,1);x64_ret();
}
static void dir_load_name(Reg out,int index_slot,int data_slot)
{
    rt_load(out,index_slot);x64_shl_imm(out,3);rt_load(R11,data_slot);x64_add(out,R11);x64_load(out,out,0);
}
static void gen_dir_sort(void)
{
    int loop=rt_label("dir.sift.loop"),child=rt_label("dir.sift.child"),done=rt_label("dir.sift.done");
    int build=rt_label("dir.sort.build"),extract=rt_label("dir.sort.extract"),sorted=rt_label("dir.sort.done");
    rt_enter(dir_sift_label,6);rt_save(1,RAX);rt_save(2,RDI);rt_save(3,RSI);
    x64_bind(loop);rt_load(RCX,2);x64_shl_imm(RCX,1);x64_add_imm(RCX,1);rt_load(RDX,3);
    x64_cmp(RCX,RDX);x64_jcc(CC_AE,done);rt_save(4,RCX);x64_add_imm(RCX,1);rt_save(5,RCX);
    x64_cmp(RCX,RDX);x64_jcc(CC_AE,child);
    dir_load_name(RAX,4,1);dir_load_name(RDI,5,1);x64_call(dir_compare_label);x64_test(RAX,RAX);x64_jcc(CC_NS,child);
    rt_load(RCX,5);rt_save(4,RCX);
    x64_bind(child);dir_load_name(RAX,2,1);dir_load_name(RDI,4,1);x64_call(dir_compare_label);
    x64_test(RAX,RAX);x64_jcc(CC_NS,done);
    rt_load(RCX,2);x64_shl_imm(RCX,3);rt_load(RDX,1);x64_add(RCX,RDX);
    rt_load(RSI,4);x64_shl_imm(RSI,3);x64_add(RSI,RDX);
    x64_load(RAX,RCX,0);x64_load(RDI,RSI,0);x64_store(RCX,0,RDI);x64_store(RSI,0,RAX);
    rt_load(RCX,4);rt_save(2,RCX);x64_jmp(loop);x64_bind(done);rt_leave();

    rt_enter(dir_sort_label,4);rt_save(1,RAX);x64_load(RCX,RAX,24);rt_save(2,RCX);
    x64_load(RCX,RAX,0);rt_save(3,RCX);x64_shr_imm(RCX,1);rt_save(4,RCX);
    x64_bind(build);rt_load(RDI,4);x64_test(RDI,RDI);x64_jcc(CC_Z,extract);
    x64_sub_imm(RDI,1);rt_save(4,RDI);rt_load(RAX,2);rt_load(RSI,3);x64_call(dir_sift_label);x64_jmp(build);
    x64_bind(extract);rt_load(RSI,3);rt_cmp(RSI,1);x64_jcc(CC_BE,sorted);x64_sub_imm(RSI,1);rt_save(3,RSI);
    rt_load(RAX,2);x64_mov(RDX,RSI);x64_shl_imm(RDX,3);x64_add(RDX,RAX);
    x64_load(RCX,RAX,0);x64_load(RDI,RDX,0);x64_store(RAX,0,RDI);x64_store(RDX,0,RCX);
    x64_xor(RDI,RDI);x64_call(dir_sift_label);x64_jmp(extract);
    x64_bind(sorted);rt_load(RAX,1);rt_leave();
}
static void gen_dir_convert(void)
{
    int good=rt_label("dir.name.convert.good"),done=rt_label("dir.name.convert.done"),os=rt_label("dir.name.convert.os");
    if(target!=TARGET_WINDOWS)return;
    rt_enter(dir_convert_label,6);rt_save(2,RAX);x64_load(RCX,RAX,0);x64_shr_imm(RCX,1);rt_save(3,RCX);
    x64_add_imm(RAX,8);rt_save(1,RAX);directory_windows_encode(0);
    x64_test(RAX,RAX);x64_jcc(CC_NZ,good);file_windows_error();rt_cmp(RAX,-1113);x64_jcc(CC_NZ,os);
    file_fail(EX_DATAERR,"Directory entry is not valid Unicode");x64_bind(os);dir_os_error(0);rt_leave();
    x64_bind(good);rt_save(5,RAX);x64_mov(RDI,RAX);x64_add_imm(RDI,8);x64_call(alloc_label);rt_save(4,RAX);
    rt_load(RCX,5);x64_store(RAX,0,RCX);directory_windows_encode(1);
    x64_test(RAX,RAX);x64_jcc(CC_NZ,done);file_windows_error();rt_cmp(RAX,-1113);x64_jcc(CC_NZ,os);
    file_fail(EX_DATAERR,"Directory entry is not valid Unicode");
    x64_bind(done);rt_load(RAX,4);rt_leave();
}
static void dir_contract_limit(Reg reg,long long cap)
{
    int bad=rt_label("dir.limit.bad"),good=rt_label("dir.limit.good");
    x64_test(reg,reg);x64_jcc(CC_S,bad);rt_cmp(reg,cap);x64_jcc(CC_BE,good);
    x64_bind(bad);file_fail(EX_SOFTWARE,"Directory limit is out of range");x64_bind(good);
}
static void gen_dir_list(void)
{
    int opened=rt_label("dir.list.opened"),read=rt_label("dir.list.read"),page=rt_label("dir.list.page");
    int record=rt_label("dir.list.record"),scan=rt_label("dir.list.nul"),name=rt_label("dir.list.name");
    int include=rt_label("dir.list.include"),next=rt_label("dir.list.next"),finish=rt_label("dir.list.close");
    int large=rt_label("dir.list.limit"),malformed=rt_label("dir.list.malformed"),wrong=rt_label("dir.list.type");
    int convert=rt_label("dir.list.convert"),sorted=rt_label("dir.list.sort");
    /* Slots: native path, owned control, names, page, count cap, byte cap,
     * total, page end, record offset, next offset, name ptr, name bytes,
     * conversion index, temporary name, Windows info class, spare. */
    rt_enter(directory_labels[5],16);x64_load(RAX,RBP,32);x64_store(R14,FILE_PATH,RAX);
    x64_load(RDI,RBP,24);dir_contract_limit(RDI,(INT64_MAX-32)/8);rt_save(5,RDI);
    x64_load(RDI,RBP,16);dir_contract_limit(RDI,INT64_MAX-32);rt_save(6,RDI);
    x64_load(RAX,RBP,32);x64_xor(RDI,RDI);file_io_call(dir_path_label);rt_save(1,RAX);
    x64_mov_imm(RDI,40);x64_call(alloc_label);rt_save(3,RAX);
    collection_shape(array_type(TY_STR),NULL,RDI);x64_store(RAX,16,RDI);
    x64_mov_imm(RDI,65536);x64_call(alloc_label);rt_save(4,RAX);rt_set(7,0);rt_set(15,15);
    dir_control(2);
    if(target==TARGET_LINUX){
        int retry=rt_label("dir.list.open.retry");x64_bind(retry);rt_load(RSI,1);x64_mov_imm(RDI,-100);
        x64_mov_imm(RDX,0x90000);x64_xor(R10,R10);x64_mov_imm(RAX,257);x64_syscall();
        rt_cmp(RAX,-4);x64_jcc(CC_Z,retry);
    }else{rt_load(RAX,1);x64_mov_imm(RDI,1);x64_call(dir_open_label);}
    x64_test(RAX,RAX);x64_jcc(CC_NS,opened);dir_os_error(0);rt_leave();
    x64_bind(opened);dir_adopt(2);
    if(target==TARGET_WINDOWS){
        x64_call(dir_meta_label);x64_test(RAX,RAX);
        {int known=rt_label("dir.list.meta");x64_jcc(CC_NS,known);dir_os_error(0);x64_jmp(finish);x64_bind(known);}
        x64_and_imm(RAX,16);x64_test(RAX,RAX);x64_jcc(CC_Z,wrong);
    }
    x64_bind(read);
    if(target==TARGET_LINUX){
        rt_load(RDI,2);x64_load(RDI,RDI,0);rt_load(RSI,4);x64_mov_imm(RDX,65536);x64_mov_imm(RAX,217);x64_syscall();
        rt_cmp(RAX,-4);x64_jcc(CC_Z,read);x64_test(RAX,RAX);x64_jcc(CC_Z,finish);x64_jcc(CC_NS,page);
        dir_os_error(0);x64_jmp(finish);
    }else{
        int zero=rt_label("dir.list.zero.page");rt_load(RDX,4);x64_mov_imm(RCX,8192);
        x64_bind(zero);x64_store_imm(RDX,0,0);x64_add_imm(RDX,8);x64_sub_imm(RCX,1);x64_jcc(CC_NZ,zero);
        rt_load(RCX,2);x64_load(RCX,RCX,0);rt_load(RDX,15);rt_load(R8,4);x64_mov_imm(R9,65536);
        file_win_begin();file_win_end(IMP_GET_FILE_INFORMATION_BY_HANDLE_EX);
        x64_test(RAX,RAX);x64_jcc(CC_NZ,page);file_windows_error();rt_cmp(RAX,-18);x64_jcc(CC_Z,finish);
        dir_os_error(0);x64_jmp(finish);
    }
    x64_bind(page);
    if(target==TARGET_WINDOWS){x64_mov_imm(RAX,65536);rt_set(15,14);}
    rt_cmp(RAX,65536);x64_jcc(CC_A,malformed);rt_save(8,RAX);rt_set(9,0);
    x64_bind(record);rt_load(RCX,9);rt_load(RDX,8);x64_cmp(RCX,RDX);x64_jcc(CC_AE,read);
    x64_sub(RDX,RCX);rt_cmp(RDX,target==TARGET_LINUX?20:70);x64_jcc(CC_B,malformed);
    rt_load(RSI,4);x64_add(RSI,RCX);
    if(target==TARGET_LINUX){
        int terminated=rt_label("dir.list.terminated");
        x64_load8(RAX,RSI,17);x64_shl_imm(RAX,8);x64_load8(R8,RSI,16);x64_add(RAX,R8);rt_cmp(RAX,20);x64_jcc(CC_B,malformed);
        x64_cmp(RAX,RDX);x64_jcc(CC_A,malformed);x64_add(RAX,RCX);rt_save(10,RAX);
        x64_add_imm(RSI,19);rt_save(11,RSI);x64_add_imm(RCX,19);x64_xor(RDX,RDX);
        x64_bind(scan);x64_cmp(RCX,RAX);x64_jcc(CC_AE,malformed);x64_load8(RDI,RSI,0);
        x64_test(RDI,RDI);x64_jcc(CC_Z,terminated);x64_add_imm(RSI,1);x64_add_imm(RCX,1);x64_add_imm(RDX,1);x64_jmp(scan);
        x64_bind(terminated);x64_test(RDX,RDX);x64_jcc(CC_Z,malformed);rt_save(12,RDX);
    }else{
        int last=rt_label("dir.list.last.record"),offset=rt_label("dir.list.offset.ready");
        x64_load(RAX,RSI,0);x64_shl_imm(RAX,32);x64_shr_imm(RAX,32);
        x64_load(RDI,RSI,60);x64_shl_imm(RDI,32);x64_shr_imm(RDI,32);rt_save(12,RDI);
        x64_test(RDI,RDI);x64_jcc(CC_Z,malformed);x64_mov(R8,RDI);x64_and_imm(R8,1);x64_jcc(CC_NZ,malformed);
        x64_add_imm(RDI,68);x64_cmp(RDI,RDX);x64_jcc(CC_A,malformed);
        x64_test(RAX,RAX);x64_jcc(CC_Z,last);x64_cmp(RAX,RDI);x64_jcc(CC_B,malformed);x64_cmp(RAX,RDX);x64_jcc(CC_AE,malformed);
        x64_mov(R8,RAX);x64_and_imm(R8,7);x64_jcc(CC_NZ,malformed);x64_add(RAX,RCX);x64_jmp(offset);
        x64_bind(last);rt_load(RAX,8);x64_bind(offset);rt_save(10,RAX);x64_add_imm(RSI,68);rt_save(11,RSI);
        {int unit=rt_label("dir.list.native.unit"),checked=rt_label("dir.list.native.checked");
         rt_load(RDX,12);x64_xor(RCX,RCX);x64_mov(RDI,RSI);
         x64_bind(unit);x64_cmp(RCX,RDX);x64_jcc(CC_AE,checked);
         x64_load8(RAX,RDI,0);x64_load8(R8,RDI,1);x64_or(RAX,R8);x64_test(RAX,RAX);x64_jcc(CC_Z,malformed);
         x64_add_imm(RDI,2);x64_add_imm(RCX,2);x64_jmp(unit);x64_bind(checked);}
    }
    /* Exclude only the exact native encodings of dot and dot-dot. */
    x64_bind(name);rt_load(RDX,12);rt_load(RSI,11);
    rt_cmp(RDX,target==TARGET_LINUX?1:2);x64_jcc(CC_Z,scan=rt_label("dir.list.dot"));
    rt_cmp(RDX,target==TARGET_LINUX?2:4);x64_jcc(CC_NZ,include);
    x64_load8(RAX,RSI,target==TARGET_LINUX?1:2);rt_cmp(RAX,'.');x64_jcc(CC_NZ,include);
    if(target==TARGET_WINDOWS){x64_load8(RAX,RSI,3);x64_test(RAX,RAX);x64_jcc(CC_NZ,include);}
    x64_bind(scan);x64_load8(RAX,RSI,0);rt_cmp(RAX,'.');x64_jcc(CC_NZ,include);
    if(target==TARGET_WINDOWS){x64_load8(RAX,RSI,1);x64_test(RAX,RAX);x64_jcc(CC_NZ,include);}
    x64_jmp(next);
    x64_bind(include);rt_load(RAX,3);x64_load(RCX,RAX,0);rt_load(RDI,5);x64_cmp(RCX,RDI);x64_jcc(CC_AE,large);
    rt_load(RAX,12);if(target==TARGET_WINDOWS)x64_shr_imm(RAX,1);
    rt_load(RDI,6);rt_load(RCX,7);x64_sub(RDI,RCX);x64_cmp(RAX,RDI);x64_jcc(CC_A,large);
    x64_add(RAX,RCX);rt_save(7,RAX);rt_load(RDI,12);x64_add_imm(RDI,8);x64_call(alloc_label);rt_save(14,RAX);
    rt_load(RCX,12);x64_store(RAX,0,RCX);x64_lea(RDI,RAX,8);rt_load(RSI,11);x64_rep_movsb();
    rt_load(RAX,3);rt_load(RDI,14);x64_call(array_append_label);
    x64_bind(next);rt_load(RCX,10);rt_save(9,RCX);x64_jmp(record);
    x64_bind(large);dir_policy_error(13,"Directory listing exceeds its limit");x64_jmp(finish);
    x64_bind(malformed);dir_policy_error(14,"Malformed native directory record");x64_jmp(finish);
    x64_bind(wrong);dir_policy_error(15,"Target is not a directory");
    x64_bind(finish);dir_close(2);io_rt_check();rt_set(13,0);rt_set(7,0);
    x64_bind(convert);rt_load(RCX,13);rt_load(RAX,3);x64_load(RDX,RAX,0);x64_cmp(RCX,RDX);x64_jcc(CC_AE,sorted);
    x64_load(RAX,RAX,24);x64_shl_imm(RCX,3);x64_add(RAX,RCX);x64_load(RAX,RAX,0);
    x64_call(target==TARGET_WINDOWS?dir_convert_label:file_validate_label);io_rt_check();rt_save(14,RAX);
    if(target==TARGET_WINDOWS){
        x64_load(RDX,RAX,0);rt_load(RCX,7);x64_add(RCX,RDX);x64_jcc(CC_O,large=rt_label("dir.list.utf8.limit"));
        rt_load(RDI,6);x64_cmp(RCX,RDI);x64_jcc(CC_A,large);rt_save(7,RCX);
    }
    rt_load(RDX,3);x64_load(RDX,RDX,24);rt_load(RCX,13);x64_shl_imm(RCX,3);x64_add(RDX,RCX);rt_load(RAX,14);x64_store(RDX,0,RAX);
    rt_load(RCX,13);x64_add_imm(RCX,1);rt_save(13,RCX);x64_jmp(convert);
    if(target==TARGET_WINDOWS){x64_bind(large);dir_policy_error(13,"Directory listing exceeds max_name_bytes");rt_leave();}
    x64_bind(sorted);rt_load(RAX,3);x64_call(dir_sort_label);rt_leave();
}
static void gen_directory_management_runtime(void)
{
    dir_path_label=rt_label("dir.path");dir_error_label=rt_label("dir.error");dir_meta_label=rt_label("dir.meta");dir_open_label=rt_label("dir.open");
    dir_compare_label=rt_label("dir.compare");dir_sift_label=rt_label("dir.sift");dir_sort_label=rt_label("dir.sort");dir_convert_label=rt_label("dir.convert");
    gen_dir_path();gen_dir_error();gen_dir_meta();gen_dir_open();gen_dir_compare();gen_dir_sort();gen_dir_convert();
    gen_dir_exists();gen_dir_create();gen_dir_remove();gen_dir_list();
}
