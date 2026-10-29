#define TQ (-256)
#define TW (-400)
#define TQOFF(field) (TQ+(int)offsetof(TextRequest,field))
static void gen_text_method(Node *node)
{
    Node args[4]={{0}},call=*node,*arg;Function fn={0};int i=1;
    args[0]=*node->lhs;
    for(arg=node->rhs;arg;arg=arg->next)args[i++]=*arg;
    while(i<3){args[i].kind=ND_STRLIT;args[i].ty=TY_STR;args[i].str="";args[i].tok=node->tok;i++;}
    args[3].kind=ND_NUM;args[3].ty=TY_I64;args[3].val=node->val;args[3].tok=node->tok;
    for(i=0;i<3;i++)args[i].next=&args[i+1];
    fn.label=text_method_label;call.func=&fn;call.lhs=args;call.nargs=4;gen_call(&call);
}
static void text_engine_call(void)
{
    int i,valid=rt_label("text.valid"),contract=rt_label("text.contract.error");
    for(i=0;i<(int)sizeof(WideRequest);i+=8)x64_store_imm(RBP,TW+i,0);
    x64_store_imm(RBP,TW+(int)offsetof(WideRequest,op),W_TEXT_METHOD);
    x64_lea(RAX,RBP,TQ);x64_store(RBP,TW+(int)offsetof(WideRequest,input),RAX);
    for(i=0;i<16;i++){x64_sub_imm(RSP,4096);x64_store_imm(RSP,0,0);}
    x64_add_imm(RSP,65536);x64_and_imm(RSP,-16);x64_lea(RDI,RBP,TW);x64_call(wide_engine_label);x64_lea(RSP,RBP,-512);
    x64_load(RAX,RBP,TQOFF(status));x64_test(RAX,RAX);x64_jcc(CC_Z,valid);
    rt_cmp(RAX,65);x64_jcc(CC_NZ,contract);
    x64_load(RAX,R14,COLLECTION_CONTEXT);x64_load(RDX,RAX,0);x64_lea(RSI,RAX,8);x64_call(write_err_label);
    {const char *msg="文字列メソッドに不正な UTF-8 です\n";x64_lea_data(RSI,prog_add_data(prog,msg,strlen(msg)));x64_mov_imm(RDX,(long long)strlen(msg));x64_mov_imm(R8,EX_DATAERR);x64_jmp(runtime_error_label);}
    x64_bind(contract);collection_fail("文字列メソッドの検索文字列・サイズが不正です\n");x64_bind(valid);
}
static void gen_text_method_runtime(void)
{
    int i,scalar=rt_label("text.scalar"),split=rt_label("text.split"),loop=rt_label("text.parts"),done=rt_label("text.parts.done");
    rt_enter(text_method_label,64);
    for(i=0;i<(int)sizeof(TextRequest);i+=8)x64_store_imm(RBP,TQ+i,0);
    for(i=0;i<3;i++){
        x64_load(RAX,RBP,40-i*8);x64_load(RCX,RAX,0);x64_store(RBP,TQOFF(length)+i*8,RCX);
        x64_add_imm(RAX,8);x64_store(RBP,TQOFF(text)+i*8,RAX);
    }
    x64_load(RAX,RBP,16);x64_store(RBP,TQOFF(op),RAX);text_engine_call();
    x64_load(RDI,RBP,TQOFF(scratch_size));x64_add_imm(RDI,8);x64_call(alloc_label);x64_store(RBP,TQOFF(scratch),RAX);
    x64_store_imm(RBP,TQOFF(phase),1);text_engine_call();
    x64_load(RAX,RBP,TQOFF(op));rt_cmp(RAX,TEXT_FIND);x64_jcc(CC_BE,scalar);
    x64_load(RDI,RBP,TQOFF(used));x64_store(RBP,TQOFF(capacity),RDI);x64_add_imm(RDI,8);x64_call(alloc_label);rt_save(1,RAX);
    x64_load(RCX,RBP,TQOFF(used));x64_store(RAX,0,RCX);x64_add_imm(RAX,8);x64_store(RBP,TQOFF(buffer),RAX);
    x64_store_imm(RBP,TQOFF(phase),2);text_engine_call();
    x64_load(RAX,RBP,TQOFF(op));rt_cmp(RAX,TEXT_SPLIT);x64_jcc(CC_Z,split);rt_load(RAX,1);rt_leave();
    x64_bind(scalar);x64_load(RAX,RBP,TQOFF(result));rt_leave();
    x64_bind(split);x64_load(RDI,RBP,TQOFF(count));x64_shl_imm(RDI,3);x64_call(alloc_label);rt_save(2,RAX);
    x64_mov_imm(RDI,40);x64_call(alloc_label);rt_save(3,RAX);
    x64_load(RCX,RBP,TQOFF(count));x64_store(RAX,0,RCX);x64_store(RAX,8,RCX);
    collection_shape(array_type(TY_STR),NULL,RDI);x64_store(RAX,16,RDI);rt_load(RCX,2);x64_store(RAX,24,RCX);x64_store_imm(RAX,32,0);
    rt_set(4,0);x64_bind(loop);rt_load(RCX,4);x64_load(RAX,RBP,TQOFF(count));x64_cmp(RCX,RAX);x64_jcc(CC_AE,done);
    x64_shl_imm(RCX,4);x64_load(RAX,RBP,TQOFF(buffer));x64_add(RAX,RCX);x64_load(RCX,RAX,0);rt_save(5,RCX);x64_load(RDI,RAX,8);rt_save(6,RDI);
    x64_add_imm(RDI,8);x64_call(alloc_label);rt_save(7,RAX);rt_load(RCX,6);x64_store(RAX,0,RCX);x64_lea(RDI,RAX,8);
    x64_load(RSI,RBP,TQOFF(text));rt_load(RDX,5);x64_add(RSI,RDX);x64_rep_movsb();
    rt_load(RAX,7);rt_load(RCX,4);x64_shl_imm(RCX,3);rt_load(RDX,2);x64_add(RDX,RCX);x64_store(RDX,0,RAX);
    rt_load(RCX,4);x64_add_imm(RCX,1);rt_save(4,RCX);x64_jmp(loop);
    x64_bind(done);rt_load(RAX,3);rt_leave();
}
#undef TQOFF
#undef TQ
#undef TW
