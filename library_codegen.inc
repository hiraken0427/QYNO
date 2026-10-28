typedef struct SizeCode SizeCode;
struct SizeCode { SizeCode *next;Type ty;int label; };
static SizeCode *size_codes,*size_codes_tail;
static int path_call_label;
static int library_size_label(Type ty)
{
    SizeCode *p;
    for(p=size_codes;p;p=p->next)if(p->ty==ty)return p->label;
    p=calloc(1,sizeof(*p));if(!p)fatal("lib.size のコードを確保できません");
    p->ty=ty;p->label=new_label("lib.size");
    if(size_codes_tail)size_codes_tail->next=p;else size_codes=p;
    size_codes_tail=p;return p->label;
}
static void gen_library_builtin(Node *node)
{
    Type ty=node->lhs->ty;
    if(node->val==BI_LIB_SIZE){
        gen_expr(node->lhs);collection_context(node->tok);x64_call(library_size_label(ty));return;
    }
    if(node->val==BI_LIB_LENGTH){
        if(type_is_array(ty)||type_is_text(ty)){
            Node length=*node;length.kind=ND_LENGTH;length.lhs=node->lhs;gen_expr(&length);return;
        }
        gen_expr(node->lhs);gen_wide(W_DIGITS,TY_I64,ty,0,NULL);
        {int ok=new_label("lib.length.finite");x64_test(R8,R8);x64_jcc(CC_Z,ok);gen_fail(node->tok,"lib.length に NaN・無限大は指定できません",EX_SOFTWARE);x64_bind(ok);}
        return;
    }
    gen_expr(node->lhs);push();
    if(node->lhs->next)gen_expr(node->lhs->next);else x64_xor(RAX,RAX);
    x64_mov(RDI,RAX);pop(RAX);x64_mov_imm(R8,node->val-BI_PATH_JOIN);
    collection_context(node->tok);x64_call(path_call_label);
}
static void gen_library_size_runtime(void)
{
    SizeCode *p;
    for(p=size_codes;p;p=p->next){
        int bits=value_bit_width(p->ty);
        if(bits>=0){x64_bind(p->label);x64_mov_imm(RAX,bits);x64_ret();continue;}
        if(type_is_text(p->ty)){
            int ok=new_label("lib.size.text.ok");
            x64_bind(p->label);x64_load(RAX,RAX,0);x64_mov_imm(RCX,INT64_MAX/8);x64_cmp(RAX,RCX);x64_jcc(CC_BE,ok);
            collection_fail("lib.size の結果が int_64 の範囲を超えています\n");
            x64_bind(ok);x64_shl_imm(RAX,3);x64_ret();continue;
        }
        rt_enter(p->label,4);rt_save(1,RAX);rt_set(2,0);rt_set(3,0);
        if(type_is_array(p->ty)){
            int loop=new_label("lib.size.loop"),done=new_label("lib.size.done"),ok=new_label("lib.size.sum.ok");
            x64_bind(loop);rt_load(RAX,1);rt_load(RCX,2);x64_load(RDX,RAX,0);x64_cmp(RCX,RDX);x64_jcc(CC_AE,done);
            x64_load(RAX,RAX,24);x64_shl_imm(RCX,3);x64_add(RAX,RCX);x64_load(RAX,RAX,0);
            x64_call(library_size_label(array_element(p->ty)));rt_load(RCX,3);x64_add(RAX,RCX);x64_jcc(CC_NO,ok);
            collection_fail("lib.size の結果が int_64 の範囲を超えています\n");
            x64_bind(ok);rt_save(3,RAX);rt_load(RCX,2);x64_add_imm(RCX,1);rt_save(2,RCX);x64_jmp(loop);
            x64_bind(done);
        }else{
            ModelField *f;
            for(f=type_model(p->ty)->fields;f;f=f->next){
                int ok=new_label("lib.size.field.ok");
                rt_load(RAX,1);x64_load(RAX,RAX,24);x64_load(RAX,RAX,f->index*8);
                x64_call(library_size_label(f->ty));rt_load(RCX,3);x64_add(RAX,RCX);x64_jcc(CC_NO,ok);
                collection_fail("lib.size の結果が int_64 の範囲を超えています\n");
                x64_bind(ok);rt_save(3,RAX);
            }
        }
        rt_load(RAX,3);rt_leave();
    }
}
static void gen_library_path_runtime(void)
{
    int i,one=new_label("path.one"),capacity_ok=new_label("path.capacity.ok"),ok=new_label("path.ok"),bad_arg=new_label("path.bad.arg"),text=new_label("path.text");
    rt_enter(path_call_label,32);rt_save(1,RAX);rt_save(2,RDI);rt_save(3,R8);
    x64_load(RAX,RAX,0);x64_test(RDI,RDI);x64_jcc(CC_Z,one);x64_load(RCX,RDI,0);x64_add(RAX,RCX);x64_jcc(CC_O,bad_arg);
    x64_bind(one);x64_mov_imm(RCX,INT64_MAX-12);x64_cmp(RAX,RCX);x64_jcc(CC_BE,capacity_ok);x64_jmp(bad_arg);
    x64_bind(capacity_ok);x64_add_imm(RAX,4);rt_save(5,RAX);x64_mov(RDI,RAX);x64_add_imm(RDI,8);x64_call(alloc_label);rt_save(4,RAX);
    for(i=0;i<128;i+=8)x64_store_imm(RBP,-192+i,0);
    rt_load(RAX,1);x64_load(RCX,RAX,0);x64_store(RBP,-192+88,RCX);x64_add_imm(RAX,8);x64_store(RBP,-192+112,RAX);
    rt_load(RAX,2);x64_test(RAX,RAX);x64_jcc(CC_Z,one=new_label("path.no.right"));
    x64_load(RCX,RAX,0);x64_store(RBP,-192+24,RCX);x64_add_imm(RAX,8);x64_store(RBP,-192+16,RAX);x64_bind(one);
    x64_store_imm(RBP,-192+48,W_PATH);x64_store_imm(RBP,-192+64,target==TARGET_WINDOWS);
    rt_load(RAX,3);x64_store(RBP,-192+72,RAX);rt_load(RAX,5);x64_store(RBP,-192+104,RAX);
    rt_load(RAX,4);x64_add_imm(RAX,8);x64_store(RBP,-192+120,RAX);
    x64_and_imm(RSP,-16);x64_lea(RDI,RBP,-192);x64_call(wide_engine_label);x64_lea(RSP,RBP,-256);
    x64_load(RAX,RBP,-192+80);x64_test(RAX,RAX);x64_jcc(CC_Z,ok);
    rt_cmp(RAX,65);x64_jcc(CC_NZ,bad_arg);
    x64_load(RAX,R14,COLLECTION_CONTEXT);x64_load(RDX,RAX,0);x64_lea(RSI,RAX,8);x64_call(write_err_label);
    x64_lea_data(RSI,prog_add_data(prog,"io.path のパスが不正な UTF-8 です\n",strlen("io.path のパスが不正な UTF-8 です\n")));
    x64_mov_imm(RDX,(long long)strlen("io.path のパスが不正な UTF-8 です\n"));x64_mov_imm(R8,EX_DATAERR);x64_jmp(runtime_error_label);
    x64_bind(bad_arg);collection_fail("io.path の引数が不正です（NUL・拡張子・パス形式を確認してください）\n");
    x64_bind(ok);rt_load(RAX,3);rt_cmp(RAX,5);x64_jcc(CC_B,text);rt_cmp(RAX,6);x64_jcc(CC_A,text);
    x64_load(RAX,RBP,-192+32);rt_leave();
    x64_bind(text);rt_load(RAX,4);x64_load(RCX,RBP,-192+32);x64_store(RAX,0,RCX);rt_leave();
}
