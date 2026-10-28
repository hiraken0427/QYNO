#ifndef QYNO_WIDE_RUNTIME_H
#define QYNO_WIDE_RUNTIME_H
#include <stdint.h>
typedef struct { uint64_t lo, hi; } WideBits;
/* Integer tags encode sign and width; real tags encode their binary format. */
enum { W_HALF=1016, W_FLOAT=1032, W_DOUBLE=1064, W_QUAD=1128 };
enum { W_CONVERT, W_ADD, W_SUB, W_MUL, W_DIV, W_MOD, W_NEG, W_ABS,
       W_POW, W_AND, W_OR, W_XOR, W_NOT, W_SHL, W_SHR, W_CMP,
       W_SQRT, W_ROUND, W_READ, W_FORMAT, W_NEXT, W_TEXT, W_CURSOR, W_DIGITS, W_PATH };
typedef struct {
    WideBits a, b, out;
    int64_t op, ty, from, aux;
    int64_t status, length, used, base;
    const char *input;
    char *buffer;
} WideRequest;
/* No allocation, OS calls, mutable globals, locale, or host long-double format. */
void wide_execute(WideRequest *r);
#endif
