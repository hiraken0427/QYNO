/* Common runtime error views carry a hidden canonical identity at 40. The
 * visited table bounds output and avoids repeated shared causes in a DAG. */
static void error_write_string(void)
{x64_load(RDX,RAX,0);x64_lea(RSI,RAX,8);x64_call(write_err_label);}
static void error_write_number(void)
{x64_mov_imm(RDI,10);x64_xor(RSI,RSI);x64_mov_imm(R11,1);x64_call(integer_format_label);error_write_string();}
static void gen_related_error_runtime(void)
{
    int scan=rt_label("error.related.seen"),found=rt_label("error.related.found"),fresh=rt_label("error.related.fresh");
    int loop=rt_label("error.related.children"),done=rt_label("error.related.return"),limited=rt_label("error.related.limit");
    rt_enter(related_report_label,6);rt_save(1,RAX);rt_save(2,RDI);
    x64_load(R8,RAX,40);x64_load(RCX,RDI,0);rt_cmp(RCX,256);x64_jcc(CC_AE,limited);x64_xor(RDX,RDX);
    x64_bind(scan);x64_cmp(RDX,RCX);x64_jcc(CC_AE,fresh);
    x64_mov(RAX,RDX);x64_shl_imm(RAX,3);x64_add(RAX,RDI);x64_load(RAX,RAX,8);x64_cmp(RAX,R8);x64_jcc(CC_Z,done);
    x64_add_imm(RDX,1);x64_jmp(scan);
    x64_bind(fresh);x64_mov(RAX,RCX);x64_shl_imm(RAX,3);x64_add(RAX,RDI);x64_store(RAX,8,R8);x64_add_imm(RCX,1);x64_store(RDI,0,RCX);
    rt_load(RAX,1);x64_load(RAX,RAX,24);rt_save(3,RAX);
    file_emit_err(QMSG_0893);
    rt_load(RAX,3);x64_load(RAX,RAX,16);error_write_string();file_emit_err(":");
    rt_load(RAX,3);x64_load(RAX,RAX,24);error_write_number();file_emit_err(":");
    rt_load(RAX,3);x64_load(RAX,RAX,32);error_write_number();file_emit_err(": ");
    rt_load(RAX,3);x64_load(RAX,RAX,0);x64_call(type_enum(standard_type(emitting_ast,"lib","ErrorCode"))->name_label);error_write_string();file_emit_err(": ");
    rt_load(RAX,3);x64_load(RAX,RAX,8);error_write_string();
    {int no_path=rt_label("error.related.no.path");
     rt_load(RAX,1);x64_load(RAX,RAX,40);x64_load(RCX,RAX,56);x64_test(RCX,RCX);x64_jcc(CC_NZ,no_path);
     x64_load(RAX,RAX,24);x64_load(RAX,RAX,24);x64_test(RAX,RAX);x64_jcc(CC_Z,no_path);x64_load(RCX,RAX,0);x64_test(RCX,RCX);x64_jcc(CC_Z,no_path);rt_save(6,RAX);
     file_emit_err(QMSG_0836);rt_load(RAX,6);error_write_string();file_emit_err("]");x64_bind(no_path);}
    file_emit_err("\n");
    rt_load(RAX,3);x64_load(RAX,RAX,48);rt_save(4,RAX);rt_set(5,0);
    x64_bind(loop);rt_load(RAX,4);rt_load(RCX,5);x64_load(RDX,RAX,0);x64_cmp(RCX,RDX);x64_jcc(CC_AE,done);
    x64_load(RAX,RAX,24);x64_shl_imm(RCX,3);x64_add(RAX,RCX);x64_load(RAX,RAX,0);rt_load(RDI,2);x64_call(related_report_label);
    rt_load(RCX,5);x64_add_imm(RCX,1);rt_save(5,RCX);x64_jmp(loop);
    x64_bind(limited);rt_cmp(RCX,256);x64_jcc(CC_NZ,done);x64_store_imm(RDI,0,257);
    file_emit_err(QMSG_0894);
    x64_bind(found);x64_bind(done);rt_leave();
}
