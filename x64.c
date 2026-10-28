/*
 * x64 命令のエンコーダ
 *
 * 1 命令ごとに、機械語を prog->text に追記し、text が NULL でなければ
 * 同じ命令を Intel 記法のアセンブリとして text にも出力する。
 * ラベルへのジャンプ・呼び出しは x64_end でまとめて相対位置を埋める。
 * データやインポートへの参照は Program の再配置情報に記録し、
 * 実行ファイルの書き出し時（pe.c / elf.c）に埋める。
 */

#include "qyc.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define LABEL_NAME_SIZE 128

typedef struct {
    char name[LABEL_NAME_SIZE];
    long offset;            /* 未確定なら -1 */
} Label;

typedef struct {
    int label;
    size_t pos;             /* rel32 を書き込む位置 */
} Fixup;

static Program *prog;
static FILE *text;
static Label *labels;          /* 必要に応じて広げる */
static int label_count;
static int label_cap;
static Fixup *fixups;
static int fixup_count;
static int fixup_cap;

static const char *reg64_names[] = {
    "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
};

static const char *reg32_names[] = {
    "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
    "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d"
};

static const char *reg8_names[] = {
    "al", "cl", "dl", "bl", "spl", "bpl", "sil", "dil",
    "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b"
};

void x64_begin(Program *out_prog, FILE *out_text)
{
    prog = out_prog;
    text = out_text;
    label_count = 0;
    fixup_count = 0;
}

/* ラベルへのジャンプ・呼び出しの相対位置を埋める */
void x64_end(void)
{
    int i;
    Fixup *f;
    long target;
    long rel;

    for (i = 0; i < fixup_count; i++) {
        f = &fixups[i];
        target = labels[f->label].offset;
        if (target < 0) {
            fatal("内部エラー: ラベル %s の位置が決まっていません", labels[f->label].name);
        }
        rel = target - (long)(f->pos + 4);
        buf_set_u32(&prog->text, f->pos, (uint32_t)(int32_t)rel);
    }
}

/* ---------- 出力の共通処理 ---------- */

static void text_line(const char *fmt, va_list ap)
{
    fprintf(text, "    ");
    vfprintf(text, fmt, ap);
    fprintf(text, "\n");
}

/* 機械語を追記し、対応するアセンブリを1行出力する */
static void emit(const unsigned char *bytes, size_t n, const char *fmt, ...)
{
    va_list ap;

    buf_add(&prog->text, bytes, n);
    if (text == NULL) {
        return;
    }
    va_start(ap, fmt);
    text_line(fmt, ap);
    va_end(ap);
}

/* アセンブラ指示（テキストのみ。機械語は出さない） */
void x64_directive(const char *fmt, ...)
{
    va_list ap;

    if (text == NULL) {
        return;
    }
    va_start(ap, fmt);
    text_line(fmt, ap);
    va_end(ap);
}

void x64_comment(const char *fmt, ...)
{
    va_list ap;

    if (text == NULL) {
        return;
    }
    fprintf(text, "    # ");
    va_start(ap, fmt);
    vfprintf(text, fmt, ap);
    va_end(ap);
    fprintf(text, "\n");
}

int x64_new_label(const char *name)
{
    Label *l;

    if (label_count >= label_cap) {
        label_cap = label_cap == 0 ? 1024 : label_cap * 2;
        labels = realloc(labels, sizeof(Label) * (size_t)label_cap);
        if (labels == NULL) {
            fatal("メモリを確保できません");
        }
    }
    l = &labels[label_count];
    strncpy(l->name, name, LABEL_NAME_SIZE - 1);
    l->name[LABEL_NAME_SIZE - 1] = '\0';
    l->offset = -1;
    return label_count++;
}

void x64_bind(int label)
{
    labels[label].offset = (long)prog->text.len;
    if (text != NULL) {
        fprintf(text, "%s:\n", labels[label].name);
    }
}

void x64_blob(const unsigned char *bytes, size_t length)
{
    size_t i;
    buf_add(&prog->text, bytes, length);
    if (text) for (i=0;i<length;i++) {
        if (i%16==0) fprintf(text,"    .byte "); else fprintf(text,",");
        fprintf(text,"%u",(unsigned)bytes[i]);
        if (i%16==15 || i+1==length) fprintf(text,"\n");
    }
}

static void add_fixup(int label, size_t pos)
{
    if (fixup_count >= fixup_cap) {
        fixup_cap = fixup_cap == 0 ? 4096 : fixup_cap * 2;
        fixups = realloc(fixups, sizeof(Fixup) * (size_t)fixup_cap);
        if (fixups == NULL) {
            fatal("メモリを確保できません");
        }
    }
    fixups[fixup_count].label = label;
    fixups[fixup_count].pos = pos;
    fixup_count++;
}

/* ---------- エンコードの部品 ---------- */

static int low3(Reg r)
{
    return (int)r & 7;
}

static int ext(Reg r)
{
    return ((int)r >> 3) & 1;
}

/* REX プレフィックス（w：64ビット演算、reg：ModR/M の reg 欄、rm：ModR/M の r/m 欄） */
static unsigned char rex(int w, Reg reg, Reg rm)
{
    return (unsigned char)(0x40 | (w << 3) | (ext(reg) << 2) | ext(rm));
}

/* ModR/M バイト（レジスタ同士：mod = 11） */
static unsigned char modrm_reg(int reg, Reg rm)
{
    return (unsigned char)(0xC0 | ((reg & 7) << 3) | low3(rm));
}

/* [base + disp] の ModR/M・SIB・disp を b に書き、書いたバイト数を返す */
static int modrm_mem(unsigned char *b, int reg, Reg base, int32_t disp)
{
    int n;
    int mod;
    int i;

    n = 0;
    if (disp == 0 && low3(base) != 5) {
        mod = 0;
    } else if (disp >= -128 && disp <= 127) {
        mod = 1;
    } else {
        mod = 2;
    }
    b[n++] = (unsigned char)((mod << 6) | ((reg & 7) << 3) | low3(base));
    if (low3(base) == 4) {
        b[n++] = 0x24;      /* SIB：base = rsp / r12、インデックスなし */
    }
    if (mod == 1) {
        b[n++] = (unsigned char)(int8_t)disp;
    } else if (mod == 2) {
        for (i = 0; i < 4; i++) {
            b[n++] = (unsigned char)((uint32_t)disp >> (8 * i));
        }
    }
    return n;
}

/* アセンブリ表記の [base+disp] */
static const char *mem_text(char *buf, size_t size, Reg base, int32_t disp)
{
    if (disp == 0) {
        snprintf(buf, size, "[%s]", reg64_names[base]);
    } else if (disp > 0) {
        snprintf(buf, size, "[%s+%ld]", reg64_names[base], (long)disp);
    } else {
        snprintf(buf, size, "[%s-%ld]", reg64_names[base], -(long)disp);
    }
    return buf;
}

static void put_imm32(unsigned char *b, int32_t v)
{
    int i;

    for (i = 0; i < 4; i++) {
        b[i] = (unsigned char)((uint32_t)v >> (8 * i));
    }
}

/* レジスタ同士の演算：REX.W op /r（reg 欄 = src、r/m 欄 = dst） */
static void rr_op(unsigned char op, const char *name, Reg dst, Reg src)
{
    unsigned char b[3];

    b[0] = rex(1, src, dst);
    b[1] = op;
    b[2] = modrm_reg(src, dst);
    emit(b, 3, "%s %s, %s", name, reg64_names[dst], reg64_names[src]);
}

/* 1オペランドの演算：REX.W F7 /ext や REX.W FF /ext */
static void unary_op(unsigned char op, int ext_code, const char *name, Reg r)
{
    unsigned char b[3];

    b[0] = rex(1, RAX, r);
    b[1] = op;
    b[2] = modrm_reg(ext_code, r);
    emit(b, 3, "%s %s", name, reg64_names[r]);
}

/* 即値との演算：REX.W 83 /ext ib（8ビットに収まる場合）または REX.W 81 /ext id */
static void imm_op(int ext_code, const char *name, Reg dst, int32_t imm)
{
    unsigned char b[7];

    b[0] = rex(1, RAX, dst);
    b[2] = modrm_reg(ext_code, dst);
    if (imm >= -128 && imm <= 127) {
        b[1] = 0x83;
        b[3] = (unsigned char)(int8_t)imm;
        emit(b, 4, "%s %s, %ld", name, reg64_names[dst], (long)imm);
        return;
    }
    if (dst == RAX) {
        /* rax には短い形式がある（REX.W 05/2D/25/35/3D id。オペコードは /ext × 8 + 5） */
        b[1] = (unsigned char)(ext_code * 8 + 5);
        put_imm32(b + 2, imm);
        emit(b, 6, "%s %s, %ld", name, reg64_names[dst], (long)imm);
        return;
    }
    b[1] = 0x81;
    put_imm32(b + 3, imm);
    emit(b, 7, "%s %s, %ld", name, reg64_names[dst], (long)imm);
}

/* ---------- 命令 ---------- */

/* push r64：50+r（r8〜r15 は REX.B 付き） */
void x64_push(Reg r)
{
    unsigned char b[2];
    int n;

    n = 0;
    if (ext(r)) {
        b[n++] = 0x41;
    }
    b[n++] = (unsigned char)(0x50 + low3(r));
    emit(b, (size_t)n, "push %s", reg64_names[r]);
}

/* pop r64：58+r */
void x64_pop(Reg r)
{
    unsigned char b[2];
    int n;

    n = 0;
    if (ext(r)) {
        b[n++] = 0x41;
    }
    b[n++] = (unsigned char)(0x58 + low3(r));
    emit(b, (size_t)n, "pop %s", reg64_names[r]);
}

/* mov r/m64, r64：REX.W 89 /r */
void x64_mov(Reg dst, Reg src)
{
    rr_op(0x89, "mov", dst, src);
}

/* mov r/m64, imm32（符号拡張）：REX.W C7 /0 id
 * 32ビットに収まらないときは mov r64, imm64：REX.W B8+r io */
void x64_mov_imm(Reg dst, long long val)
{
    unsigned char b[10];
    int i;

    if (val >= INT32_MIN && val <= INT32_MAX) {
        b[0] = rex(1, RAX, dst);
        b[1] = 0xC7;
        b[2] = modrm_reg(0, dst);
        put_imm32(b + 3, (int32_t)val);
        emit(b, 7, "mov %s, %lld", reg64_names[dst], val);
        return;
    }
    b[0] = rex(1, RAX, dst);
    b[1] = (unsigned char)(0xB8 + low3(dst));
    for (i = 0; i < 8; i++) {
        b[2 + i] = (unsigned char)((uint64_t)val >> (8 * i));
    }
    emit(b, 10, "mov %s, %lld", reg64_names[dst], val);
}

/* add r/m64, r64：REX.W 01 /r */
void x64_add(Reg dst, Reg src)
{
    rr_op(0x01, "add", dst, src);
}

/* sub r/m64, r64：REX.W 29 /r */
void x64_sub(Reg dst, Reg src)
{
    rr_op(0x29, "sub", dst, src);
}

/* xor r/m64, r64：REX.W 31 /r */
void x64_xor(Reg dst, Reg src)
{
    rr_op(0x31, "xor", dst, src);
}

/* test r/m64, r64：REX.W 85 /r */
void x64_test(Reg a, Reg b)
{
    rr_op(0x85, "test", a, b);
}

/* imul r64, r/m64：REX.W 0F AF /r（reg 欄 = dst） */
void x64_imul(Reg dst, Reg src)
{
    unsigned char b[4];

    b[0] = rex(1, dst, src);
    b[1] = 0x0F;
    b[2] = 0xAF;
    b[3] = modrm_reg(dst, src);
    emit(b, 4, "imul %s, %s", reg64_names[dst], reg64_names[src]);
}

/* add / sub / and r/m64, imm：83（または 81）/0・/5・/4 */
void x64_add_imm(Reg dst, int32_t imm)
{
    imm_op(0, "add", dst, imm);
}

void x64_sub_imm(Reg dst, int32_t imm)
{
    imm_op(5, "sub", dst, imm);
}

void x64_and_imm(Reg dst, int32_t imm)
{
    imm_op(4, "and", dst, imm);
}

/* shr r/m64, imm8（論理右シフト）：REX.W C1 /5 ib（1ビットなら短い形式 REX.W D1 /5） */
void x64_shr_imm(Reg dst, int imm)
{
    unsigned char b[4];

    b[0] = rex(1, RAX, dst);
    b[2] = modrm_reg(5, dst);
    if (imm == 1) {
        b[1] = 0xD1;
        emit(b, 3, "shr %s, 1", reg64_names[dst]);
        return;
    }
    b[1] = 0xC1;
    b[3] = (unsigned char)imm;
    emit(b, 4, "shr %s, %d", reg64_names[dst], imm);
}

/* neg r/m64：REX.W F7 /3 */
void x64_neg(Reg r)
{
    unary_op(0xF7, 3, "neg", r);
}

/* dec r/m64：REX.W FF /1 */
void x64_dec(Reg r)
{
    unary_op(0xFF, 1, "dec", r);
}

/* cqo（rax を rdx:rax に符号拡張）：REX.W 99 */
void x64_cqo(void)
{
    static const unsigned char b[] = { 0x48, 0x99 };

    emit(b, sizeof(b), "cqo");
}

/* idiv r/m64（符号付き除算）：REX.W F7 /7 */
void x64_idiv(Reg r)
{
    unary_op(0xF7, 7, "idiv", r);
}

/* div r/m64（符号なし除算）：REX.W F7 /6 */
void x64_div(Reg r)
{
    unary_op(0xF7, 6, "div", r);
}

/* mov r64, [base+disp]：REX.W 8B /r */
void x64_load(Reg dst, Reg base, int32_t disp)
{
    unsigned char b[16];
    char mem[64];
    int n;

    b[0] = rex(1, dst, base);
    b[1] = 0x8B;
    n = 2 + modrm_mem(b + 2, dst, base, disp);
    emit(b, (size_t)n, "mov %s, %s", reg64_names[dst], mem_text(mem, sizeof(mem), base, disp));
}

/* mov [base+disp], r64：REX.W 89 /r */
void x64_store(Reg base, int32_t disp, Reg src)
{
    unsigned char b[16];
    char mem[64];
    int n;

    b[0] = rex(1, src, base);
    b[1] = 0x89;
    n = 2 + modrm_mem(b + 2, src, base, disp);
    emit(b, (size_t)n, "mov %s, %s", mem_text(mem, sizeof(mem), base, disp), reg64_names[src]);
}

/* mov qword [base+disp], imm32：REX.W C7 /0 id */
void x64_store_imm(Reg base, int32_t disp, int32_t imm)
{
    unsigned char b[16];
    char mem[64];
    int n;

    b[0] = rex(1, RAX, base);
    b[1] = 0xC7;
    n = 2 + modrm_mem(b + 2, 0, base, disp);
    put_imm32(b + n, imm);
    n += 4;
    emit(b, (size_t)n, "mov qword ptr %s, %ld", mem_text(mem, sizeof(mem), base, disp), (long)imm);
}

/* mov byte [base+disp], r8：88 /r
 * spl・bpl・sil・dil（番号 4〜7）と r8b 以降は REX が必要（REX がないと ah などを指す） */
void x64_store8(Reg base, int32_t disp, Reg src)
{
    unsigned char b[16];
    char mem[64];
    int n;

    n = 0;
    if ((int)src >= 4 || ext(base)) {
        b[n++] = rex(0, src, base);
    }
    b[n++] = 0x88;
    n += modrm_mem(b + n, src, base, disp);
    emit(b, (size_t)n, "mov byte ptr %s, %s", mem_text(mem, sizeof(mem), base, disp), reg8_names[src]);
}

/* mov byte [base+disp], imm8：C6 /0 ib */
void x64_store8_imm(Reg base, int32_t disp, uint8_t imm)
{
    unsigned char b[16];
    char mem[64];
    int n;

    n = 0;
    if (ext(base)) {
        b[n++] = rex(0, RAX, base);
    }
    b[n++] = 0xC6;
    n += modrm_mem(b + n, 0, base, disp);
    b[n++] = imm;
    emit(b, (size_t)n, "mov byte ptr %s, %u", mem_text(mem, sizeof(mem), base, disp), (unsigned)imm);
}

/* lea r64, [base+disp]：REX.W 8D /r */
void x64_lea(Reg dst, Reg base, int32_t disp)
{
    unsigned char b[16];
    char mem[64];
    int n;

    b[0] = rex(1, dst, base);
    b[1] = 0x8D;
    n = 2 + modrm_mem(b + 2, dst, base, disp);
    emit(b, (size_t)n, "lea %s, %s", reg64_names[dst], mem_text(mem, sizeof(mem), base, disp));
}

/* lea r64, [rip+disp32]：REX.W 8D /r（mod=00, r/m=101）。disp32 は書き出し時に埋める */
void x64_lea_data(Reg dst, size_t data_offset)
{
    unsigned char b[7];

    b[0] = rex(1, dst, RAX);
    b[1] = 0x8D;
    b[2] = (unsigned char)(((dst & 7) << 3) | 5);
    put_imm32(b + 3, 0);
    prog_add_reloc(prog, RELOC_DATA, prog->text.len + 3, data_offset);
    emit(b, 7, "lea %s, [rip+.L.data+%lu]", reg64_names[dst], (unsigned long)data_offset);
}

/* jmp rel32：E9 cd */
void x64_jmp(int label)
{
    static const unsigned char b[] = { 0xE9, 0x00, 0x00, 0x00, 0x00 };

    add_fixup(label, prog->text.len + 1);
    emit(b, sizeof(b), "jmp %s", labels[label].name);
}

/* 条件コードの名前（jcc・setcc の後ろに付く部分） */
static const char *cond_suffix(Cond cc)
{
    static const char *names[16] = {
        "o", "no", "b", "ae", "z", "nz", "be", "a",
        "s", "ns", "p", "np", "l", "ge", "le", "g"
    };

    return names[(int)cc & 15];
}

static const char *cond_jump_name(Cond cc)
{
    static char buf[8];

    snprintf(buf, sizeof(buf), "j%s", cond_suffix(cc));
    return buf;
}

/* jcc rel32：0F 80+cc cd */
void x64_jcc(Cond cc, int label)
{
    unsigned char b[6];
    const char *name;

    b[0] = 0x0F;
    b[1] = (unsigned char)(0x80 + cc);
    put_imm32(b + 2, 0);
    name = cond_jump_name(cc);
    add_fixup(label, prog->text.len + 2);
    emit(b, 6, "%s %s", name, labels[label].name);
}

/* call rel32：E8 cd */
void x64_call(int label)
{
    static const unsigned char b[] = { 0xE8, 0x00, 0x00, 0x00, 0x00 };

    add_fixup(label, prog->text.len + 1);
    emit(b, sizeof(b), "call %s", labels[label].name);
}

/* call [rip+disp32]：FF 15 cd（インポートアドレステーブル経由の呼び出し） */
void x64_call_import(Import imp)
{
    static const unsigned char b[] = { 0xFF, 0x15, 0x00, 0x00, 0x00, 0x00 };

    prog_add_reloc(prog, RELOC_IMPORT, prog->text.len + 2, (size_t)imp);
    emit(b, sizeof(b), "call [rip+__imp_%s]", import_name(imp));
}

/* syscall：0F 05 */
void x64_syscall(void)
{
    static const unsigned char b[] = { 0x0F, 0x05 };

    emit(b, sizeof(b), "syscall");
}

/* int3：CC */
void x64_int3(void)
{
    static const unsigned char b[] = { 0xCC };

    emit(b, sizeof(b), "int3");
}

/* ret：C3 */
void x64_ret(void)
{
    static const unsigned char b[] = { 0xC3 };

    emit(b, sizeof(b), "ret");
}

/* cmp r/m64, r64：REX.W 39 /r（a - b の結果でフラグを立てる） */
void x64_cmp(Reg a, Reg b)
{
    rr_op(0x39, "cmp", a, b);
}

/* ビット演算：REX.W 0F BA /ext ib */
static void bit_op(int ext_code, const char *name, Reg r, int bit)
{
    unsigned char b[5];

    b[0] = rex(1, RAX, r);
    b[1] = 0x0F;
    b[2] = 0xBA;
    b[3] = modrm_reg(ext_code, r);
    b[4] = (unsigned char)bit;
    emit(b, 5, "%s %s, %d", name, reg64_names[r], bit);
}

/* btc：指定ビットを反転（浮動小数点の符号反転に使う） */
void x64_btc(Reg r, int bit)
{
    bit_op(7, "btc", r, bit);
}

/* btr：指定ビットを 0 にする */
void x64_btr(Reg r, int bit)
{
    bit_op(6, "btr", r, bit);
}

/* rep movsb：rsi から rdi へ rcx バイト複写する（F3 A4） */
void x64_rep_movsb(void)
{
    static const unsigned char b[] = { 0xF3, 0xA4 };

    emit(b, sizeof(b), "rep movsb");
}

/* ---------- 浮動小数点（SSE） ---------- */

static const char *xmm_names[] = {
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
};

static void check_xmm(int x)
{
    if (x < 0 || x > 7) {
        fatal("内部エラー: 未対応の xmm レジスタです");
    }
}

/*
 * プレフィックス・REX・0F・命令・ModR/M の形の SSE 命令を出力する
 *   prefix：66 / F2 / F3（0 なら付けない）
 *   w：REX.W（64ビットの整数レジスタを使うとき 1）
 *   reg・rm：ModR/M の reg 欄と r/m 欄（番号）
 */
static void sse_emit(unsigned char prefix, int w, unsigned char op, int reg, int rm, const char *fmt, ...)
{
    unsigned char b[6];
    int n;
    va_list ap;

    n = 0;
    if (prefix != 0) {
        b[n++] = prefix;
    }
    if (w || ext((Reg)reg) || ext((Reg)rm)) {
        b[n++] = rex(w, (Reg)reg, (Reg)rm);
    }
    b[n++] = 0x0F;
    b[n++] = op;
    b[n++] = modrm_reg(reg, (Reg)rm);
    buf_add(&prog->text, b, (size_t)n);
    if (text == NULL) {
        return;
    }
    va_start(ap, fmt);
    text_line(fmt, ap);
    va_end(ap);
}

/* movq xmm, r64：66 REX.W 0F 6E /r */
void x64_movq_to_xmm(int xmm, Reg src)
{
    check_xmm(xmm);
    sse_emit(0x66, 1, 0x6E, xmm, src, "movq %s, %s", xmm_names[xmm], reg64_names[src]);
}

/* movq r64, xmm：66 REX.W 0F 7E /r */
void x64_movq_from_xmm(Reg dst, int xmm)
{
    check_xmm(xmm);
    sse_emit(0x66, 1, 0x7E, xmm, dst, "movq %s, %s", reg64_names[dst], xmm_names[xmm]);
}

/* movd xmm, r32：66 0F 6E /r */
void x64_movd_to_xmm(int xmm, Reg src)
{
    check_xmm(xmm);
    sse_emit(0x66, 0, 0x6E, xmm, src, "movd %s, %s", xmm_names[xmm], reg32_names[src]);
}

/* movd r32, xmm：66 0F 7E /r（上位32ビットは 0 になる） */
void x64_movd_from_xmm(Reg dst, int xmm)
{
    check_xmm(xmm);
    sse_emit(0x66, 0, 0x7E, xmm, dst, "movd %s, %s", reg32_names[dst], xmm_names[xmm]);
}

static const char *sse_name(SseOp op)
{
    switch (op) {
    case SSE_ADD:
        return "add";
    case SSE_SUB:
        return "sub";
    case SSE_MUL:
        return "mul";
    default:
        return "div";
    }
}

/* addsd / subsd / mulsd / divsd xmm, xmm：F2 0F op /r */
void x64_sse_sd(SseOp op, int dst, int src)
{
    check_xmm(dst);
    check_xmm(src);
    sse_emit(0xF2, 0, (unsigned char)op, dst, src, "%ssd %s, %s", sse_name(op), xmm_names[dst], xmm_names[src]);
}

/* addss / subss / mulss / divss xmm, xmm：F3 0F op /r */
void x64_sse_ss(SseOp op, int dst, int src)
{
    check_xmm(dst);
    check_xmm(src);
    sse_emit(0xF3, 0, (unsigned char)op, dst, src, "%sss %s, %s", sse_name(op), xmm_names[dst], xmm_names[src]);
}

/* cvtsi2sd xmm, r64：F2 REX.W 0F 2A /r（整数 → double） */
void x64_cvtsi2sd(int dst, Reg src)
{
    check_xmm(dst);
    sse_emit(0xF2, 1, 0x2A, dst, src, "cvtsi2sd %s, %s", xmm_names[dst], reg64_names[src]);
}

/* cvtsi2ss xmm, r64：F3 REX.W 0F 2A /r（整数 → float） */
void x64_cvtsi2ss(int dst, Reg src)
{
    check_xmm(dst);
    sse_emit(0xF3, 1, 0x2A, dst, src, "cvtsi2ss %s, %s", xmm_names[dst], reg64_names[src]);
}

/* cvtss2sd xmm, xmm：F3 0F 5A /r（float → double） */
void x64_cvtss2sd(int dst, int src)
{
    check_xmm(dst);
    check_xmm(src);
    sse_emit(0xF3, 0, 0x5A, dst, src, "cvtss2sd %s, %s", xmm_names[dst], xmm_names[src]);
}

/* cvtsd2ss xmm, xmm：F2 0F 5A /r（double → float） */
void x64_cvtsd2ss(int dst, int src)
{
    check_xmm(dst);
    check_xmm(src);
    sse_emit(0xF2, 0, 0x5A, dst, src, "cvtsd2ss %s, %s", xmm_names[dst], xmm_names[src]);
}

/* cvtsd2si r64, xmm：F2 REX.W 0F 2D /r（double → 整数。最も近い整数に丸める） */
void x64_cvtsd2si(Reg dst, int src)
{
    check_xmm(src);
    sse_emit(0xF2, 1, 0x2D, dst, src, "cvtsd2si %s, %s", reg64_names[dst], xmm_names[src]);
}

/* ucomisd xmm, xmm：66 0F 2E /r（比較。NaN を含むとパリティが立つ） */
void x64_ucomisd(int a, int b)
{
    check_xmm(a);
    check_xmm(b);
    sse_emit(0x66, 0, 0x2E, a, b, "ucomisd %s, %s", xmm_names[a], xmm_names[b]);
}

/* repe cmpsb：rsi と rdi の rcx バイトを比べる（F3 A6） */
void x64_repe_cmpsb(void)
{
    static const unsigned char b[] = { 0xF3, 0xA6 };

    emit(b, sizeof(b), "repe cmpsb");
}

/* xor r/m64, imm：83 /6 ib */
void x64_xor_imm(Reg dst, int32_t imm)
{
    imm_op(6, "xor", dst, imm);
}

/* setcc r/m8：0F 90+cc /0（r は al・cl・dl・bl のみ） */
void x64_setcc(Cond cc, Reg r)
{
    unsigned char b[3];

    if ((int)r > 3) {
        fatal("内部エラー: setcc は al〜bl のみ対応しています");
    }
    b[0] = 0x0F;
    b[1] = (unsigned char)(0x90 + cc);
    b[2] = modrm_reg(0, r);
    emit(b, 3, "set%s %s", cond_suffix(cc), reg8_names[r]);
}

/* movzx r32, r/m8：0F B6 /r（上位は 0 になる。src は al〜bl のみ） */
void x64_movzx8(Reg dst, Reg src)
{
    unsigned char b[4];
    int n;

    if ((int)src > 3) {
        fatal("内部エラー: movzx は al〜bl のみ対応しています");
    }
    n = 0;
    if (ext(dst)) {
        b[n++] = rex(0, dst, src);
    }
    b[n++] = 0x0F;
    b[n++] = 0xB6;
    b[n++] = modrm_reg(dst, src);
    emit(b, (size_t)n, "movzx %s, %s", reg32_names[dst], reg8_names[src]);
}

/* and r/m8, r8：20 /r・or r/m8, r8：08 /r（al〜bl のみ） */
static void op8(unsigned char op, const char *name, Reg dst, Reg src)
{
    unsigned char b[2];

    if ((int)dst > 3 || (int)src > 3) {
        fatal("内部エラー: 8ビット演算は al〜bl のみ対応しています");
    }
    b[0] = op;
    b[1] = modrm_reg(src, dst);
    emit(b, 2, "%s %s, %s", name, reg8_names[dst], reg8_names[src]);
}

void x64_and8(Reg dst, Reg src)
{
    op8(0x20, "and", dst, src);
}

void x64_or8(Reg dst, Reg src)
{
    op8(0x08, "or", dst, src);
}

/* shr r/m64, 1：REX.W D1 /5（符号なしで右に1ビットずらす） */
void x64_shr1(Reg r)
{
    unary_op(0xD1, 5, "shr", r);
}

/* cvttsd2si r64, xmm：F2 REX.W 0F 2C /r（double → 整数。0 の方向へ切り捨てる） */
void x64_cvttsd2si(Reg dst, int src)
{
    check_xmm(src);
    sse_emit(0xF2, 1, 0x2C, dst, src, "cvttsd2si %s, %s", reg64_names[dst], xmm_names[src]);
}

/* sqrtsd xmm, xmm：F2 0F 51 /r（double の平方根） */
void x64_sqrtsd(int dst, int src)
{
    check_xmm(dst);
    check_xmm(src);
    sse_emit(0xF2, 0, 0x51, dst, src, "sqrtsd %s, %s", xmm_names[dst], xmm_names[src]);
}

/* sqrtss xmm, xmm：F3 0F 51 /r（float の平方根） */
void x64_sqrtss(int dst, int src)
{
    check_xmm(dst);
    check_xmm(src);
    sse_emit(0xF3, 0, 0x51, dst, src, "sqrtss %s, %s", xmm_names[dst], xmm_names[src]);
}

/* shl r/m64, imm8：REX.W C1 /4 ib（1 なら REX.W D1 /4） */
void x64_shl_imm(Reg dst, int imm)
{
    unsigned char b[4];

    b[0] = rex(1, RAX, dst);
    b[2] = modrm_reg(4, dst);
    if (imm == 1) {
        b[1] = 0xD1;
        emit(b, 3, "shl %s, 1", reg64_names[dst]);
        return;
    }
    b[1] = 0xC1;
    b[3] = (unsigned char)imm;
    emit(b, 4, "shl %s, %d", reg64_names[dst], imm);
}

/* or r/m64, r64：REX.W 09 /r */
void x64_or(Reg dst, Reg src)
{
    rr_op(0x09, "or", dst, src);
}

/* and r/m64, r64：REX.W 21 /r */
void x64_and(Reg dst, Reg src)
{
    rr_op(0x21, "and", dst, src);
}

/* movzx r32, byte ptr [base+disp]：0F B6 /r（上位は 0 になる） */
void x64_load8(Reg dst, Reg base, int32_t disp)
{
    unsigned char b[16];
    char mem[64];
    int n;

    n = 0;
    if (ext(dst) || ext(base)) {
        b[n++] = rex(0, dst, base);
    }
    b[n++] = 0x0F;
    b[n++] = 0xB6;
    n += modrm_mem(b + n, dst, base, disp);
    emit(b, (size_t)n, "movzx %s, byte ptr %s", reg32_names[dst], mem_text(mem, sizeof(mem), base, disp));
}

/* mul r/m64：REX.W F7 /4（rdx:rax = rax × r、符号なし） */
void x64_mul(Reg r)
{
    unary_op(0xF7, 4, "mul", r);
}

/* bsr r64, r/m64：REX.W 0F BD /r（最上位の 1 のビットの位置） */
void x64_bsr(Reg dst, Reg src)
{
    unsigned char b[4];

    b[0] = rex(1, dst, src);
    b[1] = 0x0F;
    b[2] = 0xBD;
    b[3] = modrm_reg(dst, src);
    emit(b, 4, "bsr %s, %s", reg64_names[dst], reg64_names[src]);
}

/* シフト（cl の数だけ）：REX.W D3 /ext */
static void shift_cl(int ext_code, const char *name, Reg dst)
{
    unsigned char b[3];

    b[0] = rex(1, RAX, dst);
    b[1] = 0xD3;
    b[2] = modrm_reg(ext_code, dst);
    emit(b, 3, "%s %s, cl", name, reg64_names[dst]);
}

void x64_shl_cl(Reg dst)
{
    shift_cl(4, "shl", dst);
}

void x64_shr_cl(Reg dst)
{
    shift_cl(5, "shr", dst);
}

void x64_sar_cl(Reg dst)
{
    shift_cl(7, "sar", dst);
}
