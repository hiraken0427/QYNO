#define RANDOM_STATE 352
#define RANDOM_INITIALIZED 384
#define RQ (-384)
#define RW (-544)
#define RQOFF(field) (RQ+(int)offsetof(RandomRequest,field))
static void gen_random_builtin(Node *node)
{
    Node args[6]={{0}},call=*node;Function fn={0};int i,op=random_operation((int)node->val);
    for(i=0;i<6;i++){args[i].kind=ND_NUM;args[i].ty=TY_I64;args[i].tok=node->tok;}
    if(node->rhs)args[0]=*node->rhs;
    args[1].val=op;args[2].val=(op==1||op==2)?wide_tag(node->ty):0;args[3].val=node->random_flags;
    if(node->lhs){args[4]=*node->lhs;if(node->lhs->next)args[5]=*node->lhs->next;}
    for(i=0;i<5;i++)args[i].next=&args[i+1];
    args[5].next=NULL;
    fn.label=random_call_label;call.func=&fn;call.lhs=args;call.nargs=6;gen_call(&call);
}
static void random_engine_call(void)
{
    int i,good=rt_label("random.request.ok"),argument=rt_label("random.argument.error");
    for(i=0;i<(int)sizeof(WideRequest);i+=8)x64_store_imm(RBP,RW+i,0);
    x64_store_imm(RBP,RW+(int)offsetof(WideRequest,op),W_RANDOM);x64_lea(RAX,RBP,RQ);x64_store(RBP,RW+(int)offsetof(WideRequest,input),RAX);
    for(i=0;i<16;i++){x64_sub_imm(RSP,4096);x64_store_imm(RSP,0,0);}
    x64_add_imm(RSP,65536);x64_and_imm(RSP,-16);x64_lea(RDI,RBP,RW);x64_call(wide_engine_label);x64_lea(RSP,RBP,-640);
    x64_load(RAX,RBP,RQOFF(status));x64_test(RAX,RAX);x64_jcc(CC_Z,good);rt_cmp(RAX,65);x64_jcc(CC_NZ,argument);
    file_fail(EX_DATAERR,"乱数の候補が不正なUTF-8です");x64_bind(argument);file_fail(EX_SOFTWARE,"乱数の範囲・候補・文字数が不正です");x64_bind(good);
}
static void random_entropy_failure(void)
{
    int unsupported=rt_label("random.entropy.unsupported"),raised=rt_label("random.entropy.raised");
    x64_store(R14,FILE_OS_ERROR,RAX);x64_mov_imm(RDI,14);
    rt_cmp(RAX,target==TARGET_LINUX?38:120);x64_jcc(CC_Z,unsupported);rt_cmp(RAX,target==TARGET_LINUX?95:50);x64_jcc(CC_NZ,raised);
    x64_bind(unsupported);x64_mov_imm(RDI,12);x64_bind(raised);
    x64_lea_data(RAX,prog_add_str(prog,"OSから乱数シードを取得できません",strlen("OSから乱数シードを取得できません")));
    x64_call(io_raise_label);rt_leave();
}
static void random_entropy(void)
{
    if(target==TARGET_LINUX){
        int loop=rt_label("random.entropy.read"),positive=rt_label("random.entropy.positive"),nonzero=rt_label("random.entropy.nonzero");
        x64_lea(RAX,RBP,RQOFF(seed_value));rt_save(5,RAX);rt_set(6,8);x64_bind(loop);
        rt_load(RDI,5);rt_load(RSI,6);x64_xor(RDX,RDX);x64_mov_imm(RAX,318);x64_syscall();rt_cmp(RAX,-4);x64_jcc(CC_Z,loop);
        x64_test(RAX,RAX);x64_jcc(CC_NS,positive);x64_neg(RAX);random_entropy_failure();x64_bind(positive);
        x64_test(RAX,RAX);x64_jcc(CC_NZ,nonzero);x64_mov_imm(RAX,5);random_entropy_failure();x64_bind(nonzero);
        rt_load(RCX,5);x64_add(RCX,RAX);rt_save(5,RCX);rt_load(RCX,6);x64_sub(RCX,RAX);rt_save(6,RCX);x64_test(RCX,RCX);x64_jcc(CC_NZ,loop);
    }else{
        int good=rt_label("random.entropy.ok"),mapped=rt_label("random.entropy.mapped");
        x64_xor(RCX,RCX);x64_lea(RDX,RBP,RQOFF(seed_value));x64_mov_imm(R8,8);x64_mov_imm(R9,2);
        file_win_begin();file_win_end(IMP_BCRYPT_GEN_RANDOM);x64_test(RAX,RAX);x64_jcc(CC_Z,good);rt_save(7,RAX);
        x64_mov(RCX,RAX);file_win_begin();file_win_end(IMP_RTL_NTSTATUS_TO_DOS_ERROR);
        rt_cmp(RAX,317);x64_jcc(CC_NZ,mapped);rt_load(RAX,7);x64_bind(mapped);random_entropy_failure();x64_bind(good);
    }
    x64_store_imm(RBP,RQOFF(seed_auto),1);
}
static void random_copy_bits(int stack,int field)
{
    int zero=rt_label("random.zero.bits");x64_load(RAX,RBP,stack);x64_test(RAX,RAX);x64_jcc(CC_Z,zero);
    x64_load(RCX,RAX,0);x64_store(RBP,field,RCX);x64_load(RCX,RAX,8);x64_store(RBP,field+8,RCX);x64_bind(zero);
}
static void gen_random_runtime(void)
{
    int i,global=rt_label("random.global"),state=rt_label("random.state"),numeric=rt_label("random.numeric"),text=rt_label("random.text"),ready=rt_label("random.ready");
    int boolean=rt_label("random.boolean"),pool=rt_label("random.pool"),validate=rt_label("random.validate"),generate=rt_label("random.generate");
    rt_enter(random_call_label,80);
    for(i=0;i<(int)sizeof(RandomRequest);i+=8)x64_store_imm(RBP,RQ+i,0);
    x64_load(RAX,RBP,56);rt_save(1,RAX);x64_test(RAX,RAX);x64_jcc(CC_Z,global);
    x64_load(RAX,RAX,0);x64_load(RAX,RAX,24);x64_jmp(state);x64_bind(global);x64_lea(RAX,R14,RANDOM_STATE);x64_bind(state);
    x64_store(RBP,RQOFF(state),RAX);
    x64_load(RAX,RBP,48);x64_store(RBP,RQOFF(op),RAX);x64_load(RCX,RBP,40);x64_store(RBP,RQOFF(ty),RCX);
    x64_load(RCX,RBP,32);x64_store(RBP,RQOFF(flags),RCX);
    rt_cmp(RAX,1);x64_jcc(CC_Z,numeric);rt_cmp(RAX,2);x64_jcc(CC_Z,numeric);rt_cmp(RAX,3);x64_jcc(CC_Z,boolean);rt_cmp(RAX,4);x64_jcc(CC_AE,text);
    x64_load(RCX,RBP,24);x64_store(RBP,RQOFF(first),RCX);x64_jmp(ready);
    x64_bind(numeric);random_copy_bits(24,RQOFF(first));random_copy_bits(16,RQOFF(second));
    x64_store_imm(RBP,RQOFF(first_tag),-128);x64_store_imm(RBP,RQOFF(second_tag),-128);
    {int real=rt_label("random.real.tags"),second=rt_label("random.second.tag");
     x64_load(RAX,RBP,48);rt_cmp(RAX,2);x64_jcc(CC_Z,real);
     x64_load(RCX,RBP,32);x64_and_imm(RCX,64);x64_test(RCX,RCX);x64_jcc(CC_Z,second);x64_store_imm(RBP,RQOFF(first_tag),128);
     x64_bind(second);x64_load(RCX,RBP,32);x64_and_imm(RCX,128);x64_test(RCX,RCX);x64_jcc(CC_Z,ready);x64_store_imm(RBP,RQOFF(second_tag),128);x64_jmp(ready);
     x64_bind(real);x64_store_imm(RBP,RQOFF(first_tag),W_QUAD);x64_store_imm(RBP,RQOFF(second_tag),W_QUAD);x64_jmp(ready);}
    x64_bind(text);x64_load(RAX,RBP,48);rt_cmp(RAX,4);x64_jcc(CC_Z,pool);
    x64_load(RCX,RBP,24);x64_store(RBP,RQOFF(count),RCX);x64_load(RAX,RBP,16);x64_jmp(validate);
    x64_bind(pool);x64_load(RAX,RBP,24);x64_bind(validate);x64_load(RCX,RAX,0);x64_store(RBP,RQOFF(length),RCX);x64_add_imm(RAX,8);x64_store(RBP,RQOFF(text),RAX);
    x64_bind(boolean);x64_bind(ready);random_engine_call();
    {int no_text=rt_label("random.no.text");x64_load(RAX,RBP,48);rt_cmp(RAX,4);x64_jcc(CC_B,no_text);
     x64_load(RDI,RBP,RQOFF(capacity));x64_add_imm(RDI,8);x64_call(alloc_label);rt_save(3,RAX);x64_add_imm(RAX,8);x64_store(RBP,RQOFF(buffer),RAX);x64_bind(no_text);}
    rt_load(RAX,1);x64_test(RAX,RAX);x64_jcc(CC_NZ,generate);
    x64_load(RAX,RBP,RQOFF(need_random));x64_test(RAX,RAX);x64_jcc(CC_Z,generate);
    x64_load(RAX,R14,RANDOM_INITIALIZED);x64_test(RAX,RAX);x64_jcc(CC_NZ,generate);random_entropy();
    x64_bind(generate);x64_store_imm(RBP,RQOFF(phase),1);random_engine_call();
    {int local=rt_label("random.local");rt_load(RAX,1);x64_test(RAX,RAX);x64_jcc(CC_NZ,local);
     x64_load(RAX,RBP,48);x64_test(RAX,RAX);x64_jcc(CC_Z,global=rt_label("random.mark.seeded"));
     x64_load(RAX,RBP,RQOFF(need_random));x64_test(RAX,RAX);x64_jcc(CC_Z,local);
     x64_bind(global);x64_store_imm(R14,RANDOM_INITIALIZED,1);x64_bind(local);}
    {int bits=rt_label("random.return.bits"),boxed=rt_label("random.return.boxed");
     x64_load(RAX,RBP,48);rt_cmp(RAX,4);x64_jcc(CC_B,bits);rt_load(RAX,3);x64_load(RCX,RBP,RQOFF(used));x64_store(RAX,0,RCX);rt_leave();
     x64_bind(bits);x64_load(RAX,RBP,40);rt_cmp(RAX,128);x64_jcc(CC_Z,boxed);rt_cmp(RAX,-128);x64_jcc(CC_Z,boxed);rt_cmp(RAX,W_QUAD);x64_jcc(CC_Z,boxed);
     x64_load(RAX,RBP,RQOFF(out));rt_leave();x64_bind(boxed);x64_mov_imm(RDI,16);x64_call(alloc_label);
     x64_load(RCX,RBP,RQOFF(out));x64_store(RAX,0,RCX);x64_load(RCX,RBP,RQOFF(out)+8);x64_store(RAX,8,RCX);rt_leave();}
}
#undef RQOFF
#undef RQ
#undef RW
