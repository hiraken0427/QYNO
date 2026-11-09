#ifndef QYC_MACHINE_IR_H
#define QYC_MACHINE_IR_H
#include "optimizer.h"

/* Scalar SSA regions end at control-flow/cleanup boundaries. Addressable
 * source homes are the merge locations; no pointer is promoted into XMM. */
typedef enum {
    MI_CONSTANT, MI_VARIABLE, MI_FIELD, MI_ARRAY, MI_LENGTH,
    MI_BINARY, MI_CONVERT, MI_NEGATE, MI_NOT, MI_COMPARE,
    MI_OPAQUE, MI_CONTEXT, MI_STORE, MI_FIELD_STORE, MI_REF_CHECK
} MachineOp;
typedef struct {
    MachineOp op;
    Type ty;
    Node *source;
    Var *root;
    int a,b,last,available;
    uint64_t bits;
} MachineInst;
typedef struct {
    /* Ordered allocator events. STEP carries registers for lowering, while
     * sa/sb identify materialized operands at fixed-register barriers. */
    int kind,at,value,reg,slot,a,b,d,sa,sb;
} MachineEvent;
enum { ME_SPILL, ME_RELOAD, ME_FORGET, ME_EXPIRE, ME_STEP, ME_RESULT };
typedef struct {
    MachineEvent *events;
    int count,profile,spill_count;
} MachinePlan;
typedef int (*MachineSink)(void *,const MachineEvent *);
struct MachineRegion {
    MachineRegion *next;
    /* Six XMM registers normally; five when XMM5 holds a loop value. */
    MachinePlan *plans[2];
    Function *owner;
    MachineInst *inst;
    int count,result,statements;
    Node *after;
};
int machine_region_valid(const MachineRegion *region);
int machine_is_float(Type ty);
int machine_is_barrier(const MachineInst *inst);
int machine_allocate(MachineRegion *r,int profile,int fixed,MachineSink sink,void *context,int *peak);
int machine_plan_valid(const MachineRegion *r,const MachinePlan *plan);
void machine_plan_function(Function *fn);
#endif
