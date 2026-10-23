#include "qyc.h"

#include <stdlib.h>
#include <string.h>

void buf_init(Buffer *b)
{
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

void buf_free(Buffer *b)
{
    free(b->data);
    buf_init(b);
}

static void buf_reserve(Buffer *b, size_t n)
{
    size_t new_cap;
    unsigned char *new_data;

    if (b->len + n <= b->cap) {
        return;
    }
    new_cap = b->cap == 0 ? 256 : b->cap;
    while (new_cap < b->len + n) {
        new_cap *= 2;
    }
    new_data = realloc(b->data, new_cap);
    if (new_data == NULL) {
        fatal("メモリを確保できません");
    }
    b->data = new_data;
    b->cap = new_cap;
}

void buf_add(Buffer *b, const unsigned char *bytes, size_t n)
{
    buf_reserve(b, n);
    memcpy(b->data + b->len, bytes, n);
    b->len += n;
}

void buf_add_u8(Buffer *b, uint8_t v)
{
    buf_add(b, &v, 1);
}

/* 以下はリトルエンディアンで書き込む */
void buf_add_u32(Buffer *b, uint32_t v)
{
    int i;

    for (i = 0; i < 4; i++) {
        buf_add_u8(b, (uint8_t)(v >> (8 * i)));
    }
}

void buf_add_u64(Buffer *b, uint64_t v)
{
    int i;

    for (i = 0; i < 8; i++) {
        buf_add_u8(b, (uint8_t)(v >> (8 * i)));
    }
}

void buf_set_u32(Buffer *b, size_t pos, uint32_t v)
{
    int i;

    if (pos + 4 > b->len) {
        fatal("内部エラー: バッファの範囲外に書き込もうとしました");
    }
    for (i = 0; i < 4; i++) {
        b->data[pos + (size_t)i] = (unsigned char)(v >> (8 * i));
    }
}
