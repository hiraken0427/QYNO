/* Event-loop side only. Workers never receive any pointer from these roots. */
static uint64_t io_error(AsyncRuntime *r,AsyncTask *t,uint64_t os,uint64_t code,uint64_t step)
{return r->file_error(r,t,os,code,step);}
static void io_dispose(AsyncTask *t)
{if(t->io_job){file_async_release(t->io_job);t->io_job=0;}t->io_text=0;t->io_result=0;t->io_lines=0;t->io_separator=0;t->io_line=0;}
static void io_complete(AsyncRuntime *r,AsyncTask *t)
{
 FileAsyncJob *j=t->io_job;uint64_t error=0,previous=0;
 if(j->fatal){r->file_fatal(r,t,j->fatal);return;}
 if(j->code)error=io_error(r,t,j->error,j->code,j->step);
 if(j->close_error){previous=error;error=io_error(r,t,j->close_error,11,5);if(previous)error=with_related(r,error,&previous,1);}
 if(error){if(t->cancel_error)error=with_related(r,error,&t->cancel_error,1);t->error=error;io_dispose(t);finish(t);}
 else if(t->cancel_error){t->error=t->cancel_error;io_dispose(t);finish(t);}
 else if(j->mode){t->result=j->length;io_dispose(t);finish(t);}
}
static void io_refill(AsyncRuntime *r)
{
 while(r->io_backlog&&r->io_ready_count<64){
  AsyncTask *t=r->io_backlog;r->io_backlog=t->io_next;if(!r->io_backlog)r->io_backlog_last=0;t->io_next=0;
  if(r->io_ready_last)r->io_ready_last->io_next=t;else r->io_ready=t;r->io_ready_last=t;r->io_ready_count++;
 }
}
static uint64_t io_native_mode(uint64_t mode)
{
 switch(mode){
 case FA_READ_TEXT:case FA_READ_LINES:case FA_READ_BYTES:return 0;
 case FA_APPEND_TEXT:case FA_APPEND_BYTES:return 2;
 default:return 1;
 }
}
static int io_write_mode(AsyncTask *t){return io_native_mode(t->io_mode)!=0;}
static int io_progress(AsyncRuntime *r);
static void io_submit(AsyncRuntime *r,AsyncTask *t,AsyncRequest *q)
{
 uint64_t error=0;
 t->state=t->cancel_error?3:2;t->io_path=(uint64_t *)q->args[q->nargs-1];t->io_mode=q->flags;t->io_shape=q->shape;
 if(t->io_mode==FA_READ_LINES){t->io_limit=q->args[1];t->io_max_lines=q->args[0];}
 else if(t->io_mode==FA_READ_TEXT||t->io_mode==FA_READ_BYTES)t->io_limit=q->args[0];
 else if(t->io_mode==FA_WRITE_BYTES||t->io_mode==FA_APPEND_BYTES){t->io_lines=(uint64_t *)q->args[0];t->io_size=t->io_lines[0];}
 else if(t->io_mode==FA_WRITE_LINES){t->io_lines=(uint64_t *)q->args[2];t->io_separator=(uint64_t *)q->args[1];t->io_final=q->args[0];t->io_size=(uint64_t)q->delay.seconds;}
 else{t->io_text=(uint64_t *)q->args[0];t->io_size=t->io_text[0];}
 if(t->cancel_error){t->error=t->cancel_error;finish(t);return;}
 if(!r->io_pool){r->io_pool=file_async_pool(r->io_windows,r->io_imports,&error);if(!r->io_pool){r->file_fatal(r,t,71);return;}}
 error=file_async_pool_error(r->io_pool);
 if(error){t->error=io_error(r,t,error,14,6);finish(t);return;}
 t->io_job=file_async_job(r->io_pool,(const unsigned char *)(t->io_path+1),t->io_path[0],io_native_mode(t->io_mode),t->io_limit);
 if(!t->io_job){r->file_fatal(r,t,71);return;}
 if(t->io_job->phase==2){io_complete(r,t);return;}
 if(r->io_backlog_last)r->io_backlog_last->io_next=t;else r->io_backlog=t;r->io_backlog_last=t;
 io_refill(r);io_progress(r);
}
static int io_utf8_byte(AsyncTask *t,unsigned char c)
{
 if(!t->io_utf8){
  if(c<0x80)return 1;
  if(c>=0xc2&&c<=0xdf){t->io_utf8=1;t->io_utf8_min=0x80;t->io_utf8_value=c&31;return 1;}
  if(c>=0xe0&&c<=0xef){t->io_utf8=2;t->io_utf8_min=0x800;t->io_utf8_value=c&15;return 1;}
  if(c>=0xf0&&c<=0xf4){t->io_utf8=3;t->io_utf8_min=0x10000;t->io_utf8_value=c&7;return 1;}
  return 0;
 }
 if((c&0xc0)!=0x80)return 0;
 t->io_utf8_value=(t->io_utf8_value<<6)|(c&63);
 if(--t->io_utf8)return 1;
 return t->io_utf8_value>=t->io_utf8_min&&t->io_utf8_value<=0x10ffff&&!(t->io_utf8_value>=0xd800&&t->io_utf8_value<=0xdfff);
}
#include "file_async_lines.inc"
#include "file_async_bytes.inc"

static int io_progress(AsyncRuntime *r)
{
 AsyncTask *t,*next,**link;int active=0;uint64_t budget=65536,selected=0,distance=4,next_slot=0;
 /* Remove cancelled requests before reserving a worker or opening anything. */
 {int pass;for(pass=0;pass<2;pass++){
  AsyncTask *previous=0;link=pass?&r->io_backlog:&r->io_ready;
  while(*link){t=*link;if(t->cancel_error){*link=t->io_next;if(pass){if(r->io_backlog_last==t)r->io_backlog_last=previous;}else{r->io_ready_count--;if(r->io_ready_last==t)r->io_ready_last=previous;}
    t->io_next=0;t->error=t->cancel_error;io_dispose(t);finish(t);
   }else{previous=t;link=&t->io_next;}
  }
 }}
 io_refill(r);
 while(r->io_ready){
  t=r->io_ready;if(!file_async_reserve(t->io_job,io_write_mode(t)?t->io_size:0))break;
  r->io_ready=t->io_next;if(!r->io_ready)r->io_ready_last=0;t->io_next=0;r->io_ready_count--;
  if(!io_write_mode(t))file_async_launch(t->io_job);
  io_refill(r);
 }
 /* Rotate byte-copy work across native requests instead of starving later
  * results behind a large string. Errors and cancellation never need budget. */
 for(t=r->pending;t;t=t->pending_next)if(t->kind==ASYNC_FILE&&t->io_job&&t->io_job->worker){
  FileAsyncJob *j=t->io_job;int state=file_async_status(j);
  if(state==2||state==4){
   uint64_t slot=file_async_slot(j),delta=(slot+4-r->io_copy_cursor)%4;
   if(delta<distance){distance=delta;selected=j->id;next_slot=(slot+1)%4;}
  }
 }
 if(distance<4)r->io_copy_cursor=next_slot;
 for(t=r->pending;t;t=next){
  FileAsyncJob *j;int state;uint64_t count,i;next=t->pending_next;
  if(t->kind!=ASYNC_FILE||!(j=t->io_job))continue;
  active=1;if(t->cancel_error)file_async_cancel(j);
  if(!j->worker)continue;
  state=file_async_status(j);
  if(state==4){
   if(j->fatal){r->file_fatal(r,t,j->fatal);continue;}
   if(t->cancel_error){t->error=t->cancel_error;io_dispose(t);finish(t);continue;}
   if(j->id!=selected)continue;
   if(t->io_mode==FA_WRITE_LINES)io_prepare_lines(t,&budget);
   else if(t->io_mode==FA_WRITE_BYTES||t->io_mode==FA_APPEND_BYTES)io_prepare_bytes(t,&budget);
   else{
    count=j->length-t->io_offset;if(count>budget)count=budget;
    for(i=0;i<count;i++)j->data[t->io_offset+i]=((unsigned char *)(t->io_text+1))[t->io_offset+i];
    t->io_offset+=count;budget-=count;
   }
   if(t->io_offset==j->length){t->io_offset=0;file_async_launch(j);}
   continue;
  }
  if(state!=2)continue;
  io_complete(r,t);if(done(t))continue;
  if(j->id!=selected)continue;
  if(t->io_mode==FA_READ_BYTES){io_read_bytes(r,t,&budget);continue;}
  if(t->io_mode==FA_READ_LINES&&t->io_phase){io_read_lines(r,t,&budget);continue;}
  if(!t->io_result){t->io_result=r->allocate(r,j->length+8);t->io_result[0]=j->length;}
  count=j->length-t->io_offset;if(count>budget)count=budget;
  for(i=0;i<count;i++){
   unsigned char c=j->data[t->io_offset+i];
   if(!io_utf8_byte(t,c)){r->file_fatal(r,t,65);return 1;}
   ((unsigned char *)(t->io_result+1))[t->io_offset+i]=c;
  }
  t->io_offset+=count;budget-=count;
  if(t->io_offset==j->length){
   if(t->io_utf8){r->file_fatal(r,t,65);return 1;}
   if(t->io_mode==FA_READ_LINES){t->io_phase=1;t->io_offset=t->io_line_start=0;io_read_lines(r,t,&budget);}
   else{t->result=(uint64_t)t->io_result;io_dispose(t);finish(t);}
  }
 }
 return active;
}
static int io_needs_turn(AsyncRuntime *r)
{
 AsyncTask *t;
 for(t=r->pending;t;t=t->pending_next)if(t->kind==ASYNC_FILE&&t->io_job&&t->io_job->worker){
  int state=file_async_status(t->io_job);if(state==2||state==4)return 1;
 }
 return 0;
}
