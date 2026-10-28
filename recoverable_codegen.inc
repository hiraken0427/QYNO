/* Each source frame owns a lexical handler/finally chain. No non-local jumps. */
static void gen_io_finalizers(int target_loop)
{
    IoScope *s=io_scope;
    if(!s || (target_loop && s->loops<target_loop))return;
    io_scope=s->outer;
    if(s->node->rhs)gen_stmts(s->node->rhs);
    gen_io_finalizers(target_loop);
    io_scope=s;
}
static void gen_io_finally(Node *n,IoScope *outer)
{
    int failure=rt_label("io.finally.failure"),done=rt_label("io.finally.done");
    IoScope cleanup={outer,n,failure,depth,loop_depth};
    io_scope=&cleanup;gen_stmts(n->rhs);io_scope=outer;x64_jmp(done);
    x64_bind(failure);
    /* Keep the displaced error for unhandled-error diagnostics. */
    x64_load(RAX,R14,IO_PENDING);x64_load(RDX,RBP,-n->ref_temp->offset);
    {int link=rt_label("io.error.link"),skip=rt_label("io.error.link.skip");
     x64_test(RDX,RDX);x64_jcc(CC_Z,skip);x64_cmp(RAX,RDX);x64_jcc(CC_Z,skip);
     x64_bind(link);x64_load(RCX,RAX,40);x64_test(RCX,RCX);
     {int end=rt_label("io.error.link.end");x64_jcc(CC_Z,end);x64_mov(RAX,RCX);x64_jmp(link);x64_bind(end);}
     x64_store(RAX,40,RDX);x64_bind(skip);}
    gen_io_check();x64_bind(done);
}
static void gen_try(Node *n)
{
    int caught=rt_label("io.try.caught"),finish=rt_label("io.try.finish"),done=rt_label("io.try.done");
    IoScope *outer=io_scope,body={outer,n,caught,depth,loop_depth},handler={outer,n,finish,depth,loop_depth};
    Node *arm,*saved_catch=io_catch;
    x64_store_imm(RBP,-n->ref_temp->offset,0);
    io_scope=&body;if(n->lhs)gen_stmt(n->lhs);gen_stmts(n->then);io_scope=outer;
    x64_jmp(finish);
    x64_bind(caught);x64_load(RAX,R14,IO_PENDING);x64_store(RBP,-n->var->offset,RAX);
    x64_load(RDX,RAX,24);x64_load(RDX,RDX,0);
    for(arm=n->els;arm;arm=arm->next){
        Node *p;int hit=rt_label("io.case.hit"),next=rt_label("io.case.next");
        if(!arm->lhs)x64_jmp(hit);
        for(p=arm->lhs;p;p=p->next){rt_cmp(RDX,p->val);x64_jcc(CC_Z,hit);}
        x64_jmp(next);x64_bind(hit);x64_store_imm(R14,IO_PENDING,0);
        io_scope=&handler;io_catch=n;gen_stmts(arm->then);io_catch=saved_catch;io_scope=outer;
        x64_jmp(finish);x64_bind(next);
    }
    x64_bind(finish);x64_load(RAX,R14,IO_PENDING);x64_store(RBP,-n->ref_temp->offset,RAX);
    x64_store_imm(R14,IO_PENDING,0);
    gen_io_finally(n,outer);
    x64_load(RAX,RBP,-n->ref_temp->offset);x64_store(R14,IO_PENDING,RAX);gen_io_check();
    x64_bind(done);io_scope=outer;io_catch=saved_catch;
}
