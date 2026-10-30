/* Path-based I/O shares the existing handle, buffering, and error machinery. */
static int file_bulk_close_label,file_bulk_read_label;
static void bulk_pending(int failure)
{
    x64_load(R11,R14,IO_PENDING);x64_test(R11,R11);x64_jcc(CC_NZ,failure);
}
static void bulk_contract_limit(Reg reg,long long maximum)
{
    int bad=rt_label("bulk.limit.bad"),good=rt_label("bulk.limit.good");
    x64_test(reg,reg);x64_jcc(CC_S,bad);rt_cmp(reg,maximum);x64_jcc(CC_BE,good);
    x64_bind(bad);file_fail(EX_SOFTWARE,"File limit is out of range");x64_bind(good);
}
static void bulk_range_error(const char *message)
{
    x64_store_imm(R14,FILE_OS_ERROR,0);x64_mov_imm(RDI,13);
    x64_lea_data(RAX,prog_add_str(prog,message,strlen(message)));x64_call(io_raise_label);
}
static void bulk_open(int path_slot,const char *mode)
{
    rt_load(RAX,path_slot);x64_push(RAX);x64_lea_data(RAX,prog_add_str(prog,mode,1));x64_push(RAX);
    x64_call(file_labels[BI_FILE_OPEN-BI_FILE_OPEN]);x64_add_imm(RSP,16);io_rt_check();
}
static void gen_file_bulk_close(void)
{
    int restore=rt_label("bulk.close.restore"),done=rt_label("bulk.close.done");
    int link=rt_label("bulk.close.link"),tail=rt_label("bulk.close.tail");
    rt_enter(file_bulk_close_label,2);rt_save(1,RAX);
    x64_load(RCX,R14,IO_PENDING);rt_save(2,RCX);x64_store_imm(R14,IO_PENDING,0);
    x64_push(RAX);x64_call(file_labels[BI_FILE_CLOSE-BI_FILE_OPEN]);x64_add_imm(RSP,8);
    x64_load(RAX,R14,IO_PENDING);x64_test(RAX,RAX);x64_jcc(CC_Z,restore);
    rt_load(RDX,2);x64_test(RDX,RDX);x64_jcc(CC_Z,done);
    x64_bind(link);x64_load(RCX,RAX,40);x64_test(RCX,RCX);x64_jcc(CC_Z,tail);
    x64_mov(RAX,RCX);x64_jmp(link);x64_bind(tail);x64_store(RAX,40,RDX);x64_jmp(done);
    x64_bind(restore);rt_load(RAX,2);x64_store(R14,IO_PENDING,RAX);
    x64_bind(done);rt_leave();
}
static void gen_file_bulk_read(void)
{
    int loop=rt_label("bulk.read.loop"),finish=rt_label("bulk.read.close"),large=rt_label("bulk.read.limit");
    /* rax=path, rdi=max_bytes -> validated-size byte accumulator, closed handle. */
    rt_enter(file_bulk_read_label,4);rt_save(1,RAX);rt_save(2,RDI);x64_store(R14,FILE_PATH,RAX);
    bulk_contract_limit(RDI,INT64_MAX-32);
    x64_mov_imm(RDI,24);x64_call(alloc_label);rt_save(4,RAX);
    bulk_open(1,"r");rt_save(3,RAX);
    x64_bind(loop);rt_load(RAX,3);x64_call(file_getc_label);bulk_pending(finish);
    rt_cmp(RAX,-1);x64_jcc(CC_Z,finish);
    /* getc filled the fixed read buffer and consumed its first available byte. */
    rt_load(RAX,3);x64_load(RCX,RAX,24);x64_sub_imm(RCX,1);
    x64_load(RDX,RAX,32);x64_sub(RDX,RCX);
    x64_load(RSI,RAX,40);x64_add(RSI,RCX);
    rt_load(RAX,4);x64_load(R8,RAX,0);rt_load(RDI,2);x64_sub(RDI,R8);
    x64_cmp(RDX,RDI);x64_jcc(CC_A,large);x64_call(file_append_label);
    rt_load(RAX,3);x64_load(RCX,RAX,32);x64_store(RAX,24,RCX);x64_jmp(loop);
    x64_bind(large);bulk_range_error("File exceeds max_bytes");
    x64_bind(finish);rt_load(RAX,3);x64_call(file_bulk_close_label);io_rt_check();rt_load(RAX,4);rt_leave();
}
static void gen_file_bulk_read_entries(void)
{
    int loop=rt_label("bulk.lines.loop"),scan=rt_label("bulk.lines.scan"),end=rt_label("bulk.lines.end");
    int cr=rt_label("bulk.lines.cr"),delimiter=rt_label("bulk.lines.delimiter"),slice=rt_label("bulk.lines.slice");
    int done=rt_label("bulk.lines.done"),too_many=rt_label("bulk.lines.limit");
    rt_enter(file_labels[BI_FILE_READ_TEXT-BI_FILE_OPEN],0);
    x64_load(RAX,RBP,24);x64_load(RDI,RBP,16);x64_call(file_bulk_read_label);io_rt_check();
    x64_call(file_string_label);rt_leave();

    rt_enter(file_labels[BI_FILE_READ_LINES-BI_FILE_OPEN],8);
    x64_load(RAX,RBP,32);x64_store(R14,FILE_PATH,RAX);
    x64_load(RDI,RBP,16);bulk_contract_limit(RDI,(INT64_MAX-32)/8);rt_save(2,RDI);
    x64_load(RAX,RBP,32);x64_load(RDI,RBP,24);x64_call(file_bulk_read_label);io_rt_check();
    x64_call(file_string_label);rt_save(1,RAX);
    x64_load(RCX,R14,FILE_CONTEXT);x64_store(R14,COLLECTION_CONTEXT,RCX);
    x64_mov_imm(RDI,40);x64_call(alloc_label);rt_save(3,RAX);
    x64_store_imm(RAX,0,0);x64_store_imm(RAX,8,0);collection_shape(array_type(TY_STR),NULL,RDI);
    x64_store(RAX,16,RDI);x64_store_imm(RAX,24,0);x64_store_imm(RAX,32,0);rt_set(4,0);
    x64_bind(loop);rt_load(RAX,1);x64_load(RDX,RAX,0);rt_load(RCX,4);
    x64_cmp(RCX,RDX);x64_jcc(CC_AE,done);rt_save(5,RCX);
    rt_load(RAX,3);x64_load(RAX,RAX,0);rt_load(RDI,2);x64_cmp(RAX,RDI);x64_jcc(CC_AE,too_many);
    x64_bind(scan);rt_load(RAX,1);x64_load(RDX,RAX,0);x64_cmp(RCX,RDX);x64_jcc(CC_AE,end);
    x64_lea(RSI,RAX,8);x64_add(RSI,RCX);x64_load8(R8,RSI,0);
    rt_cmp(R8,10);x64_jcc(CC_Z,delimiter);rt_cmp(R8,13);x64_jcc(CC_Z,cr);
    x64_add_imm(RCX,1);x64_jmp(scan);
    x64_bind(cr);rt_save(6,RCX);x64_add_imm(RCX,1);x64_cmp(RCX,RDX);x64_jcc(CC_AE,slice);
    x64_load8(R8,RSI,1);rt_cmp(R8,10);x64_jcc(CC_NZ,slice);x64_add_imm(RCX,1);x64_jmp(slice);
    x64_bind(delimiter);rt_save(6,RCX);x64_add_imm(RCX,1);x64_jmp(slice);
    x64_bind(end);rt_save(6,RCX);
    x64_bind(slice);rt_save(7,RCX);rt_load(RDI,6);rt_load(RCX,5);x64_sub(RDI,RCX);x64_add_imm(RDI,8);
    x64_call(alloc_label);rt_save(8,RAX);rt_load(RCX,6);rt_load(RDX,5);x64_sub(RCX,RDX);x64_store(RAX,0,RCX);
    x64_lea(RDI,RAX,8);rt_load(RSI,1);x64_add_imm(RSI,8);x64_add(RSI,RDX);x64_rep_movsb();
    rt_load(RAX,3);rt_load(RDI,8);x64_call(array_append_label);
    rt_load(RCX,7);rt_save(4,RCX);x64_jmp(loop);
    x64_bind(too_many);bulk_range_error("File exceeds max_lines");rt_leave();
    x64_bind(done);rt_load(RAX,3);rt_leave();
}
static void gen_file_bulk_write_text(int builtin,const char *mode)
{
    rt_enter(file_labels[builtin-BI_FILE_OPEN],4);
    x64_load(RAX,RBP,24);rt_save(1,RAX);x64_store(R14,FILE_PATH,RAX);
    x64_load(RAX,RBP,16);rt_save(2,RAX);x64_call(file_validate_label);
    bulk_open(1,mode);rt_save(3,RAX);
    rt_load(RSI,2);x64_load(RDX,RSI,0);rt_save(4,RDX);x64_add_imm(RSI,8);
    rt_load(RAX,3);x64_call(file_write_all_label);
    rt_load(RAX,3);x64_call(file_bulk_close_label);io_rt_check();rt_load(RAX,4);rt_leave();
}
static void bulk_lines_separator(int skip)
{
    int include=rt_label("bulk.lines.include.separator");
    rt_load(RAX,4);x64_test(RAX,RAX);x64_jcc(CC_NZ,include);
    rt_load(RCX,5);x64_add_imm(RCX,1);rt_load(RAX,2);x64_load(RAX,RAX,0);
    x64_cmp(RCX,RAX);x64_jcc(CC_AE,skip);x64_bind(include);
}
static void gen_file_bulk_write_lines(void)
{
    int valid=rt_label("bulk.newline.valid"),one=rt_label("bulk.newline.one"),bad=rt_label("bulk.newline.bad");
    int validate=rt_label("bulk.lines.validate"),validated=rt_label("bulk.lines.validated");
    int scan=rt_label("bulk.lines.content"),measured=rt_label("bulk.lines.measured"),no_separator=rt_label("bulk.lines.no.separator");
    int large=rt_label("bulk.lines.size.error"),line_error=rt_label("bulk.lines.content.error");
    int loop=rt_label("bulk.lines.write"),finish=rt_label("bulk.lines.close"),next=rt_label("bulk.lines.next");
    rt_enter(file_labels[BI_FILE_WRITE_LINES-BI_FILE_OPEN],8);
    x64_load(RAX,RBP,40);rt_save(1,RAX);x64_store(R14,FILE_PATH,RAX);
    x64_load(RAX,RBP,32);rt_save(2,RAX);x64_load(RAX,RBP,24);rt_save(3,RAX);
    x64_load(RAX,RBP,16);rt_save(4,RAX);rt_set(5,0);rt_set(6,0);
    rt_load(RAX,3);x64_call(file_validate_label);x64_load(RCX,RAX,0);
    rt_cmp(RCX,1);x64_jcc(CC_Z,one);rt_cmp(RCX,2);x64_jcc(CC_NZ,bad);
    x64_load8(RCX,RAX,8);rt_cmp(RCX,13);x64_jcc(CC_NZ,bad);
    x64_load8(RCX,RAX,9);rt_cmp(RCX,10);x64_jcc(CC_Z,valid);x64_jmp(bad);
    x64_bind(one);x64_load8(RCX,RAX,8);rt_cmp(RCX,13);x64_jcc(CC_Z,valid);rt_cmp(RCX,10);x64_jcc(CC_Z,valid);
    x64_bind(bad);file_fail(EX_SOFTWARE,"write_lines newline must be LF, CRLF, or CR");x64_bind(valid);
    x64_bind(validate);rt_load(RAX,2);x64_load(RDX,RAX,0);rt_load(RCX,5);x64_cmp(RCX,RDX);x64_jcc(CC_AE,validated);
    x64_load(RAX,RAX,24);x64_shl_imm(RCX,3);x64_add(RAX,RCX);x64_load(RAX,RAX,0);rt_save(8,RAX);x64_call(file_validate_label);
    x64_load(RDX,RAX,0);rt_load(RCX,6);x64_add(RCX,RDX);x64_jcc(CC_O,large);
    rt_cmp(RCX,INT64_MAX-32);x64_jcc(CC_A,large);rt_save(6,RCX);x64_lea(RSI,RAX,8);x64_xor(RCX,RCX);
    x64_bind(scan);x64_cmp(RCX,RDX);x64_jcc(CC_AE,measured);
    x64_load8(R8,RSI,0);rt_cmp(R8,13);x64_jcc(CC_Z,line_error);rt_cmp(R8,10);x64_jcc(CC_Z,line_error);
    x64_add_imm(RSI,1);x64_add_imm(RCX,1);x64_jmp(scan);
    x64_bind(measured);bulk_lines_separator(no_separator);
    rt_load(RAX,3);x64_load(RDX,RAX,0);rt_load(RCX,6);x64_add(RCX,RDX);x64_jcc(CC_O,large);
    rt_cmp(RCX,INT64_MAX-32);x64_jcc(CC_A,large);rt_save(6,RCX);
    x64_bind(no_separator);rt_load(RCX,5);x64_add_imm(RCX,1);rt_save(5,RCX);x64_jmp(validate);
    x64_bind(large);file_fail(EX_SOFTWARE,"write_lines output is too large");
    x64_bind(line_error);file_fail(EX_SOFTWARE,"write_lines elements cannot contain CR or LF");
    x64_bind(validated);bulk_open(1,"w");rt_save(7,RAX);rt_set(5,0);
    x64_bind(loop);rt_load(RAX,2);x64_load(RDX,RAX,0);rt_load(RCX,5);x64_cmp(RCX,RDX);x64_jcc(CC_AE,finish);
    x64_load(RSI,RAX,24);x64_shl_imm(RCX,3);x64_add(RSI,RCX);x64_load(RSI,RSI,0);x64_load(RDX,RSI,0);x64_add_imm(RSI,8);
    rt_load(RAX,7);x64_call(file_write_all_label);bulk_pending(finish);
    bulk_lines_separator(next);rt_load(RSI,3);x64_load(RDX,RSI,0);x64_add_imm(RSI,8);rt_load(RAX,7);x64_call(file_write_all_label);bulk_pending(finish);
    x64_bind(next);rt_load(RCX,5);x64_add_imm(RCX,1);rt_save(5,RCX);x64_jmp(loop);
    x64_bind(finish);rt_load(RAX,7);x64_call(file_bulk_close_label);io_rt_check();rt_load(RAX,6);rt_leave();
}
static void gen_file_bulk_runtime(void)
{
    file_bulk_close_label=rt_label("bulk.close");file_bulk_read_label=rt_label("bulk.read");
    gen_file_bulk_close();gen_file_bulk_read();gen_file_bulk_read_entries();
    gen_file_bulk_write_text(BI_FILE_WRITE_TEXT,"w");gen_file_bulk_write_text(BI_FILE_APPEND_TEXT,"a");gen_file_bulk_write_lines();
}
