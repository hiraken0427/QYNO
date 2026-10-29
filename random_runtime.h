#ifndef QYNO_RANDOM_RUNTIME_H
#define QYNO_RANDOM_RUNTIME_H
#include "wide_runtime.h"
typedef struct {
    int64_t op,ty,status,phase,flags,first_tag,second_tag;
    WideBits first,second,out;
    uint64_t *state;
    const char *text;
    int64_t length,count;
    char *buffer;
    int64_t capacity,used,need_random,seed_auto;
    uint64_t seed_value;
} RandomRequest;
#endif
