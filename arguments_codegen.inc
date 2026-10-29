/* Startup constructs ordinary managed strings and an unbounded str array. */
static int arguments_copy_label;
static void gen_entry_main(void)
{
    if(main_function->param_count){
        collection_context(main_function->tok);gen_os_context(main_function->tok,"runtime","arguments");
        x64_call(arguments_label);x64_push(RAX);
    }
    x64_call(main_label);
    if(main_function->param_count)x64_add_imm(RSP,8);
}
#define ARGS_REQUEST (-208)
static void arguments_engine_call(void)
{
    int valid=rt_label("arguments.encoding.valid"),size=rt_label("arguments.size.error");
    x64_and_imm(RSP,-16);x64_lea(RDI,RBP,ARGS_REQUEST);x64_call(wide_engine_label);x64_lea(RSP,RBP,-256);
    x64_load(RAX,RBP,ARGS_REQUEST+(int)offsetof(WideRequest,status));x64_test(RAX,RAX);x64_jcc(CC_Z,valid);
    rt_cmp(RAX,65);x64_jcc(CC_NZ,size);file_fail(EX_DATAERR,"実行時引数が正しいUnicodeではありません");
    x64_bind(size);file_fail(EX_SOFTWARE,"実行時引数のサイズが範囲を超えています");x64_bind(valid);
}
static void gen_arguments_runtime(void)
{
    int setup=rt_label("arguments.setup"),loop=rt_label("arguments.copy.loop"),done=rt_label("arguments.done");
    arguments_copy_label=rt_label("arguments.copy.string");
    rt_enter(arguments_copy_label,3);rt_save(1,RAX);rt_save(2,RDI);
    x64_add_imm(RDI,8);x64_call(alloc_label);rt_save(3,RAX);
    rt_load(RCX,2);x64_store(RAX,0,RCX);x64_lea(RDI,RAX,8);rt_load(RSI,1);x64_rep_movsb();rt_load(RAX,3);
    x64_call(file_validate_label);rt_leave();

    rt_enter(arguments_label,32);
    if(target==TARGET_LINUX){
        int positive=rt_label("arguments.argc.positive");
        x64_load(RAX,R15,0);rt_cmp(RAX,1);x64_jcc(CC_A,positive);x64_mov_imm(RAX,1);x64_bind(positive);
        x64_sub_imm(RAX,1);rt_save(2,RAX);x64_lea(RAX,R15,16);rt_save(1,RAX);
    }else{
        int scan=rt_label("arguments.command.scan"),scanned=rt_label("arguments.command.scanned"),present=rt_label("arguments.command.present");
        int i;
        file_win_begin();file_win_end(IMP_GET_COMMAND_LINE_W);x64_test(RAX,RAX);x64_jcc(CC_NZ,present);
        file_fail(EX_SOFTWARE,"実行時コマンドラインを取得できません");x64_bind(present);
        rt_save(8,RAX);x64_xor(RCX,RCX);x64_bind(scan);x64_load8(RDX,RAX,0);x64_load8(R8,RAX,1);x64_shl_imm(R8,8);x64_or(RDX,R8);x64_test(RDX,RDX);x64_jcc(CC_Z,scanned);
        x64_add_imm(RAX,2);x64_add_imm(RCX,1);x64_jcc(CC_NO,scan);file_fail(EX_SOFTWARE,"実行時引数が長すぎます");
        x64_bind(scanned);
        for(i=0;i<(int)sizeof(WideRequest);i+=8)x64_store_imm(RBP,ARGS_REQUEST+i,0);
        x64_store_imm(RBP,ARGS_REQUEST+(int)offsetof(WideRequest,op),W_ARGUMENTS);
        rt_load(RAX,8);x64_store(RBP,ARGS_REQUEST+(int)offsetof(WideRequest,input),RAX);
        x64_store(RBP,ARGS_REQUEST+(int)offsetof(WideRequest,length),RCX);
        arguments_engine_call();x64_load(RAX,RBP,ARGS_REQUEST+(int)offsetof(WideRequest,out));rt_save(2,RAX);
        x64_load(RDI,RBP,ARGS_REQUEST+(int)offsetof(WideRequest,out)+8);rt_save(7,RDI);x64_call(alloc_label);rt_save(1,RAX);
        x64_store(RBP,ARGS_REQUEST+(int)offsetof(WideRequest,buffer),RAX);rt_load(RCX,7);x64_store(RBP,ARGS_REQUEST+(int)offsetof(WideRequest,base),RCX);
        arguments_engine_call();
    }
    rt_load(RAX,2);x64_mov_imm(RCX,(INT64_MAX-31)/8);x64_cmp(RAX,RCX);x64_jcc(CC_BE,setup);
    file_fail(EX_SOFTWARE,"実行時引数の個数が範囲を超えています");x64_bind(setup);
    rt_set(5,0);
    {int empty=rt_label("arguments.array.empty");x64_test(RAX,RAX);x64_jcc(CC_Z,empty);
     x64_mov(RDI,RAX);x64_shl_imm(RDI,3);x64_call(alloc_label);rt_save(5,RAX);x64_bind(empty);}
    x64_mov_imm(RDI,40);x64_call(alloc_label);rt_save(4,RAX);
    rt_load(RCX,2);x64_store(RAX,0,RCX);x64_store(RAX,8,RCX);
    collection_shape(array_type(TY_STR),NULL,RDI);x64_store(RAX,16,RDI);
    rt_load(RCX,5);x64_store(RAX,24,RCX);x64_store_imm(RAX,32,0);rt_set(3,0);
    x64_bind(loop);rt_load(RCX,3);rt_load(RAX,2);x64_cmp(RCX,RAX);x64_jcc(CC_AE,done);
    rt_load(RAX,1);
    if(target==TARGET_LINUX){
        int scan=rt_label("arguments.utf8.scan"),scanned=rt_label("arguments.utf8.scanned");
        x64_load(RAX,RAX,0);x64_mov(RDX,RAX);x64_xor(RCX,RCX);x64_bind(scan);
        x64_load8(R8,RDX,0);x64_test(R8,R8);x64_jcc(CC_Z,scanned);x64_add_imm(RDX,1);x64_add_imm(RCX,1);
        x64_jcc(CC_NO,scan);file_fail(EX_SOFTWARE,"実行時引数が長すぎます");x64_bind(scanned);rt_save(7,RCX);x64_mov(RDI,RCX);
    }else{x64_load(RDI,RAX,0);rt_save(7,RDI);x64_add_imm(RAX,8);}
    x64_call(arguments_copy_label);rt_load(RCX,3);x64_shl_imm(RCX,3);rt_load(RDX,5);x64_add(RDX,RCX);x64_store(RDX,0,RAX);
    rt_load(RAX,1);
    if(target==TARGET_WINDOWS){rt_load(RCX,7);x64_add_imm(RCX,7);x64_and_imm(RCX,-8);x64_add(RAX,RCX);}
    x64_add_imm(RAX,8);rt_save(1,RAX);rt_load(RCX,3);x64_add_imm(RCX,1);rt_save(3,RCX);x64_jmp(loop);
    x64_bind(done);rt_load(RAX,4);rt_leave();
}
#undef ARGS_REQUEST
