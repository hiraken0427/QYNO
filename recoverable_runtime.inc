/* Public io.Error fields retain the ordinary model layout. Error records add
 * hidden previous/source pointers at 40/48 and the nominal domain at 56.
 * Public copies contain fields only; throw preserves the original record. */
static void io_classify(int os_code,int code,int done)
{
    int next=rt_label("io.classify.next");
    rt_cmp(RAX,os_code);x64_jcc(CC_NZ,next);rt_set(2,code);x64_jmp(done);x64_bind(next);
}
static void io_emit_error_fields(int failure)
{
    int i;
    x64_mov_imm(RDI,64);x64_call(alloc_label);rt_save(3,RAX);
    x64_store_imm(RAX,ERROR_DOMAIN_SLOT,ERROR_DOMAIN_IO); /* error domain: io */
    x64_store_imm(RAX,0,8);x64_store_imm(RAX,8,8);
    x64_mov_imm(RDI,64);x64_call(alloc_label);rt_save(4,RAX);
    rt_load(RCX,3);x64_store(RCX,24,RAX);
    for(i=0;i<8;i++){
        if(i==0){if(failure)rt_load(RDX,2);else x64_xor(RDX,RDX);}
        else if(i>=6){if(failure)x64_load(RDX,R14,i==6?IO_LINE:IO_COLUMN);else x64_xor(RDX,RDX);}
        else if(i==4){if(failure)x64_load(RDX,R14,FILE_OS_ERROR);else x64_xor(RDX,RDX);}
        else if(!failure)x64_lea_data(RDX,empty_str_offset);
        else if(i==1)rt_load(RDX,1);
        else if(i==2)x64_load(RDX,R14,IO_OPERATION);
        else if(i==3){int known=rt_label("io.path.known");x64_load(RDX,R14,FILE_PATH);x64_test(RDX,RDX);x64_jcc(CC_NZ,known);x64_lea_data(RDX,empty_str_offset);x64_bind(known);}
        else x64_load(RDX,R14,IO_SOURCE);
        rt_load(RAX,4);x64_store(RAX,i*8,RDX);
    }
    rt_load(RAX,3);
    if(failure){x64_load(RCX,R14,FILE_CONTEXT);x64_store(RAX,48,RCX);x64_store(R14,IO_PENDING,RAX);}
}
static void gen_recoverable_runtime(void)
{
    int classified=rt_label("io.classified"),empty=rt_label("io.no.os.error");
    rt_enter(io_raise_label,4);rt_save(1,RAX);rt_save(2,RDI);
    x64_load(RAX,R14,FILE_OS_ERROR);x64_test(RAX,RAX);x64_jcc(CC_Z,empty);
    rt_load(RCX,2);rt_cmp(RCX,12);x64_jcc(CC_AE,empty);
    rt_set(2,11);
    if(target==TARGET_LINUX){
        io_classify(38,12,classified);io_classify(95,12,classified);io_classify(29,12,classified);io_classify(75,13,classified);
        io_classify(2,1,classified);io_classify(13,2,classified);io_classify(1,2,classified);
        io_classify(17,3,classified);io_classify(36,4,classified);io_classify(20,4,classified);io_classify(40,4,classified);
        io_classify(21,5,classified);io_classify(24,6,classified);io_classify(23,6,classified);
        io_classify(28,7,classified);io_classify(122,7,classified);io_classify(30,8,classified);
        io_classify(32,9,classified);io_classify(16,10,classified);io_classify(26,10,classified);
    }else{
        io_classify(1,12,classified);io_classify(50,12,classified);io_classify(120,12,classified);io_classify(534,13,classified);
        io_classify(2,1,classified);io_classify(3,1,classified);io_classify(5,2,classified);
        io_classify(80,3,classified);io_classify(183,3,classified);io_classify(123,4,classified);
        io_classify(161,4,classified);io_classify(206,4,classified);io_classify(267,4,classified);
        io_classify(4,6,classified);io_classify(39,7,classified);io_classify(112,7,classified);
        io_classify(19,8,classified);io_classify(109,9,classified);io_classify(232,9,classified);
        io_classify(32,10,classified);io_classify(33,10,classified);io_classify(170,10,classified);
    }
    x64_bind(empty);x64_bind(classified);io_emit_error_fields(1);rt_leave();
    rt_enter(io_empty_label,4);io_emit_error_fields(0);rt_leave();

    /* Unhandled propagated error and any superseded cleanup errors. */
    {int loop=rt_label("io.report.loop"),done=rt_label("io.report.done"),io=rt_label("error.exit.io"),exit_ready=rt_label("error.exit.ready");
     rt_enter(io_unhandled_label,3);x64_load(RAX,R14,IO_PENDING);rt_save(1,RAX);rt_set(3,EX_IOERR);
     x64_load(RCX,RAX,ERROR_DOMAIN_SLOT);x64_test(RCX,RCX);x64_jcc(CC_Z,io);
     x64_load(RCX,RAX,24);x64_load(RCX,RCX,0);rt_set(3,EX_SOFTWARE);
     rt_cmp(RCX,1);x64_jcc(CC_NZ,exit_ready);rt_set(3,130);x64_jmp(io);
     x64_bind(exit_ready);rt_cmp(RCX,2);x64_jcc(CC_NZ,io);rt_set(3,124);x64_bind(io);
     x64_bind(loop);rt_load(RAX,1);x64_test(RAX,RAX);x64_jcc(CC_Z,done);
     x64_load(RCX,RAX,48);x64_store(R14,FILE_CONTEXT,RCX);
     x64_load(RDX,RAX,24);x64_load(RCX,RAX,ERROR_DOMAIN_SLOT);x64_test(RCX,RCX);
     {int task=rt_label("error.report.task"),report=rt_label("error.report.message");
      x64_jcc(CC_NZ,task);x64_load(RCX,RDX,24);x64_store(R14,FILE_PATH,RCX);
      x64_load(RCX,RDX,32);x64_store(R14,FILE_OS_ERROR,RCX);x64_jmp(report);
      x64_bind(task);x64_store_imm(R14,FILE_PATH,0);x64_store_imm(R14,FILE_OS_ERROR,0);
      x64_bind(report);}
     x64_load(RAX,RDX,8);rt_load(R8,3);x64_call(io_report_label);
     rt_load(RAX,1);x64_load(RAX,RAX,40);rt_save(1,RAX);x64_jmp(loop);
     x64_bind(done);rt_load(RAX,3);x64_jmp(exit_label);}

    /* try_open(path, ref destination, ref error, mode). */
    {int valid=rt_label("io.destination.valid"),failed=rt_label("io.try.open.failed");
     rt_enter(file_labels[BI_FILE_TRY_OPEN-BI_FILE_OPEN],4);
     x64_load(RAX,RBP,32);x64_load(RCX,RAX,0);x64_test(RCX,RCX);x64_jcc(CC_Z,valid);
     x64_load(RCX,RCX,8);x64_test(RCX,RCX);x64_jcc(CC_Z,valid);
     x64_lea_data(RAX,prog_add_str(prog,QMSG_0353,strlen(QMSG_0353)));
     x64_mov_imm(R8,EX_SOFTWARE);x64_jmp(file_error_label);
     x64_bind(valid);x64_load(RAX,RBP,40);x64_push(RAX);x64_load(RAX,RBP,16);x64_push(RAX);
     x64_call(file_labels[0]);x64_add_imm(RSP,16);rt_save(1,RAX);
     x64_load(RCX,R14,IO_PENDING);x64_test(RCX,RCX);x64_jcc(CC_NZ,failed);
     x64_load(RDX,RBP,32);x64_store(RDX,0,RAX);x64_call(io_empty_label);
     x64_load(RDX,RBP,24);x64_store(RDX,0,RAX);x64_mov_imm(RAX,1);rt_leave();
     x64_bind(failed);x64_load(RDX,RBP,24);x64_store(RDX,0,RCX);
     x64_store_imm(R14,IO_PENDING,0);x64_xor(RAX,RAX);rt_leave();}
}

/* Build a common immutable view. The hidden original is kept separately for
 * throw; public code/domain metadata never changes the underlying failure. */
static void gen_common_error_runtime(void)
{
    int task=rt_label("error.wrap.task"),fields=rt_label("error.wrap.fields"),i;
    rt_enter(error_wrap_label,6);rt_save(1,RAX);
    x64_mov_imm(RDI,40);x64_call(alloc_label);rt_save(2,RAX);
    x64_store_imm(RAX,0,7);x64_store_imm(RAX,8,7);
    x64_mov_imm(RDI,56);x64_call(alloc_label);rt_save(3,RAX);
    rt_load(RCX,2);x64_store(RCX,24,RAX);
    rt_load(RCX,1);x64_load(RDX,RCX,24);rt_save(4,RDX);x64_load(RCX,RCX,ERROR_DOMAIN_SLOT);rt_save(5,RCX);
    x64_load(R8,RDX,0);x64_shl_imm(RCX,ERROR_CODE_SHIFT);x64_or(R8,RCX);x64_store(RAX,0,R8);
    x64_load(RCX,RDX,8);x64_store(RAX,8,RCX);
    rt_load(RCX,5);x64_test(RCX,RCX);x64_jcc(CC_NZ,task);
    for(i=0;i<3;i++){x64_load(RCX,RDX,(i+5)*8);x64_store(RAX,(i+2)*8,RCX);}
    x64_lea_data(RCX,prog_add_str(prog,"io",2));x64_store(RAX,40,RCX);
    x64_jmp(fields);x64_bind(task);
    for(i=0;i<3;i++){x64_load(RCX,RDX,(i+2)*8);x64_store(RAX,(i+2)*8,RCX);}
    x64_lea_data(RCX,prog_add_str(prog,"task",4));x64_store(RAX,40,RCX);
    x64_bind(fields);
    x64_mov_imm(RDI,40);x64_call(alloc_label);rt_save(6,RAX);
    rt_load(RCX,3);x64_store(RCX,48,RAX);
    rt_load(RCX,5);x64_test(RCX,RCX);
    {int done=rt_label("error.wrap.done");x64_jcc(CC_Z,done);
     rt_load(RDX,4);x64_load(RAX,RDX,40);rt_load(RCX,3);x64_store(RCX,48,RAX);x64_bind(done);}
    rt_load(RAX,2);rt_leave();
}
