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
/* All links and error trees are GC-visible. The engine has no writable globals. */
static void zero(void *p,uint64_t n){uint64_t *v=p;while(n>=8){*v++=0;n-=8;}}
static uint64_t *pending(AsyncRuntime *r){return r->state+38;}
static int done(AsyncTask *t){return t->state>=4;}
static int cancelled(uint64_t error)
{uint64_t *e=(uint64_t *)error;return e&&e[7]==1&&((uint64_t *)e[3])[0]==1;}
static void site_save(AsyncRuntime *r,uint64_t *s)
{s[0]=r->state[33];s[1]=r->state[39];s[2]=r->state[41];s[3]=r->state[42];}
static void io_save(AsyncRuntime *r,AsyncTask *t){int i;if(t)for(i=0;i<11;i++)t->io[i]=r->state[32+i];}
static void io_load(AsyncRuntime *r,AsyncTask *t){int i;for(i=0;i<11;i++)r->state[32+i]=t?t->io[i]:0;}
static uint64_t *array_new(AsyncRuntime *r,uint64_t count,uint64_t *shape)
{
    uint64_t *a=r->allocate(r,40),*data=0;
    a[0]=a[1]=count;a[2]=(uint64_t)shape;
    if(count){data=r->allocate(r,count*8);a[3]=(uint64_t)data;}
    return a;
}
/* Hidden slot 72 points to related errors, request identity/reason,
 * canonical view identity, and the cached presence of ordinary failures. */
static uint64_t error_record(AsyncRuntime *r,uint64_t code,uint64_t *message,uint64_t *site)
{
    uint64_t *e=r->allocate(r,80),*f,*meta,*errors;
    e[0]=e[1]=6;e[7]=1;
    f=r->allocate(r,48);e[3]=(uint64_t)f;f[0]=code;f[1]=(uint64_t)message;
    if(site){e[6]=site[0];f[2]=site[1];f[3]=site[2];f[4]=site[3];}
    else{e[6]=r->state[33];f[2]=r->state[39];f[3]=r->state[41];f[4]=r->state[42];}
    if(!f[2])f[2]=(uint64_t)r->empty_string;
    errors=array_new(r,0,r->error_shape);f[5]=(uint64_t)errors;
    meta=r->allocate(r,40);meta[0]=(uint64_t)errors;meta[3]=(uint64_t)e;meta[4]=code!=1;e[9]=(uint64_t)meta;
    return (uint64_t)e;
}
static uint64_t cancellation(AsyncRuntime *r,uint64_t *site,int reason)
{
    uint64_t e=error_record(r,1,r->cancel_message,site),*meta=(uint64_t *)((uint64_t *)e)[9];
    meta[1]=++r->request_sequence;meta[2]=(uint64_t)reason;return e;
}
static uint64_t error_clone(AsyncRuntime *r,uint64_t error)
{uint64_t *copy=r->allocate(r,80),*e=(uint64_t *)error;int i;for(i=0;i<10;i++)copy[i]=e[i];
 if(!copy[9]){uint64_t *meta=r->allocate(r,40);meta[3]=error;meta[4]=!cancelled(error);copy[9]=(uint64_t)meta;}return (uint64_t)copy;}
static uint64_t *related(AsyncRuntime *r,uint64_t error)
{
    uint64_t *e=(uint64_t *)error,*meta=(uint64_t *)e[9];
    if(meta&&meta[0])return (uint64_t *)meta[0];
    if(e[7])return (uint64_t *)((uint64_t *)e[3])[5];
    return array_new(r,0,r->error_shape);
}
static uint64_t common_error(AsyncRuntime *r,uint64_t error)
{
    uint64_t *e=(uint64_t *)error,*source=(uint64_t *)e[3],*v=r->allocate(r,48),*f;int i;
    v[0]=v[1]=7;v[5]=e[9]?((uint64_t *)e[9])[3]:error;f=r->allocate(r,56);v[3]=(uint64_t)f;
    f[0]=source[0]|(e[7]<<16);f[1]=source[1];
    for(i=0;i<3;i++)f[2+i]=source[(e[7]?2:5)+i];
    f[5]=(uint64_t)(e[7]?r->task_domain:r->io_domain);f[6]=(uint64_t)related(r,error);return (uint64_t)v;
}
static int has_failure(uint64_t error)
{
    uint64_t *e=(uint64_t *)error,*meta;
    if(!e)return 0;
    if(!cancelled(error))return 1;
    meta=(uint64_t *)e[9];return meta&&meta[4];
}
static uint64_t with_related(AsyncRuntime *r,uint64_t error,uint64_t *raw,uint64_t count)
{
    uint64_t copy=error_clone(r,error),*e=(uint64_t *)copy,*old=related(r,error),*items,*a,*meta,*fields;uint64_t i;
    a=array_new(r,old[0]+count,r->error_shape);items=(uint64_t *)a[3];
    for(i=0;i<old[0];i++)items[i]=((uint64_t *)old[3])[i];
    for(i=0;i<count;i++)items[old[0]+i]=common_error(r,raw[i]);
    meta=r->allocate(r,40);if(e[9]){uint64_t *previous=(uint64_t *)e[9];meta[1]=previous[1];meta[2]=previous[2];meta[3]=previous[3];meta[4]=previous[4];}
    meta[3]=copy;meta[4]=has_failure(error);
    for(i=0;i<count;i++)if(has_failure(raw[i]))meta[4]=1;
    meta[0]=(uint64_t)a;e[9]=(uint64_t)meta;
    if(e[7]){fields=r->allocate(r,48);for(i=0;i<6;i++)fields[i]=((uint64_t *)e[3])[i];fields[5]=(uint64_t)a;e[3]=(uint64_t)fields;}
    return copy;
}
static void enqueue(AsyncRuntime *r,AsyncTask *t)
{
    if(t->queued||done(t)||t->executing||!t->invoke)return;
    t->queued=1;t->state=t->cancel_error?3:0;t->queue_next=0;
    if(r->last)r->last->queue_next=t;else r->first=t;r->last=t;
}
static int within(AsyncGroup *g,AsyncGroup *outer)
{for(;g;g=g->parent)if(g==outer)return 1;return 0;}
static void detach(AsyncRuntime *r,AsyncTask *t)
{
    AsyncTask **p=&r->pending,*previous=0;
    while(*p&&*p!=t){previous=*p;p=&(*p)->pending_next;}
    if(*p){*p=t->pending_next;if(r->pending_last==t)r->pending_last=previous;}t->pending_next=0;
}
static void finish(AsyncTask *t)
{
    AsyncRuntime *r=t->runtime;AsyncWaiter *w;
    t->state=t->error?(cancelled(t->error)?6:5):4;detach(r,t);
    for(w=t->waiters;w;w=w->next)if(w->active){w->active=0;enqueue(r,w->waiter);}
    t->waiters=0;
    if(t->group && t->group->owner->waiting_group && within(t->group,t->group->owner->waiting_group))enqueue(r,t->group->owner);
}
/* Once a result is observed, lexical handles and supervisors carry its
 * remaining lifetime; ownership lists must not retain every loop iteration. */
static void observed(AsyncTask *t)
{
    AsyncTask *parent=t->parent;AsyncGroup *g=t->group;
    if(t->observed)return;
    t->observed=1;if(!done(t)||!g||!parent)return;
    if(t->group_prev)t->group_prev->group_next=t->group_next;else g->first=t->group_next;
    if(t->group_next)t->group_next->group_prev=t->group_prev;else g->last=t->group_prev;
    if(t->sibling_prev)t->sibling_prev->sibling=t->sibling;else parent->children=t->sibling;
    if(t->sibling)t->sibling->sibling_prev=t->sibling_prev;else parent->last_child=t->sibling_prev;
    t->group_prev=t->group_next=t->sibling_prev=t->sibling=0;
}
static void cancel_add(AsyncTask **head,AsyncTask **tail,AsyncTask *t,uint64_t error)
{
    if(done(t)||t->cancel_error)return;
    t->cancel_error=error;t->state=3;t->cancel_next=0;
    if(*tail)(*tail)->cancel_next=t;else *head=t;*tail=t;
}
static int request_cancel(AsyncTask *t,uint64_t error)
{
    AsyncTask *head=0,*tail=0;
    if(done(t)||t->cancel_error)return 0;
    cancel_add(&head,&tail,t,error);
    while(head){
        AsyncTask *current=head,*child;uint64_t i;
        head=current->cancel_next;if(!head)tail=0;current->cancel_next=0;
        for(child=current->children;child;child=child->sibling)cancel_add(&head,&tail,child,error);
        if(current->root_group)current->root_group->cancel_error=error;
        for(i=0;i<current->input_count;i++)cancel_add(&head,&tail,current->inputs[i],error);
        if(!current->invoke&&(current->kind==ASYNC_DELAY||current->kind==ASYNC_YIELD)){current->error=error;finish(current);}
        else {if(current->kind==ASYNC_FILE&&current->io_job)file_async_cancel(current->io_job);enqueue(current->runtime,current);}
    }
    return 1;
}
#include "../file-io/file_async_tasks.inc"

static void cancel_group(AsyncGroup *g,uint64_t error)
{
    AsyncTask *t;AsyncGroup *inner;
    if(!g->cancel_error)g->cancel_error=error;
    for(inner=g->owner->current_group;inner&&within(inner,g);inner=inner->parent)if(!inner->cancel_error)inner->cancel_error=error;
    for(t=g->owner->children;t;t=t->sibling)if(within(t->group,g))request_cancel(t,g->cancel_error);
}
static uint64_t checkpoint(AsyncRuntime *r)
{
    AsyncTask *t=r->current;
    if(t&&t->cancel_error&&!t->cleanup_mask){*pending(r)=t->cancel_error;return t->cancel_error;}return 0;
}
static void suspend(AsyncTask *t){qyno_async_swap(&t->context,t->caller);}
static void run(AsyncRuntime *r,AsyncTask *t)
{
    AsyncContext caller;AsyncTask *parent=r->current;uint64_t outer[11];int i;
    for(i=0;i<11;i++)outer[i]=r->state[32+i];
    io_save(r,parent);io_load(r,t);
    r->current=t;t->caller=&caller;t->executing=1;t->state=t->cancel_error?3:1;
    qyno_async_swap(&caller,&t->context);io_save(r,t);t->executing=0;r->current=parent;
    for(i=0;i<11;i++)r->state[32+i]=outer[i];
    t->caller=0;
    if(done(t)){t->stack=0;t->stack_top=0;zero(&t->context,sizeof(t->context));}
}
static void visit_add(AsyncTask **head,AsyncTask **tail,AsyncTask *t,uint64_t stamp)
{
    if(!t||t->visit_stamp==stamp)return;
    t->visit_stamp=stamp;t->visit_next=0;
    if(*tail)(*tail)->visit_next=t;else *head=t;*tail=t;
}
static int wait_reaches(AsyncTask *from,AsyncTask *to)
{
    AsyncRuntime *r=to->runtime;AsyncTask *head=0,*tail=0;uint64_t stamp=++r->visit_sequence;int found=0;
    if(!stamp){AsyncTask *t;for(t=r->pending;t;t=t->pending_next)t->visit_stamp=0;stamp=++r->visit_sequence;}
    visit_add(&head,&tail,from,stamp);
    while(head){
        AsyncTask *current=head;uint64_t i;
        head=current->visit_next;if(!head)tail=0;current->visit_next=0;
        if(current==to)found=1;
        if(found||done(current))continue;
        visit_add(&head,&tail,current->waiting,stamp);
        for(i=0;i<current->input_count;i++)visit_add(&head,&tail,current->inputs[i],stamp);
        if(current->waiting_group){AsyncTask *child;for(child=current->waiting_group->first;child;child=child->group_next)if(!done(child))visit_add(&head,&tail,child,stamp);}
    }
    return found;
}
static void unwait(AsyncTask *self,AsyncTask *target,AsyncWaiter *link)
{
    AsyncWaiter **p=&target->waiters;
    while(*p&&*p!=link)p=&(*p)->next;
    if(*p)*p=link->next;
    link->active=0;link->waiter=0;link->next=0;self->wait_link=0;self->waiting=0;
}
static void wait_for(AsyncRuntime *r,AsyncTask *target)
{
    AsyncTask *self=r->current,*p;AsyncWaiter *link;
    if(checkpoint(r))return;
    for(p=self;p;p=p->parent)if(p==target){*pending(r)=error_record(r,4,r->invalid_message,0);return;}
    if(!done(target)&&wait_reaches(target,self)){*pending(r)=error_record(r,4,r->invalid_message,0);return;}
    if(!done(target)){
        link=r->allocate(r,sizeof(*link));link->waiter=self;link->active=1;link->next=target->waiters;target->waiters=link;self->wait_link=link;
        self->waiting=target;
        while(!done(target)){
            self->state=self->cancel_error?3:2;suspend(self);
            if(checkpoint(r)){unwait(self,target,link);return;}
            if(self->primary){*pending(r)=self->primary;self->primary=0;break;}
        }
        unwait(self,target,link);
    }
    if(checkpoint(r))return;
    observed(target);if(target->error)*pending(r)=target->error;
}
static AsyncGroup *group_new(AsyncRuntime *r,AsyncTask *owner,AsyncGroup *parent)
{
    AsyncGroup *g=r->allocate(r,sizeof(*g));g->owner=owner;g->parent=parent;site_save(r,g->site);
    if(parent)g->cancel_error=parent->cancel_error;
    return g;
}
static int represented(AsyncTask *t,AsyncGroup *scope)
{
    AsyncLink *link;
    for(link=t->representatives;link;link=link->next){
        AsyncTask *wrapper;for(wrapper=link->wrapper;wrapper;wrapper=wrapper->parent)if(within(wrapper->group,scope))return 1;
    }
    return 0;
}
static uint64_t close_group(AsyncRuntime *r,AsyncGroup *g,uint64_t primary)
{
    AsyncTask *self=r->current,*t;uint64_t *errors,count=0,cancels=0,result=primary;int active,failure;
    if(g->closed)return g->error;
    g->closing=1;g->primary=primary;self->cleanup_mask++;
    for(;;){
        if(!g->primary&&self->cancel_error)g->primary=self->cancel_error;
        failure=0;active=0;
        for(t=g->first;t;t=t->group_next){
            uint64_t error=done(t)?t->error:t->primary;
            if(!done(t))active=1;
            if(has_failure(error)&&!t->observed&&!represented(t,g))failure=1;
        }
        if(g->primary||failure){
            uint64_t cause=g->primary&&cancelled(g->primary)?g->primary:cancellation(r,g->site,2);
            cancel_group(g,cause);
        }
        if(!active)break;
        self->waiting_group=g;self->state=self->cancel_error?3:2;suspend(self);self->waiting_group=0;
    }
    for(t=g->first;t;t=t->group_next)if(t->error&&!t->observed&&!represented(t,g)){if(has_failure(t->error))count++;else cancels++;}
    errors=r->allocate(r,(count+cancels+1)*8);count=0;
    for(t=g->first;t;t=t->group_next)if(t->error&&!t->observed&&!represented(t,g)&&has_failure(t->error))errors[count++]=t->error;
    result=g->primary;
    if(result){if(count)result=with_related(r,result,errors,count);}
    else if(count){result=error_record(r,3,r->group_message,g->site);result=with_related(r,result,errors,count);}
    else if(cancels){
        for(t=g->first;t;t=t->group_next)if(t->error&&!t->observed&&!represented(t,g)&&!has_failure(t->error))errors[count++]=t->error;
        result=error_record(r,1,r->cancel_message,g->site);result=with_related(r,result,errors,count);
    }
    while(g->first)observed(g->first);
    g->first=g->last=0;g->closed=1;g->error=result;self->cleanup_mask--;
    if(self->current_group==g)self->current_group=g->parent;
    return result;
}
__attribute__((used,noinline,visibility("hidden"))) void qyno_async_start(AsyncTask *self)
{
    AsyncRuntime *r=self->runtime;uint64_t error;
    self->result=self->invoke(r,self);error=*pending(r);*pending(r)=0;
    self->error=close_group(r,self->root_group,error);*pending(r)=0;finish(self);suspend(self);
    for(;;)suspend(self);
}
static void inputs_observed(AsyncTask *t)
{
    uint64_t i;
    for(i=0;i<t->input_count;i++){
        AsyncTask *input=t->inputs[i];AsyncLink **p=&input->representatives;
        observed(input);
        while(*p){AsyncLink *link=*p;if(link->wrapper==t){*p=link->next;link->next=0;link->wrapper=0;}else p=&link->next;}
    }
    t->inputs=0;t->input_count=0;
}
static int first_input(AsyncTask *t,uint64_t i)
{uint64_t j;for(j=0;j<i;j++)if(t->inputs[j]==t->inputs[i])return 0;return 1;}
static int before(AsyncInstant now,AsyncDeadline deadline)
{return (uint64_t)now.seconds<deadline.seconds || ((uint64_t)now.seconds==deadline.seconds&&(uint64_t)now.nanoseconds<deadline.nanoseconds);}
static void deadline_set(AsyncTask *t,AsyncInstant now,AsyncInstant delay)
{
    t->deadline.seconds=(uint64_t)now.seconds+(uint64_t)delay.seconds;
    t->deadline.nanoseconds=(uint64_t)now.nanoseconds+(uint64_t)delay.nanoseconds;
    if(t->deadline.nanoseconds>=1000000000){t->deadline.seconds++;t->deadline.nanoseconds-=1000000000;}
}
static void poll_composite(AsyncTask *t,AsyncInstant *now)
{
    AsyncRuntime *r=t->runtime;uint64_t i,count=0,cancels=0,*raw,*a;int active=0;
    if(done(t))return;
    if(t->kind==ASYNC_TIMEOUT){
        AsyncTask *target=t->inputs[0];
        if(!t->timeout_reason && done(target)){
            t->result=target->result;t->error=target->error;inputs_observed(t);finish(t);return;
        }
        if(!t->timeout_reason && (t->cancel_error||target->cancel_error)){t->timeout_reason=2;if(!t->cancel_error)t->cancel_error=target->cancel_error;}
        if(t->primary && !t->timeout_reason){t->timeout_reason=3;request_cancel(target,cancellation(r,t->site,2));}
        if(!t->timeout_reason && now && !before(*now,t->deadline)){
            t->timeout_reason=1;t->primary=error_record(r,2,r->timeout_message,t->site);
            if(t->group&&t->group->owner->waiting_group&&within(t->group,t->group->owner->waiting_group))enqueue(r,t->group->owner);
            request_cancel(target,cancellation(r,t->site,3));
        }
        if(t->timeout_reason && !done(target))return;
        if(done(target)){
            t->result=target->result;
            t->error=t->timeout_reason==1||t->timeout_reason==3?t->primary:t->cancel_error;
            if(!t->error)t->error=target->error;
            if(has_failure(target->error)&&t->error!=target->error)t->error=with_related(r,t->error,&target->error,1);
            inputs_observed(t);finish(t);
        }
        return;
    }
    for(i=0;i<t->input_count;i++){
        AsyncTask *target=t->inputs[i];if(!done(target))active=1;
        else if(target->error&&first_input(t,i)){if(has_failure(target->error))count++;else cancels++;}
    }
    if(count && !t->primary){
        for(i=0;i<t->input_count;i++)if(has_failure(t->inputs[i]->error)){t->primary=t->inputs[i]->error;break;}
        if(t->group&&t->group->owner->waiting_group&&within(t->group,t->group->owner->waiting_group))enqueue(r,t->group->owner);
    }
    if(count && active){
        uint64_t cause=cancellation(r,t->site,2);
        for(i=0;i<t->input_count;i++)request_cancel(t->inputs[i],cause);
        active=0;for(i=0;i<t->input_count;i++)if(!done(t->inputs[i]))active=1;
    }
    if(active)return;
    raw=r->allocate(r,(t->input_count+1)*8);count=0;cancels=0;
    for(i=0;i<t->input_count;i++)if(first_input(t,i)&&t->inputs[i]->error){
        if(has_failure(t->inputs[i]->error))raw[count++]=t->inputs[i]->error;else cancels++;
    }
    if(count){t->error=error_record(r,3,r->group_message,t->site);t->error=with_related(r,t->error,raw,count);}
    else if(t->cancel_error)t->error=t->cancel_error;
    else if(cancels){
        for(i=0;i<t->input_count;i++)if(first_input(t,i)&&t->inputs[i]->error&&!has_failure(t->inputs[i]->error))raw[count++]=t->inputs[i]->error;
        t->error=error_record(r,1,r->cancel_message,t->site);t->error=with_related(r,t->error,raw,count);
    }else{
        a=array_new(r,t->input_count,t->shape);t->result=(uint64_t)a;
        for(i=0;i<t->input_count;i++)((uint64_t *)a[3])[i]=t->inputs[i]->result;
    }
    inputs_observed(t);finish(t);
}
static AsyncTask *create(AsyncRequest *q)
{
    AsyncRuntime *r=q->runtime;AsyncTask *t=r->allocate(r,sizeof(*t));uint64_t i;AsyncInstant now;
    t->runtime=r;t->kind=q->op;t->invoke=q->invoke;t->nargs=q->nargs;t->parent=r->current;site_save(r,t->site);
    if(r->pending_last)r->pending_last->pending_next=t;else r->pending=t;r->pending_last=t;
    if(t->parent){
        AsyncTask *p=t->parent;t->group=p->current_group;t->sibling_prev=p->last_child;t->group_prev=t->group->last;
        if(p->last_child)p->last_child->sibling=t;else p->children=t;p->last_child=t;
        if(t->group->last)t->group->last->group_next=t;else t->group->first=t;t->group->last=t;
        if(p->cancel_error)t->cancel_error=p->cancel_error;
        else if(t->group->cancel_error)t->cancel_error=t->group->cancel_error;
    }else r->root=t;
    if(q->op==ASYNC_FILE){io_submit(r,t,q);}
    else if(q->op==ASYNC_CREATE){
        t->root_group=group_new(r,t,0);t->current_group=t->root_group;
        if(t->cancel_error){t->error=t->cancel_error;finish(t);return t;}
        if(q->nargs){t->args=r->allocate(r,q->nargs*8);for(i=0;i<q->nargs;i++)t->args[i]=q->args[q->nargs-1-i];}
        t->stack=r->allocate(r,ASYNC_STACK_BYTES);t->stack_top=((uint64_t)t->stack+ASYNC_STACK_BYTES)&~UINT64_C(15);
        t->context.rsp=t->stack_top-8;t->context.rbx=(uint64_t)t;t->context.rip=(uint64_t)qyno_async_boot;run(r,t);
    }else if(q->op==ASYNC_ALL||q->op==ASYNC_TIMEOUT){
        uint64_t *source=q->args;
        t->input_count=q->op==ASYNC_TIMEOUT?1:q->nargs;t->shape=q->shape;t->state=2;
        if(q->op==ASYNC_ALL && q->flags){t->input_count=source[0];source=(uint64_t *)source[3];}
        if(t->input_count)t->inputs=r->allocate(r,t->input_count*sizeof(*t->inputs));
        for(i=0;i<t->input_count;i++){
            AsyncLink *link;
            t->inputs[i]=q->op==ASYNC_TIMEOUT?q->task:(AsyncTask *)source[q->flags?i:t->input_count-1-i];
            link=r->allocate(r,sizeof(*link));link->wrapper=t;link->next=t->inputs[i]->representatives;t->inputs[i]->representatives=link;
        }
        if(t->cancel_error)for(i=0;i<t->input_count;i++)request_cancel(t->inputs[i],t->cancel_error);
        if(q->op==ASYNC_TIMEOUT){
            if(done(t->inputs[0]))poll_composite(t,0);
            else{
                r->clock(r,&now);
                if(*pending(r)){t->primary=*pending(r);*pending(r)=0;}
                else deadline_set(t,now,q->delay);
                poll_composite(t,&now);
            }
        }else poll_composite(t,0);
    }else{
        t->state=2;t->epoch=r->epoch+1;
        if(t->cancel_error){t->error=t->cancel_error;finish(t);return t;}
        if(q->op==ASYNC_DELAY){
            r->clock(r,&now);if(*pending(r)){t->error=*pending(r);*pending(r)=0;finish(t);return t;}
            deadline_set(t,now,q->delay);
        }
    }
    return t;
}
static void pump(AsyncRuntime *r)
{
    AsyncTask *t,*next;AsyncInstant now={0,0},delay;AsyncDeadline earliest={0,0};int timers=0,earliest_set=0;
    uint32_t io_sequence=r->io_pool?file_async_sequence(r->io_pool):0;
    int io_active=io_progress(r);
    r->epoch++;
    for(t=r->pending;t;t=t->pending_next)if(t->kind==ASYNC_DELAY || (t->kind==ASYNC_TIMEOUT&&!t->timeout_reason)){timers=1;break;}
    if(timers)r->clock(r,&now);
    for(t=r->pending;t;t=next){
        next=t->pending_next;
        if(t->kind==ASYNC_YIELD && t->epoch<=r->epoch)finish(t);
        else if(t->kind==ASYNC_DELAY){
            if(*pending(r)){t->error=*pending(r);finish(t);}
            else if(!before(now,t->deadline))finish(t);
        }else if(t->kind==ASYNC_TIMEOUT && *pending(r))t->primary=*pending(r);
    }
    *pending(r)=0;
    /* Inputs are older tasks, so a forward pass resolves nested composites. */
    for(t=r->pending;t;t=next){next=t->pending_next;if(t->kind==ASYNC_ALL||t->kind==ASYNC_TIMEOUT)poll_composite(t,timers?&now:0);}
    for(t=r->pending;t;t=t->pending_next){

        if(t->kind==ASYNC_DELAY||(t->kind==ASYNC_TIMEOUT&&!t->timeout_reason)){
            if(!earliest_set||t->deadline.seconds<earliest.seconds||(t->deadline.seconds==earliest.seconds&&t->deadline.nanoseconds<earliest.nanoseconds)){earliest=t->deadline;earliest_set=1;}
        }
    }
    if(r->first){t=r->first;r->first=t->queue_next;if(!r->first)r->last=0;t->queue_next=0;t->queued=0;if(!done(t))run(r,t);return;}
    if(io_needs_turn(r)==1)return;
    if(r->io_ready && io_active){
        /* Only retry dispatch if an active worker finished this pass. */
        AsyncTask *a;int running=0;for(a=r->pending;a;a=a->pending_next)if(a->kind==ASYNC_FILE&&a->io_job&&a->io_job->worker)running++;
        if(running<4)return;
    }
    if(earliest_set){
        uint64_t seconds=earliest.seconds-(uint64_t)now.seconds;
        delay.seconds=seconds>86400?86400:(int64_t)seconds;delay.nanoseconds=(int64_t)earliest.nanoseconds-now.nanoseconds;
        if(delay.nanoseconds<0){delay.seconds--;delay.nanoseconds+=1000000000;}
        if(delay.seconds<0){delay.seconds=0;delay.nanoseconds=0;}
        if(io_active)file_async_wait(r->io_pool,io_sequence,delay.seconds,delay.nanoseconds);else r->sleep(r,&delay);
        if(*pending(r)){uint64_t error=*pending(r);*pending(r)=0;for(t=r->pending;t;t=next){next=t->pending_next;if(t->kind==ASYNC_DELAY){t->error=error;finish(t);}else if(t->kind==ASYNC_TIMEOUT)t->primary=error;}}
    }else if(io_active){file_async_wait(r->io_pool,io_sequence,-1,0);}
    else if(r->pending){
        uint64_t error=error_record(r,4,r->invalid_message,0);
        for(t=r->pending;t;t=t->pending_next)if(t->invoke&&!t->executing){t->primary=error;enqueue(r,t);}
    }
}
__attribute__((visibility("hidden"))) uint64_t qyno_async(AsyncRequest *q)
{
    AsyncRuntime *r=q->runtime;AsyncTask *self=r->current;
    switch(q->op){
    case ASYNC_CREATE:case ASYNC_DELAY:case ASYNC_YIELD:case ASYNC_ALL:case ASYNC_TIMEOUT:case ASYNC_FILE:return (uint64_t)create(q);
    case ASYNC_SHUTDOWN:file_async_stop(r->io_pool);r->io_pool=0;return 0;
    case ASYNC_CHECK:checkpoint(r);return 0;
    case ASYNC_CANCEL:
        if(done(q->task)||q->task->cancel_error)return 0;
        return (uint64_t)request_cancel(q->task,cancellation(r,0,1));
    case ASYNC_GROUP_CANCEL:
        if(q->group->closed||q->group->cancel_error)return 0;
        cancel_group(q->group,cancellation(r,0,1));return 1;
    case ASYNC_GROUP_ENTER:
        self->current_group=group_new(r,self,self->current_group);return (uint64_t)self->current_group;
    case ASYNC_GROUP_CLOSE:
        {uint64_t original=*pending(r),error;*pending(r)=0;error=close_group(r,q->group,original);*pending(r)=error;return 0;}
    case ASYNC_AWAIT:
        {uint64_t context=r->state[33];wait_for(r,q->task);
         if(*pending(r)&&context){uint64_t copy=error_clone(r,*pending(r)),*e=(uint64_t *)copy,*trace=r->allocate(r,16);trace[0]=context;trace[1]=e[8];e[8]=(uint64_t)trace;*pending(r)=copy;}
         return q->task->result;}
    case ASYNC_ERROR_LINK:
        {uint64_t head=error_clone(r,(uint64_t)q->task),*e=(uint64_t *)head;int budget=1024;
         while(e[5]&&budget--){e[5]=error_clone(r,e[5]);e=(uint64_t *)e[5];}
         if((uint64_t)e!=(uint64_t)q->args)e[5]=(uint64_t)q->args;
         return head;}
    default:
        while(!done(q->task))pump(r);
        if(q->task->error)*pending(r)=q->task->error;
        return q->task->result;
    }
}
