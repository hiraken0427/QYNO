#ifndef QYC_OPTIMIZER_H
#define QYC_OPTIMIZER_H

#include "qyc.h"

/* Independent effects: a fatal trap does not set the recoverable error slot. */
enum {
    OPT_READ = 1u << 0, OPT_WRITE = 1u << 1,
    OPT_RAISE = 1u << 2, OPT_TRAP = 1u << 3,
    OPT_ALLOCATE = 1u << 4, OPT_EXTERNAL = 1u << 5,
    OPT_BORROW = 1u << 6, OPT_RESOURCE = 1u << 7,
    OPT_CONTEXT = 1u << 8, OPT_ESCAPE = 1u << 9,
    OPT_ALL = (1u << 10) - 1
};

/* Evaluated arguments bind to independent slots in the caller's frame. */
struct OptInline {
    Function *owner;
    Node *body;
    Var **params;
    int param_count;
    int *roots;
    int root_count;
};

void optimize_program(Ast *ast);
unsigned optimizer_local_effects(const Node *node);
unsigned optimizer_effects(const Node *node);
unsigned optimizer_copy_effects(Type ty);
int optimizer_integer_canonical(const Node *node);
void optimizer_inline(Ast *ast);
void optimizer_reachable(Ast *ast);
enum { OA_READ = 1, OA_WRITE = 2, OA_SHAPE = 4, OA_REBIND = 8, OA_ESCAPE = 16, OA_ALL = 31 };
struct OptHoist {
    OptHoist *next;
    Function *owner;
    Node *source;
    Var *root;
    int slot;
};
void optimizer_memory(Ast *ast);
int optimizer_may_change(Node *nodes, Var *root, int shape_only);
void optimizer_ranges(Ast *ast);
void optimizer_machine(Ast *ast);
int optimizer_scalar(Type ty);

#endif
