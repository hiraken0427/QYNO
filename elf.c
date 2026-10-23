/*
 * Linux x64 用の実行ファイル（ELF64 形式）を書き出す
 *
 * ファイルの構成
 *   0x00  ELF ヘッダ
 *   0x40  プログラムヘッダ ×2（読み込み用 PT_LOAD、スタック設定用 PT_GNU_STACK）
 *   0xB0  機械語（エントリ処理・実行時ルーチン・main）
 *   ....  文字列などのデータ（16バイト境界から）
 *
 * ファイル全体を仮想アドレス 0x400000 に読み込む。
 */

#if defined(__unix__) || defined(__APPLE__)
#define _POSIX_C_SOURCE 200809L
#include <sys/stat.h>
#endif

#include "qyc.h"

#include <stdlib.h>
#include <string.h>

#define BASE_ADDRESS   0x400000ULL
#define EHDR_SIZE      64
#define PHDR_SIZE      56
#define PHDR_COUNT     2
#define CODE_OFFSET    (EHDR_SIZE + PHDR_SIZE * PHDR_COUNT)

static void put_u16(unsigned char *p, uint16_t v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
}

static void put_u32(unsigned char *p, uint32_t v)
{
    int i;

    for (i = 0; i < 4; i++) {
        p[i] = (unsigned char)(v >> (8 * i));
    }
}

static void put_u64(unsigned char *p, uint64_t v)
{
    int i;

    for (i = 0; i < 8; i++) {
        p[i] = (unsigned char)(v >> (8 * i));
    }
}

/* rip 相対の参照を埋める（disp32 = 参照先 - 次の命令の位置） */
static void apply_relocs(unsigned char *img, const Program *prog, size_t data_offset)
{
    int i;
    const Reloc *r;
    size_t pos;
    long disp;

    for (i = 0; i < prog->reloc_count; i++) {
        r = &prog->relocs[i];
        if (r->kind != RELOC_DATA) {
            fatal("内部エラー: Linux 向けにインポートは使えません");
        }
        pos = CODE_OFFSET + r->pos;
        disp = (long)(data_offset + r->target) - (long)(pos + 4);
        put_u32(img + pos, (uint32_t)(int32_t)disp);
    }
}

void write_elf(const char *path, const Program *prog)
{
    size_t data_offset;
    size_t file_size;
    unsigned char *img;
    unsigned char *ph;
    FILE *fp;

    data_offset = (CODE_OFFSET + prog->text.len + 15) / 16 * 16;
    file_size = data_offset + prog->data.len;
    img = calloc(1, file_size);
    if (img == NULL) {
        fatal("メモリを確保できません");
    }

    /* ELF ヘッダ */
    img[0] = 0x7F;
    img[1] = 'E';
    img[2] = 'L';
    img[3] = 'F';
    img[4] = 2;                                         /* 64ビット */
    img[5] = 1;                                         /* リトルエンディアン */
    img[6] = 1;                                         /* バージョン */
    put_u16(img + 16, 2);                               /* e_type: 実行ファイル */
    put_u16(img + 18, 62);                              /* e_machine: x86-64 */
    put_u32(img + 20, 1);                               /* e_version */
    put_u64(img + 24, BASE_ADDRESS + CODE_OFFSET + prog->entry);    /* e_entry */
    put_u64(img + 32, EHDR_SIZE);                       /* e_phoff */
    put_u16(img + 52, EHDR_SIZE);                       /* e_ehsize */
    put_u16(img + 54, PHDR_SIZE);                       /* e_phentsize */
    put_u16(img + 56, PHDR_COUNT);                      /* e_phnum */
    put_u16(img + 58, 64);                              /* e_shentsize */

    /* PT_LOAD：ファイル全体を読み取り・実行可能で読み込む */
    ph = img + EHDR_SIZE;
    put_u32(ph + 0, 1);                                 /* p_type: PT_LOAD */
    put_u32(ph + 4, 5);                                 /* p_flags: R | X */
    put_u64(ph + 8, 0);                                 /* p_offset */
    put_u64(ph + 16, BASE_ADDRESS);                     /* p_vaddr */
    put_u64(ph + 24, BASE_ADDRESS);                     /* p_paddr */
    put_u64(ph + 32, file_size);                        /* p_filesz */
    put_u64(ph + 40, file_size);                        /* p_memsz */
    put_u64(ph + 48, 0x1000);                           /* p_align */

    /* PT_GNU_STACK：スタックを実行不可にする */
    ph = img + EHDR_SIZE + PHDR_SIZE;
    put_u32(ph + 0, 0x6474E551);                        /* p_type: PT_GNU_STACK */
    put_u32(ph + 4, 6);                                 /* p_flags: R | W */
    put_u64(ph + 48, 16);                               /* p_align */

    /* 機械語とデータ */
    memcpy(img + CODE_OFFSET, prog->text.data, prog->text.len);
    if (prog->data.len > 0) {
        memcpy(img + data_offset, prog->data.data, prog->data.len);
    }
    apply_relocs(img, prog, data_offset);

    fp = fopen(path, "wb");
    if (fp == NULL) {
        fatal("%s に書き込めません", path);
    }
    if (fwrite(img, 1, file_size, fp) != file_size) {
        fclose(fp);
        fatal("%s の書き込みに失敗しました", path);
    }
    if (fclose(fp) != 0) {
        fatal("%s の書き込みに失敗しました", path);
    }
    free(img);

#if defined(__unix__) || defined(__APPLE__)
    /* 実行権限を付ける */
    if (chmod(path, 0755) != 0) {
        fatal("%s に実行権限を付けられません", path);
    }
#endif
}
