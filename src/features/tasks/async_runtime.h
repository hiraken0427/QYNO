#ifndef QYNO_ASYNC_RUNTIME_H
#define QYNO_ASYNC_RUNTIME_H
#include <stddef.h>
#include <stdint.h>
typedef struct AsyncRuntime AsyncRuntime;
typedef struct AsyncTask AsyncTask;
typedef struct AsyncGroup AsyncGroup;
typedef struct AsyncLink AsyncLink;
typedef struct AsyncWaiter AsyncWaiter;
typedef struct { uint64_t rsp,rbp,rbx,r12,r13,r14,r15,rip; } AsyncContext;
typedef struct { int64_t seconds,nanoseconds; } AsyncInstant;
typedef struct { uint64_t seconds,nanoseconds; } AsyncDeadline;
typedef uint64_t (*AsyncInvoke)(AsyncRuntime *,AsyncTask *);
struct AsyncWaiter { AsyncWaiter *next;AsyncTask *waiter;uint64_t active; };
struct AsyncLink { AsyncLink *next;AsyncTask *wrapper; };
struct AsyncGroup {
    AsyncTask *owner;AsyncGroup *parent;AsyncTask *first,*last;
    uint64_t closing,closed,cancel_error,primary,error,site[4];
};
struct AsyncTask {
    AsyncRuntime *runtime;AsyncContext context;AsyncContext *caller;AsyncInvoke invoke;
    uint64_t *args,nargs;void *stack;uint64_t stack_top;
    AsyncTask *parent,*children,*last_child,*sibling,*pending_next,*queue_next,*waiting;
    uint64_t result,error,state,observed,queued,kind,epoch;
    AsyncDeadline deadline;
    uint64_t io[11];
    AsyncGroup *group,*root_group,*current_group,*waiting_group;
    AsyncTask *group_next,*group_prev,*sibling_prev;
    AsyncWaiter *waiters,*wait_link;
    AsyncLink *representatives;
    AsyncTask **inputs;uint64_t input_count,*shape;
    uint64_t cancel_error,executing,cleanup_mask,timeout_reason,primary,site[4];
    AsyncTask *cancel_next,*visit_next;uint64_t visit_stamp;
};
struct AsyncRuntime {
    uint64_t *state,initial_top;
    void *(*allocate)(AsyncRuntime *,uint64_t);
    void (*clock)(AsyncRuntime *,AsyncInstant *);
    void (*sleep)(AsyncRuntime *,AsyncInstant *);
    AsyncTask *current,*root,*pending,*pending_last,*first,*last;
    uint64_t epoch,request_sequence;
    uint64_t *invalid_message,*empty_string,*cancel_message,*group_message,*timeout_message;
    uint64_t *io_domain,*task_domain,*error_shape;
    uint64_t visit_sequence;
};
enum { ASYNC_CREATE,ASYNC_AWAIT,ASYNC_RUN,ASYNC_DELAY,ASYNC_YIELD,
       ASYNC_GROUP_ENTER,ASYNC_GROUP_CLOSE,ASYNC_CANCEL,ASYNC_GROUP_CANCEL,
       ASYNC_ALL,ASYNC_TIMEOUT,ASYNC_CHECK,ASYNC_ERROR_LINK };
typedef struct {
    uint64_t op;AsyncRuntime *runtime;AsyncInvoke invoke;uint64_t *args,nargs;
    AsyncTask *task;AsyncInstant delay;AsyncGroup *group;uint64_t *shape,flags;
} AsyncRequest;
#define ASYNC_STACK_BYTES (1024u*1024u)
#define ASYNC_STATE_RUNTIME 392
#define ASYNC_STATE_HEAP 400
#define ASYNC_STATE_COUNT 408
#define ASYNC_STATE_TOP 416
#define ASYNC_STATE_SIZE 424
#endif
