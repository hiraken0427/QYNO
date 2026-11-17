/* Single-threaded cooperative tasks. All retained state, including suspended
 * stacks, is allocated by the language GC. No libc or writable globals. */
#include "async_runtime.h"

void qyno_async_swap(AsyncContext *,AsyncContext *);
__attribute__((visibility("hidden"))) void qyno_async_boot(void);
__asm__(
".text\n"
".type qyno_async_swap,@function\nqyno_async_swap:\n"
"mov %rsp,0(%rdi)\nmov %rbp,8(%rdi)\nmov %rbx,16(%rdi)\n"
"mov %r12,24(%rdi)\nmov %r13,32(%rdi)\nmov %r14,40(%rdi)\nmov %r15,48(%rdi)\n"
"lea 1f(%rip),%rax\nmov %rax,56(%rdi)\n"
"mov 0(%rsi),%rsp\nmov 8(%rsi),%rbp\nmov 16(%rsi),%rbx\n"
"mov 24(%rsi),%r12\nmov 32(%rsi),%r13\nmov 40(%rsi),%r14\nmov 48(%rsi),%r15\n"
"jmp *56(%rsi)\n1: ret\n.size qyno_async_swap,.-qyno_async_swap\n"
".type qyno_async_boot,@function\nqyno_async_boot:\nmov %rbx,%rdi\njmp qyno_async_start\n.size qyno_async_boot,.-qyno_async_boot\n");
static void zero(void *p,uint64_t size)
{ uint64_t *v=p;while(size>=8){*v++=0;size-=8;} }
static uint64_t *pending(AsyncRuntime *r) { return r->state+304/8; }
static int done(AsyncTask *t) { return t->state>=4; }
static void io_save(AsyncRuntime *r,AsyncTask *t)
{ int i;if(t)for(i=0;i<11;i++)t->io[i]=r->state[32+i]; }
static void io_load(AsyncRuntime *r,AsyncTask *t)
{ int i;for(i=0;i<11;i++)r->state[32+i]=t?t->io[i]:0; }
static void enqueue(AsyncRuntime *r,AsyncTask *t)
{
    if(t->queued||done(t))return;
    t->queued=1;t->state=0;t->queue_next=0;
    if(r->last)r->last->queue_next=t;else r->first=t;
    r->last=t;
}
static void detach(AsyncRuntime *r,AsyncTask *t)
{
    AsyncTask **p=&r->pending,*previous=0;
    while(*p&&*p!=t){previous=*p;p=&(*p)->pending_next;}
    if(*p){*p=t->pending_next;if(r->pending_last==t)r->pending_last=previous;}
    t->pending_next=0;
}
static void finish(AsyncTask *t)
{ t->state=t->error?5:4;detach(t->runtime,t); }
static uint64_t error_record(AsyncRuntime *r,uint64_t code,uint64_t *message)
{
    uint64_t *e=r->allocate(r,72),*fields,*errors;
    e[0]=e[1]=6;e[7]=1;
    fields=r->allocate(r,48);e[3]=(uint64_t)fields;
    fields[0]=code;fields[1]=(uint64_t)message;
    fields[2]=r->state[312/8];fields[3]=r->state[328/8];fields[4]=r->state[336/8];
    if(!fields[2])fields[2]=(uint64_t)r->empty_string;
    errors=r->allocate(r,40);fields[5]=(uint64_t)errors;
    e[6]=r->state[264/8];return (uint64_t)e;
}
static void suspend(AsyncTask *t)
{ qyno_async_swap(&t->context,t->caller); }
static void run(AsyncRuntime *r,AsyncTask *t)
{
    AsyncContext caller;
    AsyncTask *parent=r->current;
    uint64_t outer_io[11];int i;
    for(i=0;i<11;i++)outer_io[i]=r->state[32+i];
    io_save(r,parent);io_load(r,t);r->current=t;t->caller=&caller;t->state=1;
    qyno_async_swap(&caller,&t->context);
    io_save(r,t);
    r->current=parent;
    for(i=0;i<11;i++)r->state[32+i]=outer_io[i];
    t->caller=0;
    if(done(t)){t->stack=0;t->stack_top=0;zero(&t->context,sizeof(t->context));}
}
static int precedes(AsyncInstant a,AsyncInstant b)
{ return a.seconds<b.seconds || (a.seconds==b.seconds && a.nanoseconds<b.nanoseconds); }
static void wait_for(AsyncRuntime *r,AsyncTask *target)
{
    AsyncTask *self=r->current;
    if(target==self){*pending(r)=error_record(r,4,r->invalid_message);return;}
    /* A task cannot wait for an ancestor whose completion owns this task. */
    {AsyncTask *p=self;while(p){if(p==target){*pending(r)=error_record(r,4,r->invalid_message);return;}p=p->parent;}}
    while(!done(target)){self->waiting=target;self->state=2;suspend(self);if(self->error){*pending(r)=self->error;self->waiting=0;return;}}
    self->waiting=0;target->observed=1;
    if(target->error)*pending(r)=target->error;
}
__attribute__((used,noinline,visibility("hidden"))) void qyno_async_start(AsyncTask *self)
{
    AsyncRuntime *r=self->runtime;AsyncTask *child;
    self->result=self->invoke(r,self);
    self->error=*pending(r);*pending(r)=0;
    /* Implicit ownership keeps discarded/overwritten handles and descendants
     * alive. Explicit groups and cancellation are added by the next stage. */
    for(child=self->children;child;child=child->sibling){
        if(!done(child)){self->waiting=child;self->state=2;suspend(self);while(!done(child)){self->state=2;suspend(self);}}
        self->waiting=0;
        if(child->error&&!child->observed&&!self->error){self->error=child->error;child->observed=1;}
    }
    self->children=self->last_child=0;
    *pending(r)=0;finish(self);suspend(self);
    for(;;)suspend(self);
}
static AsyncTask *create(AsyncRequest *q)
{
    AsyncRuntime *r=q->runtime;AsyncTask *t=r->allocate(r,sizeof(*t));uint64_t i;
    t->runtime=r;t->invoke=q->invoke;t->nargs=q->nargs;t->parent=r->current;
    if(r->pending_last)r->pending_last->pending_next=t;else r->pending=t;
    r->pending_last=t;
    if(t->parent){if(t->parent->last_child)t->parent->last_child->sibling=t;else t->parent->children=t;t->parent->last_child=t;}
    else r->root=t;
    if(q->op==ASYNC_CREATE){
        if(q->nargs){t->args=r->allocate(r,q->nargs*8);for(i=0;i<q->nargs;i++)t->args[i]=q->args[q->nargs-1-i];}
        t->stack=r->allocate(r,ASYNC_STACK_BYTES);
        t->stack_top=((uint64_t)t->stack+ASYNC_STACK_BYTES)&~UINT64_C(15);
        t->context.rsp=t->stack_top-8;t->context.rbx=(uint64_t)t;t->context.rip=(uint64_t)qyno_async_boot;
        run(r,t);
    }else{
        t->kind=q->op;t->state=2;t->epoch=r->epoch+1;
        if(q->op==ASYNC_DELAY){
            AsyncInstant now;r->clock(r,&now);
            if(*pending(r)){t->error=*pending(r);*pending(r)=0;finish(t);return t;}
            /* T_SLEEP validation limits durations; saturate unrepresentable
             * absolute deadlines instead of overflowing signed arithmetic. */
            if(q->delay.seconds>INT64_MAX-now.seconds)t->deadline.seconds=INT64_MAX;
            else t->deadline.seconds=now.seconds+q->delay.seconds;
            t->deadline.nanoseconds=now.nanoseconds+q->delay.nanoseconds;
            if(t->deadline.nanoseconds>=1000000000){t->deadline.nanoseconds-=1000000000;if(t->deadline.seconds<INT64_MAX)t->deadline.seconds++;}
        }
    }
    return t;
}
static void pump(AsyncRuntime *r)
{
    AsyncTask *t,*next;AsyncInstant now,earliest={INT64_MAX,999999999};int timers=0,has_deadline=0;
    r->epoch++;
    /* Check timers even while runnable tasks exist, so yielding work cannot
     * starve a timer. Clock errors fail the waiting timer tasks. */
    for(t=r->pending;t;t=t->pending_next)if(t->kind==ASYNC_DELAY){timers=1;break;}
    if(timers)r->clock(r,&now);
    for(t=r->pending;t;t=next){
        next=t->pending_next;
        if(t->kind==ASYNC_YIELD && t->epoch<=r->epoch)finish(t);
        else if(t->kind==ASYNC_DELAY){
            if(*pending(r)){t->error=*pending(r);finish(t);}
            else if(!precedes(now,t->deadline))finish(t);
            else if(!has_deadline || precedes(t->deadline,earliest)){earliest=t->deadline;has_deadline=1;}
        }
    }
    *pending(r)=0;
    for(t=r->pending;t;t=t->pending_next)if(t->state==2&&t->waiting&&done(t->waiting))enqueue(r,t);
    if(r->first){t=r->first;r->first=t->queue_next;if(!r->first)r->last=0;t->queue_next=0;t->queued=0;run(r,t);return;}
    if(timers && has_deadline){
        AsyncInstant delay={earliest.seconds-now.seconds,earliest.nanoseconds-now.nanoseconds};
        if(delay.nanoseconds<0){delay.seconds--;delay.nanoseconds+=1000000000;}
        r->sleep(r,&delay);
        if(*pending(r)){for(t=r->pending;t;t=t->pending_next)if(t->kind==ASYNC_DELAY){t->error=*pending(r);t->deadline=now;}*pending(r)=0;}
    }else if(r->pending){
        /* Waiting cycles are errors, never an event-loop busy wait. */
        uint64_t error=error_record(r,4,r->invalid_message);
        for(t=r->pending;t;t=t->pending_next)if(t->waiting){t->waiting=0;t->error=error;enqueue(r,t);}
    }
}
__attribute__((visibility("hidden"))) uint64_t qyno_async(AsyncRequest *q)
{
    AsyncRuntime *r=q->runtime;
    if(q->op==ASYNC_CREATE||q->op==ASYNC_DELAY||q->op==ASYNC_YIELD)return (uint64_t)create(q);
    if(q->op==ASYNC_AWAIT){
        uint64_t context=r->state[264/8];
        wait_for(r,q->task);
        if(r->current->error && !*pending(r))*pending(r)=r->current->error;
        if(*pending(r) && context){
            uint64_t *original=(uint64_t *)*pending(r),*copy=r->allocate(r,72),*trace;int i;
            for(i=0;i<9;i++)copy[i]=original[i];
            trace=r->allocate(r,16);trace[0]=context;trace[1]=copy[8];copy[8]=(uint64_t)trace;
            *pending(r)=(uint64_t)copy;
        }
        return q->task->result;
    }
    while(!done(q->task))pump(r);
    if(q->task->error)*pending(r)=q->task->error;
    return q->task->result;
}
