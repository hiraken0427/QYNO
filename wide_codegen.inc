static int wide_engine_label, wide_call_label;
static int wide_codegen_op(NodeKind k)
{return k==ND_ADD?W_ADD:k==ND_SUB?W_SUB:k==ND_MUL?W_MUL:k==ND_DIV?W_DIV:k==ND_MOD?W_MOD:k==ND_BIT_AND?W_AND:k==ND_BIT_OR?W_OR:k==ND_BIT_XOR?W_XOR:k==ND_BIT_NOT?W_NOT:k==ND_SHL?W_SHL:W_SHR;}
static void wide_check(Token *tok)
{int ok=new_label("wide.checked");x64_test(R8,R8);x64_jcc(CC_Z,ok);gen_fail(tok,"数値の変換・演算結果が範囲外です",EX_SOFTWARE);x64_bind(ok);}

/* RAX=left/source, RDI=right. The bridge preserves GC state and uses the same
 * SysV ABI on both targets; the freestanding image makes no OS calls. */
static void gen_wide(int op,Type ty,Type from,int aux,Token *tok)
{
    x64_mov_imm(R8,op);x64_mov_imm(R9,wide_tag(ty));x64_mov_imm(R10,wide_tag(from));x64_mov_imm(R11,aux);
    x64_call(wide_call_label);
    if(tok){int ok=new_label("wide.ok");x64_test(R8,R8);x64_jcc(CC_Z,ok);gen_fail(tok,"数値の演算・変換が不正です（0除算・範囲・指数・シフト量を確認してください）",EX_SOFTWARE);x64_bind(ok);}
}
static void wide_unpack(Reg value,Reg tag,int offset)
{
    int boxed=new_label("wide.unpack.boxed"),done=new_label("wide.unpack.done"),zero=new_label("wide.unpack.zero");
    x64_store(RBP,offset,value);x64_store_imm(RBP,offset+8,0);
    rt_cmp(tag,128);x64_jcc(CC_Z,boxed);rt_cmp(tag,-128);x64_jcc(CC_Z,boxed);rt_cmp(tag,W_QUAD);x64_jcc(CC_NZ,done);
    x64_bind(boxed);x64_test(value,value);x64_jcc(CC_Z,zero);
    x64_load(RCX,value,8);x64_load(value,value,0);x64_store(RBP,offset,value);x64_store(RBP,offset+8,RCX);x64_jmp(done);
    x64_bind(zero);x64_store_imm(RBP,offset,0);x64_bind(done);
}
static void gen_wide_runtime(void)
{
    int binary=new_label("wide.binary"),invoke=new_label("wide.invoke"),plain=new_label("wide.plain"),out=new_label("wide.out");
    int format=new_label("wide.format"),read=new_label("wide.read"),boxed=new_label("wide.box"),scalar=new_label("wide.scalar");
    int restore=new_label("wide.restore"),i;
    rt_enter(wide_call_label,64);
    rt_save(1,RSI);rt_save(2,RDX);rt_save(3,RDI);rt_save(4,RAX);
    x64_store(RBP,-208,R8);x64_store(RBP,-200,R9);x64_store(RBP,-192,R10);x64_store(RBP,-184,R11);
    x64_store_imm(RBP,-176,0);x64_store_imm(RBP,-168,0);x64_store_imm(RBP,-160,0);x64_store(RBP,-152,RDI);
    x64_store(RBP,-144,RSI);x64_lea(RCX,RBP,-512);x64_store(RBP,-136,RCX);
    wide_unpack(RAX,R10,-256);
    rt_load(RDI,3);x64_store(RBP,-240,RDI);x64_store_imm(RBP,-232,0);
    x64_load(R8,RBP,-208);
    { int ops[]={W_ADD,W_SUB,W_MUL,W_DIV,W_MOD,W_AND,W_OR,W_XOR,W_CMP,W_NEXT};
      for(i=0;i<(int)(sizeof(ops)/sizeof(*ops));i++){rt_cmp(R8,ops[i]);x64_jcc(CC_Z,binary);} }
    rt_cmp(R8,W_FORMAT);x64_jcc(CC_Z,format);rt_cmp(R8,W_READ);x64_jcc(CC_Z,read);x64_jmp(invoke);
    x64_bind(binary);x64_load(R9,RBP,-200);wide_unpack(RDI,R9,-240);x64_jmp(invoke);
    x64_bind(format);rt_load(RSI,1);x64_store(RBP,-184,RSI);x64_jmp(invoke);
    x64_bind(read);rt_load(RSI,1);rt_load(RDX,2);x64_sub(RDX,RSI);x64_store(RBP,-168,RDX);
    x64_bind(invoke);
    /* Probe the largest pure-C frame page by page, including on Windows. */
    for(i=0;i<16;i++){x64_sub_imm(RSP,4096);x64_store_imm(RSP,0,0);}
    x64_add_imm(RSP,65536);x64_and_imm(RSP,-16);x64_lea(RDI,RBP,-256);x64_call(wide_engine_label);
    x64_lea(RSP,RBP,-512);
    x64_load(R8,RBP,-176);x64_test(R8,R8);x64_jcc(CC_NZ,out);
    x64_load(R8,RBP,-208);rt_cmp(R8,W_FORMAT);x64_jcc(CC_Z,plain);
    rt_cmp(R8,W_CMP);x64_jcc(CC_Z,scalar);
    x64_load(R9,RBP,-200);rt_cmp(R9,-128);x64_jcc(CC_Z,boxed);rt_cmp(R9,128);x64_jcc(CC_Z,boxed);rt_cmp(R9,W_QUAD);x64_jcc(CC_NZ,scalar);
    x64_bind(boxed);x64_mov_imm(RDI,16);x64_call(alloc_label);
    x64_load(RCX,RBP,-224);x64_store(RAX,0,RCX);x64_load(RCX,RBP,-216);x64_store(RAX,8,RCX);x64_jmp(restore);
    x64_bind(scalar);x64_load(RAX,RBP,-224);x64_jmp(restore);
    x64_bind(plain);x64_load(RDI,RBP,-224);x64_add_imm(RDI,8);x64_call(alloc_label);rt_save(5,RAX);
    x64_load(RCX,RBP,-224);x64_store(RAX,0,RCX);x64_lea(RDI,RAX,8);x64_lea(RSI,RBP,-512);x64_rep_movsb();rt_load(RAX,5);x64_jmp(restore);
    x64_bind(out);x64_xor(RAX,RAX);
    x64_bind(restore);rt_load(RSI,1);rt_load(RDX,2);x64_load(R8,RBP,-208);rt_cmp(R8,W_READ);x64_jcc(CC_NZ,scalar=new_label("wide.no.read"));
    x64_load(RCX,RBP,-160);x64_add(RSI,RCX);x64_bind(scalar);
    x64_load(R8,RBP,-176);x64_mov(RCX,R8);rt_leave();
    while(prog->text.len%16){unsigned char nop=0x90;x64_blob(&nop,1);}
    x64_directive(".p2align 4");
    x64_blob(wide_blob,WIDE_ENTRY_OFFSET);x64_bind(wide_engine_label);x64_blob(wide_blob+WIDE_ENTRY_OFFSET,sizeof(wide_blob)-WIDE_ENTRY_OFFSET);
}
static void gen_wide_literal(Node *n)
{
    if(type_is_boxed(n->ty))x64_lea_data(RAX,prog_add_data(prog,(const char *)&n->wide,sizeof(n->wide)));
    else x64_mov_imm(RAX,integer_bits(n->wide.lo));
}
static void gen_wide_parse(Type ty)
{
    x64_mov_imm(R8,W_READ);x64_mov_imm(R9,wide_tag(ty));x64_mov_imm(R10,0);x64_xor(R11,R11);x64_call(wide_call_label);
}
