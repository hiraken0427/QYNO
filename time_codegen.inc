static void gen_time_builtin(Node *node)
{
    Function fn={0};Node call=*node;fn.label=time_labels[(int)node->val-BI_TIME_NOW];call.func=&fn;gen_call(&call);
}
static void time_os_failure(const char *message)
{
    int generic=rt_label("time.os.generic"),raised=rt_label("time.os.raised");
    x64_neg(RAX);x64_store(R14,FILE_OS_ERROR,RAX);x64_mov_imm(RDI,14);
    rt_cmp(RAX,target==TARGET_LINUX?38:120);x64_jcc(CC_Z,generic);
    rt_cmp(RAX,target==TARGET_LINUX?22:1);x64_jcc(CC_Z,generic);
    rt_cmp(RAX,target==TARGET_LINUX?95:50);x64_jcc(CC_NZ,raised);
    x64_bind(generic);x64_mov_imm(RDI,12);x64_bind(raised);
    x64_lea_data(RAX,prog_add_str(prog,message,strlen(message)));x64_call(io_raise_label);rt_leave();
}
static void time_quad_integer(long long value,Reg dst)
{
    WideRequest request={0};request.op=W_CONVERT;request.ty=W_QUAD;request.from=-64;request.a.lo=(uint64_t)value;wide_execute(&request);
    x64_lea_data(dst,prog_add_data(prog,(const char *)&request.out,sizeof(request.out)));
}
static void time_i128_integer(long long value,Reg dst)
{WideBits b={(uint64_t)value,value<0?UINT64_MAX:0};x64_lea_data(dst,prog_add_data(prog,(const char *)&b,sizeof(b)));}
static void gen_time_numeric_runtime(void)
{
    /* Integer seconds + nanoseconds -> binary128 seconds. */
    rt_enter(time_timespec_label,4);rt_save(1,RAX);rt_save(2,RDI);
    gen_wide(W_CONVERT,TY_QUAD,TY_I64,0,NULL);rt_save(3,RAX);
    rt_load(RAX,2);gen_wide(W_CONVERT,TY_QUAD,TY_I64,0,NULL);time_quad_integer(1000000000,RDI);gen_wide(W_DIV,TY_QUAD,TY_QUAD,0,NULL);
    rt_load(RDI,3);gen_wide(W_ADD,TY_QUAD,TY_QUAD,0,NULL);rt_leave();
    /* Unsigned FILETIME ticks -> signed integer epoch delta -> binary128. */
    rt_enter(time_filetime_label,0);gen_wide(W_CONVERT,TY_I128,TY_U64,0,NULL);
    time_i128_integer(116444736000000000LL,RDI);gen_wide(W_SUB,TY_I128,TY_I128,0,NULL);
    gen_wide(W_CONVERT,TY_QUAD,TY_I128,0,NULL);time_quad_integer(10000000,RDI);gen_wide(W_DIV,TY_QUAD,TY_QUAD,0,NULL);rt_leave();
}
static void gen_time_clock_runtime(void)
{
    int mono=rt_label("time.clock.mono"),cpu=rt_label("time.clock.cpu"),frequency=rt_label("time.clock.frequency"),normal=rt_label("time.clock.normal");
    rt_enter(time_clock_label,10);rt_save(1,RDI);
#ifdef QYNO_TEST_CLOCK_STEP_NS
    {int real=rt_label("time.test.real"),tick=rt_label("time.test.tick"),as_quad=rt_label("time.test.quad");
     rt_cmp(RDI,1);x64_jcc(CC_Z,tick);rt_cmp(RDI,3);x64_jcc(CC_Z,tick);rt_cmp(RDI,4);x64_jcc(CC_NZ,real);
     x64_mov_imm(RAX,1000000000);rt_leave();
     x64_bind(tick);x64_load(RAX,R14,344);x64_mov(RCX,RAX);x64_add_imm(RCX,QYNO_TEST_CLOCK_STEP_NS);x64_store(R14,344,RCX);
     rt_cmp(RDI,3);x64_jcc(CC_NZ,as_quad);gen_wide(W_CONVERT,TY_I128,TY_I64,0,NULL);rt_leave();
     x64_bind(as_quad);gen_wide(W_CONVERT,TY_QUAD,TY_I64,0,NULL);time_quad_integer(1000000000,RDI);gen_wide(W_DIV,TY_QUAD,TY_QUAD,0,NULL);rt_leave();x64_bind(real);}
#endif
    rt_cmp(RDI,4);x64_jcc(CC_Z,frequency);
    if(target==TARGET_LINUX){
        int supported=rt_label("time.clock.supported"),retry=rt_label("time.clock.retry");
        rt_cmp(RDI,3);x64_jcc(CC_NZ,supported);x64_mov_imm(RDI,1);x64_bind(supported);
        rt_save(2,RDI);x64_bind(retry);rt_load(RDI,2);x64_lea(RSI,RBP,-64);x64_mov_imm(RAX,228);x64_syscall();
        rt_cmp(RAX,-4);x64_jcc(CC_Z,retry);x64_test(RAX,RAX);x64_jcc(CC_NS,normal);time_os_failure("時計を取得できません");x64_bind(normal);
        x64_load(RAX,RBP,-64);x64_load(RDI,RBP,-56);rt_load(RCX,1);rt_cmp(RCX,3);x64_jcc(CC_Z,mono);
        x64_call(time_timespec_label);rt_leave();
        x64_bind(mono);rt_save(3,RDI);gen_wide(W_CONVERT,TY_I128,TY_I64,0,NULL);time_i128_integer(1000000000,RDI);gen_wide(W_MUL,TY_I128,TY_I128,0,NULL);rt_save(4,RAX);
        rt_load(RAX,3);gen_wide(W_CONVERT,TY_I128,TY_I64,0,NULL);rt_load(RDI,4);gen_wide(W_ADD,TY_I128,TY_I128,0,NULL);rt_leave();
        x64_bind(frequency);x64_mov_imm(RAX,1000000000);rt_leave();
    }else{
        rt_cmp(RDI,0);x64_jcc(CC_Z,normal);rt_cmp(RDI,2);x64_jcc(CC_Z,cpu);x64_jmp(mono);
        x64_bind(normal);x64_lea(RCX,RBP,-16);file_win_begin();file_win_end(IMP_GET_SYSTEM_TIME_PRECISE_AS_FILE_TIME);
        rt_load(RAX,2);x64_call(time_filetime_label);rt_leave();
        x64_bind(mono);x64_lea(RCX,RBP,-16);file_win_begin();file_win_end(IMP_QUERY_PERFORMANCE_COUNTER);
        {int good=rt_label("time.qpc.ok");x64_test(RAX,RAX);x64_jcc(CC_NZ,good);file_windows_error();time_os_failure("単調時計を取得できません");x64_bind(good);}
        rt_load(RCX,1);rt_cmp(RCX,3);
        {int as_quad=rt_label("time.qpc.quad");x64_jcc(CC_NZ,as_quad);rt_load(RAX,2);gen_wide(W_CONVERT,TY_I128,TY_I64,0,NULL);rt_leave();x64_bind(as_quad);}
        rt_load(RAX,2);gen_wide(W_CONVERT,TY_QUAD,TY_I64,0,NULL);rt_save(3,RAX);
        x64_mov_imm(RDI,4);x64_call(time_clock_label);io_rt_check();gen_wide(W_CONVERT,TY_QUAD,TY_I64,0,NULL);x64_mov(RDI,RAX);rt_load(RAX,3);gen_wide(W_DIV,TY_QUAD,TY_QUAD,0,NULL);rt_leave();
        x64_bind(frequency);x64_lea(RCX,RBP,-16);file_win_begin();file_win_end(IMP_QUERY_PERFORMANCE_FREQUENCY);
        {int good=rt_label("time.qpf.ok");x64_test(RAX,RAX);x64_jcc(CC_NZ,good);file_windows_error();time_os_failure("時計の周波数を取得できません");x64_bind(good);}
        rt_load(RAX,2);rt_leave();
        x64_bind(cpu);file_win_begin();file_win_end(IMP_GET_CURRENT_PROCESS);x64_mov(RCX,RAX);
        x64_lea(RDX,RBP,-32);x64_lea(R8,RBP,-40);x64_lea(R9,RBP,-48);x64_lea(R10,RBP,-56);
        file_win_begin();x64_store(RSP,32,R10);file_win_end(IMP_GET_PROCESS_TIMES);
        {int good=rt_label("time.cpu.ok");x64_test(RAX,RAX);x64_jcc(CC_NZ,good);file_windows_error();time_os_failure("CPU時間を取得できません");x64_bind(good);}
        rt_load(RAX,6);gen_wide(W_CONVERT,TY_QUAD,TY_U64,0,NULL);rt_save(3,RAX);
        rt_load(RAX,7);gen_wide(W_CONVERT,TY_QUAD,TY_U64,0,NULL);rt_load(RDI,3);gen_wide(W_ADD,TY_QUAD,TY_QUAD,0,NULL);
        time_quad_integer(10000000,RDI);gen_wide(W_DIV,TY_QUAD,TY_QUAD,0,NULL);rt_leave();
    }
}
/* TimeRequest is kept below GC-rooted locals; all ABI offsets come from C. */
#define TQ (-336)
#define TW (-512)
static void time_request_init(int op)
{
    size_t i;
    for(i=0;i<sizeof(TimeRequest);i+=8)x64_store_imm(RBP,TQ+(int)i,0);
    x64_store_imm(RBP,TQ+(int)offsetof(TimeRequest,op),op);
}
static void time_engine_call(void)
{
    int i;
    for(i=0;i<(int)sizeof(WideRequest);i+=8)x64_store_imm(RBP,TW+i,0);
    x64_store_imm(RBP,TW+(int)offsetof(WideRequest,op),W_TIME);x64_lea(RAX,RBP,TQ);x64_store(RBP,TW+(int)offsetof(WideRequest,input),RAX);
    x64_and_imm(RSP,-16);x64_lea(RDI,RBP,TW);x64_call(wide_engine_label);x64_lea(RSP,RBP,-640);
}
static void gen_time_sleep_runtime(void)
{
    int absolute=rt_label("time.sleep.absolute"),loop=rt_label("time.sleep.loop"),done=rt_label("time.sleep.done");
    rt_enter(time_sleep_label,80);rt_save(1,RAX);x64_test(RDI,RDI);x64_jcc(CC_NZ,absolute);
    /* sleep(0) neither waits nor reads an OS clock. */
    x64_test(RAX,RAX);x64_jcc(CC_Z,done);
    x64_load(RCX,RAX,0);x64_load(RDX,RAX,8);x64_shl_imm(RDX,1);x64_or(RCX,RDX);x64_test(RCX,RCX);x64_jcc(CC_Z,done);
    x64_mov_imm(RDI,1);x64_call(time_clock_label);io_rt_check();rt_load(RDI,1);gen_wide(W_ADD,TY_QUAD,TY_QUAD,0,NULL);rt_save(1,RAX);
    x64_bind(absolute);x64_bind(loop);x64_mov_imm(RDI,1);x64_call(time_clock_label);io_rt_check();x64_mov(RDI,RAX);rt_load(RAX,1);gen_wide(W_SUB,TY_QUAD,TY_QUAD,0,NULL);rt_save(2,RAX);
    time_quad_integer(0,RDI);gen_wide(W_CMP,TY_QUAD,TY_QUAD,CMP_LE,NULL);x64_test(RAX,RAX);x64_jcc(CC_NZ,done);
    time_request_init(T_WAIT_PARTS);rt_load(RAX,2);x64_load(RCX,RAX,0);x64_store(RBP,TQ+(int)offsetof(TimeRequest,args),RCX);x64_load(RCX,RAX,8);x64_store(RBP,TQ+(int)offsetof(TimeRequest,args)+8,RCX);time_engine_call();
    {int valid=rt_label("time.wait.valid");x64_load(RAX,RBP,TQ+(int)offsetof(TimeRequest,status));x64_test(RAX,RAX);x64_jcc(CC_Z,valid);file_fail(EX_SOFTWARE,"待機時間がint_64の範囲外です");x64_bind(valid);}
    if(target==TARGET_LINUX){
        x64_lea(RDI,RBP,TQ+(int)offsetof(TimeRequest,out));x64_xor(RSI,RSI);x64_mov_imm(RAX,35);x64_syscall();rt_cmp(RAX,-4);x64_jcc(CC_Z,loop);
        x64_test(RAX,RAX);x64_jcc(CC_NS,loop);time_os_failure("待機に失敗しました");
    }else{
        x64_load(RCX,RBP,TQ+(int)offsetof(TimeRequest,used));file_win_begin();file_win_end(IMP_SLEEP);x64_jmp(loop);
    }
    x64_bind(done);x64_xor(RAX,RAX);rt_leave();
}
static void gen_time_pure_entry(int op)
{
    const TimeSpec *spec=time_spec(op);int i;char code;
    rt_enter(time_labels[op],80);time_request_init(op);
    for(i=0;i<spec->max_args;i++){
        code=spec->args[i];x64_load(RAX,RBP,16+(spec->max_args-1-i)*8);
        if(code=='q'||code=='i'){
            if(code=='i')gen_wide(W_CONVERT,TY_QUAD,TY_I64,0,NULL);
            {int nonzero=rt_label("time.numeric.nonzero");x64_test(RAX,RAX);x64_jcc(CC_Z,nonzero);
             x64_load(RCX,RAX,0);x64_store(RBP,TQ+(int)offsetof(TimeRequest,args)+i*16,RCX);x64_load(RCX,RAX,8);x64_store(RBP,TQ+(int)offsetof(TimeRequest,args)+i*16+8,RCX);x64_bind(nonzero);}
        }else if(code=='s'){
            int base=op==T_FORMAT?(int)offsetof(TimeRequest,pattern):(int)offsetof(TimeRequest,text);
            x64_load(RCX,RAX,0);x64_store(RBP,TQ+base+8,RCX);x64_add_imm(RAX,8);x64_store(RBP,TQ+base,RAX);
        }else if(code=='d'){
            int j;x64_load(RDX,RAX,24);
            for(j=0;j<7;j++){
                x64_load(RCX,RDX,j*8);
                if(j==5){int zero=rt_label("time.second.zero");x64_test(RCX,RCX);x64_jcc(CC_Z,zero);x64_load(R8,RCX,8);x64_load(RCX,RCX,0);x64_store(RBP,TQ+(int)offsetof(TimeRequest,date)+48,R8);x64_bind(zero);}
                x64_store(RBP,TQ+(int)offsetof(TimeRequest,date)+(j==6?56:j*8),RCX);
            }
        }
    }
    /* A bounded expansion for date format tokens; raw strings remain GC rooted. */
    x64_load(RAX,RBP,TQ+(int)offsetof(TimeRequest,length));x64_load(RCX,RBP,TQ+(int)offsetof(TimeRequest,pattern_length));x64_add(RAX,RCX);
    {int capacity_ok=rt_label("time.capacity.ok");x64_mov_imm(RCX,(INT64_MAX-264)/8);x64_cmp(RAX,RCX);x64_jcc(CC_BE,capacity_ok);file_fail(EX_SOFTWARE,"timeの文字列が大きすぎます");x64_bind(capacity_ok);}
    x64_shl_imm(RAX,3);x64_add_imm(RAX,256);x64_store(RBP,TQ+(int)offsetof(TimeRequest,capacity),RAX);x64_mov(RDI,RAX);x64_add_imm(RDI,8);x64_call(alloc_label);rt_save(4,RAX);x64_add_imm(RAX,8);x64_store(RBP,TQ+(int)offsetof(TimeRequest,buffer),RAX);
    time_engine_call();
    {int valid=rt_label("time.argument.valid");x64_load(RAX,RBP,TQ+(int)offsetof(TimeRequest,status));x64_test(RAX,RAX);x64_jcc(CC_Z,valid);
     if(op==T_TRY_PARSE){x64_xor(RAX,RAX);rt_leave();}
     else {file_fail(op==T_PARSE?EX_DATAERR:EX_SOFTWARE,"timeの引数・日時・形式・範囲が不正です");}
     x64_bind(valid);}
    if(op==T_SLEEP||op==T_SLEEP_UNTIL){x64_load(RAX,RBP,16);x64_mov_imm(RDI,op==T_SLEEP_UNTIL);x64_call(time_sleep_label);io_rt_check();rt_leave();return;}
    if(op==T_ELAPSED){x64_mov_imm(RDI,1);x64_call(time_clock_label);io_rt_check();x64_load(RDI,RBP,16);gen_wide(W_SUB,TY_QUAD,TY_QUAD,0,NULL);rt_leave();return;}
    if(spec->result==TR_DATE){
        x64_mov_imm(RDI,16);x64_call(alloc_label);x64_load(RCX,RBP,TQ+(int)offsetof(TimeRequest,date)+40);x64_store(RAX,0,RCX);x64_load(RCX,RBP,TQ+(int)offsetof(TimeRequest,date)+48);x64_store(RAX,8,RCX);rt_save(5,RAX);
        x64_mov_imm(RDI,40);x64_call(alloc_label);x64_store_imm(RAX,0,7);x64_store_imm(RAX,8,7);rt_save(6,RAX);
        x64_mov_imm(RDI,56);x64_call(alloc_label);rt_load(RCX,6);x64_store(RCX,24,RAX);
        for(i=0;i<7;i++){if(i==5)rt_load(RDX,5);else x64_load(RDX,RBP,TQ+(int)offsetof(TimeRequest,date)+(i==6?56:i*8));x64_store(RAX,i*8,RDX);}
        rt_load(RAX,6);rt_leave();return;
    }
    if(spec->result==TR_TEXT){rt_load(RAX,4);x64_load(RCX,RBP,TQ+(int)offsetof(TimeRequest,used));x64_store(RAX,0,RCX);rt_leave();return;}
    if(spec->result==TR_QUAD||op==T_TRY_PARSE){
        x64_mov_imm(RDI,16);x64_call(alloc_label);x64_load(RCX,RBP,TQ+(int)offsetof(TimeRequest,out));x64_store(RAX,0,RCX);x64_load(RCX,RBP,TQ+(int)offsetof(TimeRequest,out)+8);x64_store(RAX,8,RCX);
        if(op==T_TRY_PARSE){x64_load(RCX,RBP,16);x64_store(RCX,0,RAX);x64_mov_imm(RAX,1);}rt_leave();return;
    }
    x64_load(RAX,RBP,TQ+(int)offsetof(TimeRequest,out));rt_leave();
}
static void gen_time_runtime(void)
{
    int op;
    gen_time_numeric_runtime();gen_time_clock_runtime();gen_time_sleep_runtime();
    for(op=0;op<T_COUNT;op++){
        if(op<=T_CPU||op==T_TICKS||op==T_FREQUENCY){
            rt_enter(time_labels[op],0);x64_mov_imm(RDI,op<=T_CPU?op:op==T_TICKS?3:4);x64_call(time_clock_label);io_rt_check();rt_leave();
        }else gen_time_pure_entry(op);
    }
}
#undef TQ
#undef TW
