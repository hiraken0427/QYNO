#ifndef QYNO_TEXT_RUNTIME_H
#define QYNO_TEXT_RUNTIME_H
#include <stdint.h>
enum { TEXT_CONTAINS,TEXT_STARTS,TEXT_ENDS,TEXT_FIND,TEXT_TRIM,TEXT_SPLIT,TEXT_REPLACE };
/* Phases: 0 validates all input and measures scratch, 1 measures output,
 * 2 fills it. Inputs remain immutable throughout; capacities are in bytes. */
typedef struct {
    const unsigned char *text[3];
    uint64_t length[3];
    int64_t op,phase,status,result;
    uint64_t scratch_size,capacity,used,count;
    uint64_t *scratch;
    unsigned char *buffer;
} TextRequest;
#endif
