/* Bridges keep the compiler's mutable heap registers in shared managed state.
 * The C engine always follows SysV, including inside a generated PE image. */
#define ASQ (-128)
#define TQ (-336)
typedef char AsyncRequestFitsBridge[(sizeof(AsyncRequest)<=88)?1:-1];
typedef char AsyncTimeRequestFitsBridge[(TQ+(int)sizeof(TimeRequest)<=ASQ)?1:-1];
static void async_store_heap(void)
{ x64_store(R14,ASYNC_STATE_HEAP,R12);x64_store(R14,ASYNC_STATE_COUNT,R13); }
static void async_load_heap(void)
{ x64_load(R12,R14,ASYNC_STATE_HEAP);x64_load(R13,R14,ASYNC_STATE_COUNT); }
static void async_request(int op)
{
    int i;
    for(i=0;i<(int)sizeof(AsyncRequest);i+=8)x64_store_imm(RBP,ASQ+i,0);
    x64_store_imm(RBP,ASQ+(int)offsetof(AsyncRequest,op),op);
    x64_load(RCX,R14,ASYNC_STATE_RUNTIME);x64_store(RBP,ASQ+(int)offsetof(AsyncRequest,runtime),RCX);
}
static void async_engine(void)
{
    async_store_heap();x64_lea(RDI,RBP,ASQ);x64_and_imm(RSP,-16);x64_call(async_engine_label);
    async_load_heap();
}
static void async_c_enter(int label)
{
    rt_enter(label,80);
    rt_save(1,RBX);rt_save(2,R12);rt_save(3,R13);rt_save(4,R14);rt_save(5,R15);
    rt_save(6,RDI);rt_save(7,RSI);rt_save(8,RDX);rt_save(9,RCX);rt_save(10,R8);
    x64_load(R14,RDI,(int)offsetof(AsyncRuntime,state));async_load_heap();
    x64_load(RCX,RDI,(int)offsetof(AsyncRuntime,current));
    {int native=rt_label("async.native.top"),ready=rt_label("async.stack.top");
     x64_test(RCX,RCX);x64_jcc(CC_Z,native);x64_load(R15,RCX,(int)offsetof(AsyncTask,stack_top));
     x64_jmp(ready);x64_bind(native);x64_load(R15,RDI,(int)offsetof(AsyncRuntime,initial_top));x64_bind(ready);}
}
static void async_c_leave(void)
{
    async_store_heap();
    rt_load(RBX,1);rt_load(R12,2);rt_load(R13,3);rt_load(R14,4);rt_load(R15,5);rt_leave();
}
static void gen_async_init(void)
{
    int i;
    x64_mov_imm(RDI,sizeof(AsyncRuntime));x64_call(alloc_label);x64_store(R14,ASYNC_STATE_RUNTIME,RAX);
    x64_store(RAX,(int)offsetof(AsyncRuntime,state),R14);x64_store(RAX,(int)offsetof(AsyncRuntime,initial_top),R15);
    {int offsets[]={(int)offsetof(AsyncRuntime,allocate),(int)offsetof(AsyncRuntime,clock),(int)offsetof(AsyncRuntime,sleep)};
     int labels[]={async_alloc_label,async_clock_label,async_sleep_label};
     for(i=0;i<3;i++){x64_lea_label(RCX,labels[i]);x64_store(RAX,offsets[i],RCX);}}
    x64_lea_data(RCX,empty_str_offset);x64_store(RAX,(int)offsetof(AsyncRuntime,empty_string),RCX);
    {const char *message=QMSG_0879;
     x64_lea_data(RCX,prog_add_str(prog,message,strlen(message)));x64_store(RAX,(int)offsetof(AsyncRuntime,invalid_message),RCX);}
    {int offsets[]={(int)offsetof(AsyncRuntime,cancel_message),(int)offsetof(AsyncRuntime,group_message),(int)offsetof(AsyncRuntime,timeout_message),(int)offsetof(AsyncRuntime,io_domain),(int)offsetof(AsyncRuntime,task_domain)};
     const char *messages[]={QMSG_0890,QMSG_0891,QMSG_0892,"io","task"};
     for(i=0;i<5;i++){x64_lea_data(RCX,prog_add_str(prog,messages[i],strlen(messages[i])));x64_store(RAX,offsets[i],RCX);}}
    collection_shape(array_type(standard_type(emitting_ast,"lib","Error")),NULL,RCX);x64_store(RAX,(int)offsetof(AsyncRuntime,error_shape),RCX);
    x64_mov_imm(RCX,target==TARGET_WINDOWS);x64_store(RAX,(int)offsetof(AsyncRuntime,io_windows),RCX);
    x64_lea_label(RCX,async_file_error_label);x64_store(RAX,(int)offsetof(AsyncRuntime,file_error),RCX);
    x64_lea_label(RCX,async_file_fatal_label);x64_store(RAX,(int)offsetof(AsyncRuntime,file_fatal),RCX);
    if(target==TARGET_WINDOWS&&file_async_enabled){
        Import imports[]={IMP_CREATE_THREAD,IMP_CREATE_EVENT_W,IMP_SET_EVENT,IMP_WAIT_FOR_SINGLE_OBJECT,IMP_CLOSE_HANDLE,
            IMP_VIRTUAL_ALLOC,IMP_VIRTUAL_FREE,IMP_GET_LAST_ERROR,IMP_CREATE_FILE_W,IMP_READ_FILE,IMP_WRITE_FILE,IMP_GET_FILE_TYPE,IMP_GET_FILE_INFORMATION_BY_HANDLE,IMP_GET_FULL_PATH_NAME_W};
        for(i=0;i<FA_IMPORT_COUNT;i++){x64_load_import(RCX,imports[i]);x64_store(RAX,(int)offsetof(AsyncRuntime,io_imports)+i*8,RCX);}
    }
    async_store_heap();
}
static void gen_async_call(Function *fn,int nargs)
{
    gen_os_context(fn->tok,"task","function");
    x64_lea_label(RAX,fn->async_label);x64_mov_imm(RDI,nargs);x64_mov(RSI,RSP);x64_call(async_create_label);
}
static void gen_async_await(Node *node)
{
    gen_async_checkpoint(node->tok);
    gen_expr(node->lhs);gen_os_context(node->tok,"task","await");x64_call(async_await_label);gen_io_check();
    if(type_is_model(node->ty)){x64_call(type_model(node->ty)->copy_label);gen_copy_check(node->ty);}
    else if(type_is_array(node->ty)){
        collection_shape(node->ty,NULL,RDI);x64_xor(RSI,RSI);x64_call(array_runtime_label(node->ty,AR_COPY));gen_copy_check(node->ty);
    }
}
static void gen_async_builtin(Node *node)
{
    if(builtin_is_file_async((int)node->val)){
        Node *a;
        for(a=node->lhs;a;a=a->next){gen_expr(a);push();}
        gen_os_context(node->tok,"io.file",builtin_name((Builtin)node->val));
        x64_load(RAX,RSP,(node->nargs-1)*8);x64_call(file_validate_label);
        {int loop=rt_label("async.file.path.nul"),valid=rt_label("async.file.path.valid"),next=rt_label("async.file.path.next");
         x64_load(RCX,RAX,0);x64_lea(RSI,RAX,8);x64_bind(loop);x64_test(RCX,RCX);x64_jcc(CC_Z,valid);
         x64_load8(R8,RSI,0);x64_test(R8,R8);x64_jcc(CC_NZ,next);file_fail(EX_SOFTWARE,QMSG_0406);
         x64_bind(next);x64_add_imm(RSI,1);x64_sub_imm(RCX,1);x64_jmp(loop);x64_bind(valid);}

        if(node->val>=BI_FILE_READ_BYTES_ASYNC&&node->val<=BI_FILE_APPEND_BYTES_ASYNC){
            x64_load(RAX,RSP,0);
            if(node->val!=BI_FILE_READ_BYTES_ASYNC)x64_load(RAX,RAX,0);
            bulk_contract_limit(RAX,(INT64_MAX-32)/8);x64_xor(RCX,RCX);
        }
        else if(node->val==BI_FILE_WRITE_LINES_ASYNC){x64_mov(RAX,RSP);x64_call(async_file_lines_label);x64_mov(RCX,RAX);}
        else{
            x64_load(RAX,RSP,0);
            if(node->val==BI_FILE_WRITE_TEXT_ASYNC||node->val==BI_FILE_APPEND_TEXT_ASYNC)x64_call(file_validate_label);
            else{
                if(node->val==BI_FILE_READ_LINES_ASYNC){bulk_contract_limit(RAX,(INT64_MAX-32)/8);x64_load(RAX,RSP,8);}
                bulk_contract_limit(RAX,INT64_MAX-32);
            }
            x64_xor(RCX,RCX);
        }
        if(node->val==BI_FILE_READ_LINES_ASYNC||node->val==BI_FILE_READ_BYTES_ASYNC)collection_shape(task_result(node->ty),NULL,RDX);else x64_xor(RDX,RDX);
        x64_mov(RAX,RSP);x64_mov_imm(RDI,node->val-BI_FILE_READ_TEXT_ASYNC);x64_mov_imm(RSI,node->nargs);x64_call(async_file_label);
        x64_add_imm(RSP,node->nargs*8);depth-=node->nargs;gen_io_check();return;
    }
    if(node->val==BI_TASK_ALL){
        Node *a;
        for(a=node->lhs;a;a=a->next){gen_expr(a);push();}
        gen_os_context(node->tok,"task","all");
        collection_shape(task_result(node->ty),NULL,RSI);
        if(node->random_flags)x64_load(RAX,RSP,0);else x64_mov(RAX,RSP);
        x64_mov_imm(RDI,node->nargs);x64_mov_imm(RDX,node->random_flags);x64_call(async_all_label);
        x64_add_imm(RSP,node->nargs*8);depth-=node->nargs;gen_io_check();return;
    }
    if(node->val==BI_TASK_TIMEOUT){
        gen_expr(node->lhs);push();gen_expr(node->lhs->next);x64_mov(RDI,RAX);pop(RAX);
        gen_os_context(node->tok,"task","with_timeout");x64_call(async_timeout_label);gen_io_check();return;
    }
    if(node->lhs)gen_expr(node->lhs);
    gen_os_context(node->tok,"task",node->val==BI_TASK_YIELD?"yield":"sleep_async");
    x64_call(node->val==BI_TASK_YIELD?async_yield_label:async_delay_label);gen_io_check();
}
static int async_node_slots(Node *n,int budget)
{
    int slots=0;
    for(;n&&slots<budget;n=n->next){
        slots+=4;
        slots+=async_node_slots(n->lhs,budget-slots);slots+=async_node_slots(n->rhs,budget-slots);
        slots+=async_node_slots(n->cond,budget-slots);slots+=async_node_slots(n->then,budget-slots);
        slots+=async_node_slots(n->els,budget-slots);slots+=async_node_slots(n->init,budget-slots);
        slots+=async_node_slots(n->step,budget-slots);slots+=async_node_slots(n->end,budget-slots);
    }
    return slots;
}
static void gen_async_stack_check(Function *fn)
{
    int native=rt_label("async.stack.checked");
    int reserve=fn->stack_size+131072+async_node_slots(fn->body,ASYNC_STACK_BYTES/8)*8;
    x64_load(R10,R14,ASYNC_STATE_RUNTIME);x64_test(R10,R10);x64_jcc(CC_Z,native);
    x64_load(R10,R10,(int)offsetof(AsyncRuntime,current));x64_test(R10,R10);x64_jcc(CC_Z,native);
    x64_load(R10,R10,(int)offsetof(AsyncTask,stack));x64_mov_imm(R11,reserve);x64_add(R10,R11);x64_cmp(RSP,R10);x64_jcc(CC_AE,native);
    gen_fail(fn->tok,QMSG_0880,EX_SOFTWARE);
    x64_bind(native);
}
static void gen_async_checkpoint(Token *tok)
{gen_os_context(tok,"task","checkpoint");x64_call(async_check_label);gen_io_check();}
static void gen_async_group_close(Node *node)
{gen_os_context(node->tok,"task","group");x64_load(RAX,RBP,-node->var->offset);x64_call(async_group_close_label);gen_io_check();}
static void gen_async_group(Node *node)
{
    int failed=rt_label("async.group.failure"),done=rt_label("async.group.done");
    IoScope *outer=io_scope,body={outer,node,failed,depth,loop_depth};
    gen_os_context(node->tok,"task","group");x64_call(async_group_enter_label);x64_store(RBP,-node->var->offset,RAX);
    io_scope=&body;gen_stmts(node->then);io_scope=outer;gen_async_group_close(node);x64_jmp(done);
    x64_bind(failed);gen_async_group_close(node);x64_bind(done);
}
#include "../file-io/file_async_codegen.inc"

static void gen_async_runtime(Ast *ast)
{
    Function *fn;int i;
    rt_enter(async_create_label,80);rt_save(1,RAX);rt_save(2,RDI);rt_save(3,RSI);
    async_request(ASYNC_CREATE);rt_load(RAX,1);x64_store(RBP,ASQ+(int)offsetof(AsyncRequest,invoke),RAX);
    rt_load(RAX,2);x64_store(RBP,ASQ+(int)offsetof(AsyncRequest,nargs),RAX);
    rt_load(RAX,3);x64_store(RBP,ASQ+(int)offsetof(AsyncRequest,args),RAX);async_engine();rt_leave();
    {int ops[]={ASYNC_AWAIT,ASYNC_RUN,ASYNC_YIELD};int labels[]={async_await_label,async_run_label,async_yield_label};
     for(i=0;i<3;i++){
         rt_enter(labels[i],80);rt_save(1,RAX);async_request(ops[i]);
         if(i<2){rt_load(RAX,1);x64_store(RBP,ASQ+(int)offsetof(AsyncRequest,task),RAX);}
         async_engine();rt_leave();
     }}
    {int ops[]={ASYNC_GROUP_ENTER,ASYNC_GROUP_CLOSE,ASYNC_CANCEL,ASYNC_GROUP_CANCEL,ASYNC_CHECK,ASYNC_ERROR_LINK};
     int labels[]={async_group_enter_label,async_group_close_label,async_cancel_label,async_group_cancel_label,async_check_label,async_link_label};
     for(i=0;i<6;i++){
         rt_enter(labels[i],80);rt_save(1,RAX);rt_save(2,RDI);async_request(ops[i]);rt_load(RAX,1);
         x64_store(RBP,ASQ+(int)offsetof(AsyncRequest,task),RAX);x64_store(RBP,ASQ+(int)offsetof(AsyncRequest,group),RAX);
         rt_load(RAX,2);x64_store(RBP,ASQ+(int)offsetof(AsyncRequest,args),RAX);async_engine();
         if(ops[i]==ASYNC_CHECK)rt_load(RAX,1);
         rt_leave();
     }}
    rt_enter(async_file_label,80);rt_save(1,RAX);rt_save(2,RDI);rt_save(3,RSI);rt_save(4,RDX);rt_save(5,RCX);async_request(ASYNC_FILE);
    rt_load(RAX,1);x64_store(RBP,ASQ+(int)offsetof(AsyncRequest,args),RAX);
    rt_load(RAX,2);x64_store(RBP,ASQ+(int)offsetof(AsyncRequest,flags),RAX);
    rt_load(RAX,3);x64_store(RBP,ASQ+(int)offsetof(AsyncRequest,nargs),RAX);
    rt_load(RAX,4);x64_store(RBP,ASQ+(int)offsetof(AsyncRequest,shape),RAX);
    rt_load(RAX,5);x64_store(RBP,ASQ+(int)offsetof(AsyncRequest,delay),RAX);async_engine();rt_leave();
    {int done=rt_label("async.shutdown.done");rt_enter(async_shutdown_label,80);rt_save(1,RAX);
     x64_load(RCX,R14,ASYNC_STATE_RUNTIME);x64_test(RCX,RCX);x64_jcc(CC_Z,done);
     async_request(ASYNC_SHUTDOWN);async_engine();x64_bind(done);rt_load(RAX,1);rt_leave();}
    rt_enter(async_all_label,80);rt_save(1,RAX);rt_save(2,RDI);rt_save(3,RSI);rt_save(4,RDX);
    async_request(ASYNC_ALL);rt_load(RAX,1);x64_store(RBP,ASQ+(int)offsetof(AsyncRequest,args),RAX);
    rt_load(RAX,2);x64_store(RBP,ASQ+(int)offsetof(AsyncRequest,nargs),RAX);
    rt_load(RAX,3);x64_store(RBP,ASQ+(int)offsetof(AsyncRequest,shape),RAX);
    rt_load(RAX,4);x64_store(RBP,ASQ+(int)offsetof(AsyncRequest,flags),RAX);async_engine();rt_leave();
    /* Validate in binary128 and round only the OS wait interval up to ns. */
    {int mode;
    for(mode=0;mode<2;mode++){
    rt_enter(mode?async_timeout_label:async_delay_label,80);rt_save(1,RAX);rt_save(2,RDI);
    if(mode)x64_mov(RAX,RDI);
    time_request_init(T_SLEEP);
    x64_load(RCX,RAX,0);x64_store(RBP,TQ+(int)offsetof(TimeRequest,args),RCX);
    x64_load(RCX,RAX,8);x64_store(RBP,TQ+(int)offsetof(TimeRequest,args)+8,RCX);time_engine_call();
    {int valid=rt_label("async.delay.valid");x64_load(RAX,RBP,TQ+(int)offsetof(TimeRequest,status));x64_test(RAX,RAX);x64_jcc(CC_Z,valid);file_fail(EX_SOFTWARE,QMSG_0333);x64_bind(valid);}
    x64_store_imm(RBP,TQ+(int)offsetof(TimeRequest,op),T_ASYNC_PARTS);time_engine_call();
    async_request(mode?ASYNC_TIMEOUT:ASYNC_DELAY);
    if(mode){rt_load(RAX,1);x64_store(RBP,ASQ+(int)offsetof(AsyncRequest,task),RAX);}
    for(i=0;i<2;i++){x64_load(RAX,RBP,TQ+(int)offsetof(TimeRequest,out)+i*8);x64_store(RBP,ASQ+(int)offsetof(AsyncRequest,delay)+i*8,RAX);}
    async_engine();rt_leave();
    }}

    async_c_enter(async_alloc_label);rt_load(RDI,7);x64_call(alloc_label);async_c_leave();
    async_c_enter(async_clock_label);x64_mov_imm(RDI,1);x64_call(time_clock_label);
    {int error=rt_label("async.clock.error");x64_load(R11,R14,IO_PENDING);x64_test(R11,R11);x64_jcc(CC_NZ,error);
     time_request_init(T_ASYNC_PARTS);x64_load(RCX,RAX,0);x64_store(RBP,TQ+(int)offsetof(TimeRequest,args),RCX);
     x64_load(RCX,RAX,8);x64_store(RBP,TQ+(int)offsetof(TimeRequest,args)+8,RCX);time_engine_call();
     rt_load(RDX,7);for(i=0;i<2;i++){x64_load(RCX,RBP,TQ+(int)offsetof(TimeRequest,out)+i*8);x64_store(RDX,i*8,RCX);}
     x64_bind(error);async_c_leave();}
    async_c_enter(async_sleep_label);rt_load(RDX,7);x64_load(RAX,RDX,0);x64_load(RDI,RDX,8);
    x64_call(time_timespec_label);x64_xor(RDI,RDI);x64_call(time_sleep_label);async_c_leave();

    gen_file_async_callbacks();

    for(fn=ast->funcs;fn;fn=fn->next)if(fn->is_async&&!fn->is_template){
        async_c_enter(fn->async_label);
        for(i=0;i<fn->param_count;i++){
            rt_load(RCX,7);x64_load(RCX,RCX,(int)offsetof(AsyncTask,args));x64_load(RAX,RCX,i*8);x64_push(RAX);
        }
        x64_call(fn->label);if(fn->param_count)x64_add_imm(RSP,fn->param_count*8);async_c_leave();
    }
    x64_provide(async_engine_label);
    while(x64_output_enabled()&&prog->text.len%16){unsigned char nop=0x90;x64_blob(&nop,1);}
    x64_directive(".p2align 4");x64_blob(async_blob,ASYNC_ENTRY_OFFSET);x64_bind(async_engine_label);
    x64_blob(async_blob+ASYNC_ENTRY_OFFSET,sizeof(async_blob)-ASYNC_ENTRY_OFFSET);
}
#undef ASQ

#undef TQ
