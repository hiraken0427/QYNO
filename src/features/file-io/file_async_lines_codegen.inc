/* Validate and measure a captured line array before opening a destination. */
static void gen_file_async_lines_validation(void)
{
    int loop=rt_label("async.lines.validate"),scan=rt_label("async.lines.scan"),next=rt_label("async.lines.next");
    int sep=rt_label("async.lines.separator"),after=rt_label("async.lines.after.separator"),done=rt_label("async.lines.valid");
    int one=rt_label("async.lines.newline.one"),valid=rt_label("async.lines.newline.valid");
    int bad=rt_label("async.lines.newline.bad"),line=rt_label("async.lines.line.bad"),large=rt_label("async.lines.large");
    rt_enter(async_file_lines_label,8);rt_save(1,RAX);
    x64_load(RAX,RAX,8);rt_save(2,RAX);x64_call(file_validate_label);x64_load(RCX,RAX,0);
    rt_cmp(RCX,1);x64_jcc(CC_Z,one);rt_cmp(RCX,2);x64_jcc(CC_NZ,bad);
    x64_load8(RCX,RAX,8);rt_cmp(RCX,13);x64_jcc(CC_NZ,bad);
    x64_load8(RCX,RAX,9);rt_cmp(RCX,10);x64_jcc(CC_Z,valid);x64_jmp(bad);
    x64_bind(one);x64_load8(RCX,RAX,8);rt_cmp(RCX,10);x64_jcc(CC_Z,valid);rt_cmp(RCX,13);x64_jcc(CC_Z,valid);
    x64_bind(bad);file_fail(EX_SOFTWARE,QMSG_0795);
    x64_bind(valid);rt_load(RAX,1);x64_load(RCX,RAX,16);rt_save(3,RCX);x64_load(RCX,RAX,0);rt_save(8,RCX);
    rt_set(4,0);rt_set(5,0);
    x64_bind(loop);rt_load(RAX,3);x64_load(RCX,RAX,0);rt_load(RDX,5);x64_cmp(RDX,RCX);x64_jcc(CC_AE,done);
    x64_load(RAX,RAX,24);x64_shl_imm(RDX,3);x64_add(RAX,RDX);x64_load(RAX,RAX,0);rt_save(6,RAX);x64_call(file_validate_label);
    x64_load(RCX,RAX,0);rt_load(RDX,4);x64_add(RDX,RCX);x64_jcc(CC_O,large);rt_cmp(RDX,INT64_MAX-32);x64_jcc(CC_A,large);rt_save(4,RDX);
    x64_lea(RDX,RAX,8);x64_xor(R8,R8);
    x64_bind(scan);x64_cmp(R8,RCX);x64_jcc(CC_AE,next);x64_load8(R9,RDX,0);rt_cmp(R9,10);x64_jcc(CC_Z,line);rt_cmp(R9,13);x64_jcc(CC_Z,line);
    x64_add_imm(RDX,1);x64_add_imm(R8,1);x64_jmp(scan);
    x64_bind(next);rt_load(RAX,8);x64_test(RAX,RAX);x64_jcc(CC_NZ,sep);
    rt_load(RAX,5);x64_add_imm(RAX,1);rt_load(RDX,3);x64_load(RCX,RDX,0);x64_cmp(RAX,RCX);x64_jcc(CC_AE,after);
    x64_bind(sep);rt_load(RAX,2);x64_load(RCX,RAX,0);rt_load(RAX,4);x64_add(RAX,RCX);x64_jcc(CC_O,large);rt_cmp(RAX,INT64_MAX-32);x64_jcc(CC_A,large);rt_save(4,RAX);
    x64_bind(after);rt_load(RAX,5);x64_add_imm(RAX,1);rt_save(5,RAX);x64_jmp(loop);
    x64_bind(line);file_fail(EX_SOFTWARE,QMSG_0797);
    x64_bind(large);file_fail(EX_SOFTWARE,QMSG_0796);
    x64_bind(done);rt_load(RAX,4);rt_leave();
}
