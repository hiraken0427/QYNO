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
struct MachineRegion {
    Function *owner;
    MachineInst *inst;
    int count,result,statements;
    Node *after;
};
int machine_region_valid(const MachineRegion *region);
#endif
