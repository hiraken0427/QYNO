/* Freestanding native I/O workers. No language allocator, TLS, libc, or
 * writable globals. All OS errors are captured by the thread that sees them. */
#include "file_async_runtime.h"
#define FA_CHUNK 65536u
#define FA_STACK (256u*1024u)
#define FA_MS __attribute__((ms_abi))
#if __GCC_ATOMIC_INT_LOCK_FREE != 2
#error File workers require lock-free 32-bit atomics
#endif
typedef uint64_t (FA_MS *WinCall)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
struct FileAsyncWorker {
 FileAsyncPool *pool;FileAsyncJob *job;void *stack;uint64_t thread,event;
 uint32_t state,tid;
};
struct FileAsyncPool {
 uint64_t windows,imports[FA_IMPORT_COUNT],event,error,next_id;
 uint32_t sequence;FileAsyncWorker workers[4];
};
static long fa_sys(long number,long a,long b,long c,long d,long e,long f)
{
 register long r10 __asm__("r10")=d,r8 __asm__("r8")=e,r9 __asm__("r9")=f;
 long result;__asm__ volatile("syscall":"=a"(result):"a"(number),"D"(a),"S"(b),"d"(c),"r"(r10),"r"(r8),"r"(r9):"rcx","r11","memory","cc");return result;
}
static uint64_t fa_win(FileAsyncPool *p,int op,uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f,uint64_t g)
{
 uint64_t result=((WinCall)(uintptr_t)p->imports[op])(a,b,c,d,e,f,g);
 return op==FA_CREATE_THREAD||op==FA_CREATE_EVENT||op==FA_ALLOC||op==FA_OPEN?result:(uint32_t)result;
}
static uint64_t fa_error(FileAsyncPool *p){return fa_win(p,FA_ERROR,0,0,0,0,0,0,0);}
static void *fa_allocate(FileAsyncPool *p,uint64_t size)
{
 if(!size)size=1;
 if(p->windows)return (void *)(uintptr_t)fa_win(p,FA_ALLOC,0,size,0x3000,4,0,0,0);
 {long v=fa_sys(9,0,(long)size,3,0x22,-1,0);return (unsigned long)v>=(unsigned long)-4095?0:(void *)v;}
}
static void fa_free(FileAsyncPool *p,void *address,uint64_t size)
{if(!address)return;if(p->windows)fa_win(p,FA_FREE,(uint64_t)address,0,0x8000,0,0,0,0);else fa_sys(11,(long)address,(long)size,0,0,0,0);}
static void fa_copy(void *to,const void *from,uint64_t size)
{unsigned char *d=to;const unsigned char *s=from;uint64_t i;for(i=0;i<size;i++)d[i]=s[i];}
static uint32_t fa_load(uint32_t *p){return __atomic_load_n(p,__ATOMIC_ACQUIRE);}
static void fa_store(uint32_t *p,uint32_t value){__atomic_store_n(p,value,__ATOMIC_RELEASE);}
static void fa_wake(FileAsyncPool *p,FileAsyncWorker *w)
{if(p->windows)fa_win(p,FA_SET_EVENT,w->event,0,0,0,0,0,0);else fa_sys(202,(long)&w->state,129,1,0,0,0);}
static void fa_notify(FileAsyncPool *p)
{
 __atomic_add_fetch(&p->sequence,1,__ATOMIC_RELEASE);
 if(p->windows)fa_win(p,FA_SET_EVENT,p->event,0,0,0,0,0,0);
 else fa_sys(202,(long)&p->sequence,129,1,0,0,0);
}
static void fa_failure(FileAsyncJob *j,uint64_t step,uint64_t error,uint64_t code)
{j->step=step;j->error=error;j->code=code;}
static long fa_open(FileAsyncJob *j)
{
 FileAsyncPool *p=j->pool;long h;
 if(p->windows){
  h=(long)fa_win(p,FA_OPEN,(uint64_t)j->path,j->mode?0x40000000:0x80000000,7,0,j->mode?2:3,0x02000080,0);
  if(h==-1){fa_failure(j,1,fa_error(p),11);return -1;}
 }else{
  do{h=fa_sys(257,-100,(long)j->path,0x80800|(j->mode?(1|64|512):0),0666,0,0);}while(h==-4&&!fa_load(&j->cancel));
  if(h<0){if(h!=-4)fa_failure(j,1,(uint64_t)-h,11);return -1;}
 }
 return h;
}
static int fa_regular(FileAsyncJob *j,long h)
{
 FileAsyncPool *p=j->pool;uint64_t info[20]={0};long result;
 if(p->windows){
  result=(long)fa_win(p,FA_KIND,(uint64_t)h,0,0,0,0,0,0);
  if(result!=1){fa_failure(j,2,result?0:fa_error(p),5);return 0;}
  if(!fa_win(p,FA_INFO,(uint64_t)h,(uint64_t)info,0,0,0,0,0)){fa_failure(j,2,fa_error(p),11);return 0;}
  result=!(info[0]&16);
 }else{
  do{result=fa_sys(5,h,(long)info,0,0,0,0);}while(result==-4&&!fa_load(&j->cancel));
  if(result<0){if(result!=-4)fa_failure(j,2,(uint64_t)-result,11);return 0;}
  result=((info[3]&0xf000)==0x8000);
 }
 if(!result)fa_failure(j,2,0,5);
 return result!=0;
}
static long fa_transfer(FileAsyncJob *j,long h,void *data,uint64_t count)
{
 FileAsyncPool *p=j->pool;long result;
 if(p->windows){
  uint32_t moved=0;
  if(!fa_win(p,j->mode?FA_WRITE:FA_READ,(uint64_t)h,(uint64_t)data,count,(uint64_t)&moved,0,0,0)){
   fa_failure(j,j->mode?4:3,fa_error(p),11);return -1;
  }return moved;
 }
 do{result=fa_sys(j->mode?1:0,h,(long)data,(long)count,0,0,0);}while(result==-4&&!fa_load(&j->cancel));
 if(result<0&&result!=-4)fa_failure(j,j->mode?4:3,(uint64_t)-result,11);
 return result;
}
static int fa_grow(FileAsyncJob *j,uint64_t need)
{
 unsigned char *data;uint64_t cap=j->capacity?j->capacity:FA_CHUNK;
 if(need<=j->capacity)return 1;
 while(cap<need){if(cap>UINT64_MAX/2){cap=need;break;}cap*=2;}
 if(cap>j->limit)cap=j->limit;
 data=fa_allocate(j->pool,cap);if(!data){j->fatal=71;return 0;}
 fa_copy(data,j->data,j->length);fa_free(j->pool,j->data,j->capacity);j->data=data;j->capacity=cap;return 1;
}
static void fa_execute(FileAsyncJob *j)
{
 unsigned char buffer[FA_CHUNK];long h,n;uint64_t offset=0;
 if(fa_load(&j->cancel))return;
 h=fa_open(j);if(h==-1)return;
 if(fa_regular(j,h))while(!fa_load(&j->cancel)){
  if(j->mode){
   if(offset==j->length)break;
   n=fa_transfer(j,h,j->data+offset,j->length-offset<FA_CHUNK?j->length-offset:FA_CHUNK);
   if(n<=0){if(!n)fa_failure(j,4,0,11);break;}offset+=(uint64_t)n;
  }else{
   uint64_t count=j->limit-j->length;
   if(count>=FA_CHUNK)count=FA_CHUNK;else count++;
   n=fa_transfer(j,h,buffer,count);if(n<=0)break;
   if((uint64_t)n>j->limit-j->length){fa_failure(j,7,0,13);break;}
   if(!fa_grow(j,j->length+(uint64_t)n))break;
   fa_copy(j->data+j->length,buffer,(uint64_t)n);j->length+=(uint64_t)n;
  }
 }
 if(j->pool->windows){if(!fa_win(j->pool,FA_CLOSE,(uint64_t)h,0,0,0,0,0,0))j->close_error=fa_error(j->pool);}
 else{n=fa_sys(3,h,0,0,0,0,0);if(n<0)j->close_error=(uint64_t)-n;}
}
__attribute__((visibility("hidden"))) void file_worker_linux_entry(FileAsyncWorker *w);
void file_worker_linux_entry(FileAsyncWorker *w)
{
 FileAsyncPool *p=w->pool;
 for(;;){
  uint32_t state=fa_load(&w->state);
  if(state==3)return;
  if(state==1){fa_execute(w->job);fa_store(&w->state,2);fa_notify(p);continue;}
  if(p->windows)fa_win(p,FA_WAIT,w->event,0xffffffffu,0,0,0,0,0);
  else fa_sys(202,(long)&w->state,128,state,0,0,0);
 }
}
static uint32_t FA_MS fa_windows_entry(FileAsyncWorker *w)
{file_worker_linux_entry(w);return 0;}
__attribute__((visibility("hidden"))) long qyno_file_clone(void *,FileAsyncWorker *,uint32_t *);
__asm__(".text\n.hidden qyno_file_clone\n.type qyno_file_clone,@function\nqyno_file_clone:\n"
 "mov %rsi,(%rdi)\nmov %rdx,%r10\nmov %rdi,%rsi\nmov $0x350d00,%edi\nxor %r8d,%r8d\nmov $56,%eax\nsyscall\ntest %rax,%rax\njnz 1f\n"
 "mov (%rsp),%rdi\ncall file_worker_linux_entry\nxor %edi,%edi\nmov $60,%eax\nsyscall\nud2\n1: ret\n.size qyno_file_clone,.-qyno_file_clone\n");
static void fa_workers_stop(FileAsyncPool *p)
{
 int i;
 for(i=0;i<4;i++)if(p->workers[i].thread){fa_store(&p->workers[i].state,3);fa_wake(p,&p->workers[i]);}
 for(i=0;i<4;i++){
  FileAsyncWorker *w=&p->workers[i];
  if(w->thread){
   if(p->windows){fa_win(p,FA_WAIT,w->thread,0xffffffffu,0,0,0,0,0);fa_win(p,FA_CLOSE,w->thread,0,0,0,0,0,0);}
   else{uint32_t tid;while((tid=fa_load(&w->tid))!=0)fa_sys(202,(long)&w->tid,0,tid,0,0,0);}
   w->thread=0;
  }
  if(w->event){fa_win(p,FA_CLOSE,w->event,0,0,0,0,0,0);w->event=0;}
  if(w->stack){fa_free(p,w->stack,FA_STACK+4096);w->stack=0;}
 }
 if(p->event){fa_win(p,FA_CLOSE,p->event,0,0,0,0,0,0);p->event=0;}
}
FileAsyncPool *file_async_pool(uint64_t windows,const uint64_t *imports,uint64_t *error)
{
 FileAsyncPool initial={0},*p;int i;initial.windows=windows;
 for(i=0;i<FA_IMPORT_COUNT;i++)initial.imports[i]=imports[i];
 p=fa_allocate(&initial,sizeof(*p));if(!p){*error=0;return 0;}
 p->windows=windows;for(i=0;i<FA_IMPORT_COUNT;i++)p->imports[i]=imports[i];
 if(windows){p->event=fa_win(p,FA_CREATE_EVENT,0,0,0,0,0,0,0);if(!p->event){p->error=fa_error(p);goto failed;}}
 for(i=0;i<4;i++){
  FileAsyncWorker *w=&p->workers[i];w->pool=p;
  if(windows){
   w->event=fa_win(p,FA_CREATE_EVENT,0,0,0,0,0,0,0);if(!w->event){p->error=fa_error(p);goto failed;}
   w->thread=fa_win(p,FA_CREATE_THREAD,0,FA_STACK,(uint64_t)fa_windows_entry,(uint64_t)w,0,0,0);
   if(!w->thread){p->error=fa_error(p);goto failed;}
  }else{
   long thread;w->stack=fa_allocate(p,FA_STACK+4096);if(!w->stack){p->error=12;goto failed;}
   {long protect=fa_sys(10,(long)w->stack,4096,0,0,0,0);if(protect<0){p->error=(uint64_t)-protect;goto failed;}}
   thread=qyno_file_clone((unsigned char *)w->stack+FA_STACK+4096-16,w,&w->tid);
   if(thread<0){p->error=(uint64_t)-thread;goto failed;}w->thread=(uint64_t)thread;
  }
 }
 *error=0;return p;
 failed: *error=p->error;fa_workers_stop(p);return p;
}
uint64_t file_async_pool_error(FileAsyncPool *p){return p->error;}
/* Decode validated UTF-8 to a native UTF-16 path; no language heap involved. */
static unsigned char *fa_native_path(FileAsyncPool *p,const unsigned char *path,uint64_t length,uint64_t *size,uint64_t *error)
{
 unsigned char *out;
 if(!p->windows){
  uint64_t cap=4096,cwd=0;unsigned char *base=0;
  if(length&&path[0]!='/')for(;;){
   long n;base=fa_allocate(p,cap);if(!base)return 0;
   do{n=fa_sys(79,(long)base,(long)cap,0,0,0,0);}while(n==-4);
   if(n>=0){cwd=(uint64_t)n-1;break;}
   fa_free(p,base,cap);base=0;
   if(n!=-34){*error=(uint64_t)-n;return 0;}
   if(cap>UINT64_MAX/2)return 0;
   cap*=2;
  }
  if(length>UINT64_MAX-cwd-2){fa_free(p,base,cap);return 0;}
  *size=cwd+length+(cwd?2:1);out=fa_allocate(p,*size);
  if(out){if(cwd){fa_copy(out,base,cwd);out[cwd++]='/';}fa_copy(out+cwd,path,length);}
  fa_free(p,base,cap);return out;
 }else{
  uint16_t *wide,*full;uint64_t i=0,n=0;uint32_t c;uint64_t needed;
  if(length>UINT64_MAX/2-1)return 0;
  wide=fa_allocate(p,(length+1)*2);if(!wide)return 0;
  while(i<length){
   c=path[i++];if(c>=0xc0){unsigned more=c<0xe0?1:c<0xf0?2:3;c&=more==1?31:more==2?15:7;while(more--)c=(c<<6)|(path[i++]&63);}
   if(c>0xffff){c-=0x10000;wide[n++]=(uint16_t)(0xd800+(c>>10));wide[n++]=(uint16_t)(0xdc00+(c&1023));}else wide[n++]=(uint16_t)c;
  }
  if(!n || (n>=4&&wide[0]=='\\'&&wide[1]=='\\'&&(wide[2]=='?'||wide[2]=='.')&&wide[3]=='\\')){*size=(length+1)*2;return (unsigned char *)wide;}
  needed=fa_win(p,FA_FULL_PATH,(uint64_t)wide,0,0,0,0,0,0);
  if(!needed){*error=fa_error(p);fa_free(p,wide,(length+1)*2);return 0;}
  full=fa_allocate(p,needed*2);if(!full){fa_free(p,wide,(length+1)*2);return 0;}
  n=fa_win(p,FA_FULL_PATH,(uint64_t)wide,needed,(uint64_t)full,0,0,0,0);fa_free(p,wide,(length+1)*2);
  if(!n||n>=needed){*error=fa_error(p);fa_free(p,full,needed*2);return 0;}
  *size=needed*2;return (unsigned char *)full;
 }
}
FileAsyncJob *file_async_job(FileAsyncPool *p,const unsigned char *path,uint64_t length,uint64_t mode,uint64_t limit)
{
 FileAsyncJob *j=fa_allocate(p,sizeof(*j));if(!j)return 0;
 j->pool=p;j->id=++p->next_id;j->mode=mode;j->limit=limit;
 j->path=fa_native_path(p,path,length,&j->path_size,&j->error);
 if(!j->path){if(j->error){j->step=1;j->code=11;}else j->fatal=71;j->phase=2;}
 return j;
}
int file_async_reserve(FileAsyncJob *j,uint64_t bytes)
{
 int i;FileAsyncPool *p=j->pool;
 for(i=0;i<4;i++)if(fa_load(&p->workers[i].state)==0){
  FileAsyncWorker *w=&p->workers[i];w->job=j;j->worker=w;j->phase=4;fa_store(&w->state,4);
  if(j->mode){j->length=j->capacity=bytes;if(bytes){j->data=fa_allocate(p,bytes);if(!j->data)j->fatal=71;}}
  return 1;
 }return 0;
}
void file_async_launch(FileAsyncJob *j){j->phase=1;fa_store(&j->worker->state,1);fa_wake(j->pool,j->worker);}
void file_async_cancel(FileAsyncJob *j){fa_store(&j->cancel,1);}
int file_async_status(FileAsyncJob *j)
{if(j->phase==4)return 4;if(j->phase==2)return 2;return (int)fa_load(&j->worker->state);}
uint64_t file_async_slot(FileAsyncJob *j)
{return (uint64_t)(j->worker-j->pool->workers);}
void file_async_release(FileAsyncJob *j)
{
 FileAsyncPool *p=j->pool;FileAsyncWorker *w=j->worker;
 fa_free(p,j->data,j->capacity);fa_free(p,j->path,j->path_size);fa_free(p,j,sizeof(*j));
 if(w){w->job=0;fa_store(&w->state,0);}
}
uint32_t file_async_sequence(FileAsyncPool *p){return fa_load(&p->sequence);}
void file_async_wait(FileAsyncPool *p,uint32_t sequence,int64_t seconds,int64_t nanoseconds)
{
 if(file_async_sequence(p)!=sequence)return;
 if(p->windows){uint64_t millis;
  if(seconds<0)millis=0xffffffffu;
  else if(seconds>4294966)millis=0xfffffffeu;
  else millis=(uint64_t)seconds*1000+((uint64_t)nanoseconds+999999)/1000000;
  fa_win(p,FA_WAIT,p->event,millis,0,0,0,0,0);
 }else{int64_t duration[2]={seconds,nanoseconds};fa_sys(202,(long)&p->sequence,128,sequence,seconds<0?0:(long)duration,0,0);}
}
void file_async_stop(FileAsyncPool *p){if(p){fa_workers_stop(p);fa_free(p,p,sizeof(*p));}}
