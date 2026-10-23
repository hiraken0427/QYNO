#include "qyc.h"

#include <stdlib.h>

void prog_init(Program *prog)
{
    buf_init(&prog->text);
    buf_init(&prog->data);
    prog->relocs = NULL;
    prog->reloc_count = 0;
    prog->reloc_cap = 0;
    prog->entry = 0;
}

void prog_free(Program *prog)
{
    buf_free(&prog->text);
    buf_free(&prog->data);
    free(prog->relocs);
    prog->relocs = NULL;
    prog->reloc_count = 0;
    prog->reloc_cap = 0;
}

/* データを追加し、data 内の位置を返す */
size_t prog_add_data(Program *prog, const char *bytes, size_t n)
{
    size_t offset;

    offset = prog->data.len;
    buf_add(&prog->data, (const unsigned char *)bytes, n);
    return offset;
}

void prog_add_reloc(Program *prog, RelocKind kind, size_t pos, size_t target)
{
    Reloc *new_relocs;
    int new_cap;

    if (prog->reloc_count >= prog->reloc_cap) {
        new_cap = prog->reloc_cap == 0 ? 16 : prog->reloc_cap * 2;
        new_relocs = realloc(prog->relocs, sizeof(Reloc) * (size_t)new_cap);
        if (new_relocs == NULL) {
            fatal("メモリを確保できません");
        }
        prog->relocs = new_relocs;
        prog->reloc_cap = new_cap;
    }
    prog->relocs[prog->reloc_count].kind = kind;
    prog->relocs[prog->reloc_count].pos = pos;
    prog->relocs[prog->reloc_count].target = target;
    prog->reloc_count++;
}

/* str の値（[長さ 8バイト][本体]）をデータに置き、その先頭の位置を返す */
size_t prog_add_str(Program *prog, const char *bytes, size_t n)
{
    size_t offset;
    int i;

    while (prog->data.len % 8 != 0) {
        buf_add_u8(&prog->data, 0);
    }
    offset = prog->data.len;
    for (i = 0; i < 8; i++) {
        buf_add_u8(&prog->data, (uint8_t)((uint64_t)n >> (8 * i)));
    }
    if (n > 0) {
        buf_add(&prog->data, (const unsigned char *)bytes, n);
    }
    return offset;
}
