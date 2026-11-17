#ifndef QYNO_ASYNC_RUNTIME_H
#define QYNO_ASYNC_RUNTIME_H
#include <stddef.h>
#include <stdint.h>

/* The freestanding engine and generated bridges share this private ABI. */
typedef struct AsyncRuntime AsyncRuntime;
typedef struct AsyncTask AsyncTask;
typedef struct { uint64_t rsp,rbp,rbx,r12,r13,r14,r15,rip; } AsyncContext;
typedef struct { int64_t seconds,nanoseconds; } AsyncInstant;
typedef uint64_t (*AsyncInvoke)(AsyncRuntime *,AsyncTask *);
struct AsyncTask {
    AsyncRuntime *runtime;
    AsyncContext context;
    AsyncContext *caller;
    AsyncInvoke invoke;
    uint64_t *args;
    uint64_t nargs;
    void *stack;
    uint64_t stack_top;
    AsyncTask *parent,*children,*last_child,*sibling,*pending_next,*queue_next,*waiting;
    uint64_t result,error,state,observed,queued,kind,epoch;
    AsyncInstant deadline;
    uint64_t io[11]; /* collection and I/O context, state offsets 256..336 */
};
struct AsyncRuntime {
    uint64_t *state;
    uint64_t initial_top;
    void *(*allocate)(AsyncRuntime *,uint64_t);
    void (*clock)(AsyncRuntime *,AsyncInstant *);
    void (*sleep)(AsyncRuntime *,AsyncInstant *);
    AsyncTask *current,*root,*pending,*pending_last,*first,*last;
    uint64_t epoch;
    uint64_t *invalid_message,*empty_string;
};
enum { ASYNC_CREATE, ASYNC_AWAIT, ASYNC_RUN, ASYNC_DELAY, ASYNC_YIELD };
typedef struct {
    uint64_t op;
    AsyncRuntime *runtime;
    AsyncInvoke invoke;
    uint64_t *args,nargs;
    AsyncTask *task;
    AsyncInstant delay;
} AsyncRequest;
#define ASYNC_STACK_BYTES (1024u*1024u)
#define ASYNC_STATE_RUNTIME 392
#define ASYNC_STATE_HEAP 400
#define ASYNC_STATE_COUNT 408
#define ASYNC_STATE_TOP 416
#define ASYNC_STATE_SIZE 424
#endif
