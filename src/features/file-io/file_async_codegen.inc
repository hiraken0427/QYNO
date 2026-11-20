#include "file_async_lines_codegen.inc"
/* C callbacks enter only on the event-loop thread. Preserve its previous I/O
 * context while constructing an independent task-owned error record. */
static void file_async_context(void)
{
    rt_load(RDX,7);
    x64_load(RCX,RDX,(int)offsetof(AsyncTask,io_path));x64_store(R14,FILE_PATH,RCX);
    {int i;int offsets[]={FILE_CONTEXT,IO_SOURCE,IO_LINE,IO_COLUMN};
     for(i=0;i<4;i++){x64_load(RCX,RDX,(int)offsetof(AsyncTask,site)+i*8);x64_store(R14,offsets[i],RCX);}}
    {const char *names[]={"read_text_async","write_text_async","append_text_async","read_lines_async","write_lines_async",
                          "read_bytes_async","write_bytes_async","append_bytes_async"};
     int i,ready=rt_label("async.file.operation.ready");
     for(i=0;i<(int)(sizeof(names)/sizeof(*names));i++){int next=rt_label("async.file.operation.next");
         x64_load(RCX,RDX,(int)offsetof(AsyncTask,io_mode));rt_cmp(RCX,i);x64_jcc(CC_NZ,next);
         x64_lea_data(RCX,prog_add_str(prog,names[i],strlen(names[i])));x64_jmp(ready);x64_bind(next);}
     x64_lea_data(RCX,empty_str_offset);x64_bind(ready);x64_store(R14,IO_OPERATION,RCX);}

}
static void gen_file_async_callbacks(void)
{
    int i;
    gen_file_async_lines_validation();
    async_c_enter(async_file_error_label);
    for(i=0;i<11;i++){x64_load(RCX,R14,256+i*8);rt_save(20+i,RCX);}
    file_async_context();rt_load(RCX,8);x64_store(R14,FILE_OS_ERROR,RCX);rt_load(RDI,9);
    {const char *messages[]={QMSG_0416,QMSG_0747,QMSG_0312,QMSG_0371,QMSG_0113,QMSG_0895,QMSG_0793,QMSG_0794};
     int ready=rt_label("async.file.message.ready");
     for(i=1;i<=8;i++){
         int next=rt_label("async.file.message.next");rt_load(RCX,10);rt_cmp(RCX,i);x64_jcc(CC_NZ,next);
         x64_lea_data(RAX,prog_add_str(prog,messages[i-1],strlen(messages[i-1])));x64_jmp(ready);x64_bind(next);
     }
     x64_lea_data(RAX,empty_str_offset);x64_bind(ready);}
    x64_call(io_raise_label);x64_load(RAX,R14,IO_PENDING);rt_save(19,RAX);
    for(i=0;i<11;i++){rt_load(RCX,20+i);x64_store(R14,256+i*8,RCX);}
    rt_load(RAX,19);async_c_leave();

    async_c_enter(async_file_fatal_label);file_async_context();
    {int memory=rt_label("async.file.fatal.memory");rt_load(RCX,8);rt_cmp(RCX,65);x64_jcc(CC_NZ,memory);
     file_fail(EX_DATAERR,QMSG_0485);x64_bind(memory);file_fail(EX_SOFTWARE,QMSG_0635);}
}
