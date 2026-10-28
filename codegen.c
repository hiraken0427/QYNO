/*
 * コード生成
 *
 * 実行ファイルの .text は次の順に並ぶ。
 *   _start              エントリ処理（main を呼び、戻り値で終了する）
 *   .L.qy_write         rsi が指す rdx バイトを標準出力に書く
 *   .L.qy_print_int     rax の整数を10進数で書く
 *   .L.qy_print_double  rax（double のビット列）を10進数で書く
 *   .L.qy_print_float   rax（float を double に変換したビット列）を有効数字7桁で書く
 *   .L.qy_print_bool    rax が 0 なら false、それ以外なら true と書く
 *   .L.qy_print_str     rax が指す str を書く
 *   .L.qy_alloc         rdi バイトのメモリを確保し、先頭を rax に返す
 *   .L.qy_concat        rax と rdi の str を連結した新しい str を rax に返す
 *   .L.qy_str_eq        rdi と rax の str が等しければ 1、違えば 0 を rax に返す
 *   .L.qy_pow_int       rax（int）の rdi 乗を rax に返す
 *   .L.qy_pow_double    rax（double）の rdi 乗を rax に返す
 *   .L.qy_pow_float     rax（float）の rdi 乗を rax に返す
 *   .L.qy_write_err     rsi が指す rdx バイトを標準エラー出力に書く
 *   .L.qy_runtime_error rsi が指す rdx バイトのメッセージを標準エラー出力に書き、終了コード r8 で終わる
 *   .L.qy_readline など 入力（00.22）。標準入力から1行読み、形に当てはめて値を読み取る
 *   .L.qy_round.*       rax（double）を rdi 桁で丸める（四捨五入・切り上げ・切り捨て）
 *
 * 実行時エラーで途中から終わるときのため、エントリ処理は main を呼ぶ前の rsp を r15 に残す。
 * r15 は生成するコードでは使わず、Windows の API もシステムコールも壊さない。
 *   関数                ユーザーのプログラムの各関数（main を含む）
 *
 * 関数の呼び出し：呼び出し側が引数を左から順に積んで call し、戻ったら積んだ分を取り除く。
 * 呼ばれた側では i 番目（0 始まり）の引数が [rbp + 16 + 8 * (引数の数 - 1 - i)] にある。
 * 戻り値は rax に入れる。
 *
 * 値はすべて rax に 64 ビットで置く。
 *   int：整数、bool：0 か 1、str/char：[長さ 8バイト][本体] の先頭アドレス
 *   配列：[長さ][確保容量][各次元の制約][要素の領域] の先頭アドレス
 *   double：IEEE 754 倍精度のビット列、float：単精度のビット列（下位32ビット）
 * 浮動小数点の計算は、その都度 xmm0・xmm1 に移して行う。
 *
 * 実行時ルーチンは rax・rcx・rdx・rsi・rdi・r8〜r11・xmm0〜xmm2 を壊してよい。
 * ローカル変数は [rbp - offset] に置く。
 * r12 は管理領域のリスト、r13 は前回回収後の確保量、r14 は入力状態と診断の情報。
 * 管理領域の確保は値レジスタを退避し、スタックと到達可能な領域を保守的に走査して回収する。
 */

#include "qyc.h"
#include "wide_blob.h"
#include <math.h>

#include <stdlib.h>
#include <string.h>

#define STD_OUTPUT_HANDLE (-11)
#define STD_ERROR_HANDLE (-12)
#define TWO_POW_52_BITS 0x4330000000000000LL   /* 2^52（これ以上の double は整数） */
#define CP_UTF8_VALUE 65001
#define FRAC_DIGITS 6               /* 小数点以下の表示桁数（末尾の 0 は省く） */
#define FLOAT_DIGITS 7              /* float の有効数字 */

static Program *prog;
static FILE *text;
static Target target;
static void gen_grapheme_call(void);
static void gen_input_state_init(void);
static void gen_lineedit_init(void);
static void lineedit_labels(void);
static int edit_line_label;
static void gen_fail(Token *tok, const char *text, int code);
static void gen_store_var(Var *v, Token *tok);
static void gen_var_addr(Var *v, Token *tok, int check);
static void gen_load_var(Var *v, Token *tok);
static void gen_print_value(Node *expr);

/* 実行時エラーの終了コード（sysexits.h の取り決め） */
#define EX_DATAERR  65      /* sysexits.h：入力のデータが正しくない */
#define EX_SOFTWARE 70      /* sysexits.h：プログラムの内部の誤り */
#define EX_IOERR    74      /* sysexits.h：入出力の失敗 */

#define OUTPUT_SINK 288
static int file_labels[BI_COUNT - BI_FILE_OPEN];
static int file_write_all_label;
static int file_error_label, file_validate_label, file_path_label, file_raw_close_label;
static int file_finalize_label, file_cleanup_label, file_kind_label, file_raw_open_label;
static int file_check_label, file_append_label, file_string_label, file_capture_label;
static int file_getc_label, file_text_label;
static void gen_file_builtin(Node *node);
static void gen_file_context(Node *node);

static int depth;
static int main_label;
static int write_label;
static int print_int_label;
static int complex_make_labels[2], complex_op_labels[2][4], complex_pow_labels[2], complex_print_labels[2];
static int complex_sqrt_labels[2];
static int complex_equal_label, complex_abs_label, complex_parse_labels[2], complex_real_label;
static int parse_real_label, skip_ws_label, word_follows_label;
static int print_uint_label;
static int parse_int_label;
static int integer_format_label;
static int print_double_label;
static int print_float_label;
static int print_bool_label;
static int print_str_label;
static int print_addr_label;
static int alloc_label;
static int raw_alloc_label;
static int raw_free_label;
static int gc_mark_label;
static int gc_collect_label;
static int array_copy_label, array_slot_label, array_get_label;
static int array_add_label, array_remove_label, array_clear_label;
static int array_equal_label, array_print_label, quoted_string_label;
static int array_ref_label, string_get_label, string_replace_label;
static Function *emitting_function;
static Model *emitting_models;
static int concat_label;
static int str_eq_label;
static int utf8_next_checked_label;
static int string_scan_label;
static int pow_int_label;
static int pow_double_label;
static int pow_float_label;
static int write_err_label;
static int runtime_error_label;
static int exit_label;
static int round_labels[3];         /* 四捨五入・切り上げ・切り捨て */
static int round_integer_labels[2][3]; /* int 専用。double へ変換しない */
static int return_label;
static int label_seq;

/* 今いるループの break() と continue() の飛び先（内側ほど後ろ） */
#define MAX_LOOP_DEPTH 256
static int break_labels[MAX_LOOP_DEPTH];
static int continue_labels[MAX_LOOP_DEPTH];
static int loop_depth;
static size_t newline_offset;
static size_t empty_str_offset;
static size_t zero_char_offset;

static long long double_bits(double d)
{
    uint64_t bits;

    memcpy(&bits, &d, sizeof(bits));
    return (long long)bits;
}

static long long float_bits(float f)
{
    uint32_t bits;

    memcpy(&bits, &f, sizeof(bits));
    return (long long)bits;
}

/* data の offset から len バイトを出力する */
static void gen_write_data(size_t offset, size_t len)
{
    x64_lea_data(RSI, offset);
    x64_mov_imm(RDX, (long long)len);
    x64_call(write_label);
}

/* 固定の文字列を出力する */
static void gen_write_const(const char *s)
{
    size_t len;

    len = strlen(s);
    gen_write_data(prog_add_data(prog, s, len), len);
}

/* ---------- エントリ処理 ---------- */

static void gen_entry_windows(void)
{
    /* 入口では rsp が 16 の倍数 + 8。40 引いてシャドウ領域と境界合わせを行う */
    x64_sub_imm(RSP, 40);

    /* コンソールを UTF-8 に切り替え、元の設定を [rsp+32] に保存する */
    x64_call_import(IMP_GET_CONSOLE_OUTPUT_CP);
    x64_store(RSP, 32, RAX);
    x64_mov_imm(RCX, CP_UTF8_VALUE);
    x64_call_import(IMP_SET_CONSOLE_OUTPUT_CP);

    x64_xor(R12, R12);
    x64_xor(R13, R13);
    x64_xor(R14, R14);
    x64_mov(R15, RSP);
    gen_input_state_init();     /* 入力の状態の領域を r14 に */
    x64_call(main_label);

    /* ここから終了処理（rax が終了コード。実行時エラーもここへ来る） */
    x64_bind(exit_label);
    x64_mov(RSP, R15);
    x64_mov(RBX, RAX);
    x64_call(file_cleanup_label);

    /* コンソールの設定を戻し、main の戻り値で終了する */
    x64_load(RCX, RSP, 32);
    x64_call_import(IMP_SET_CONSOLE_OUTPUT_CP);
    x64_mov(RCX, RBX);
    x64_call_import(IMP_EXIT_PROCESS);
    x64_int3();
}

static void gen_entry_linux(void)
{
    x64_xor(R12, R12);
    x64_xor(R13, R13);
    x64_xor(R14, R14);
    x64_mov(R15, RSP);
    gen_input_state_init();
    x64_call(main_label);
    x64_bind(exit_label);
    x64_mov(RSP, R15);
    x64_mov(RBX, RAX);
    x64_call(file_cleanup_label);
    x64_mov(RDI, RBX);
    x64_mov_imm(RAX, 60);       /* exit */
    x64_syscall();
}

/* ---------- 実行時ルーチン ---------- */

/* rsi が指す rdx バイトを標準出力（to_err なら標準エラー出力）に書く */
static void gen_write(int label, int to_err)
{
    x64_bind(label);
    if (!to_err) {
        int normal = x64_new_label(".L.qy_write.stdout");
        x64_load(R11, R14, OUTPUT_SINK); x64_test(R11, R11);
        x64_jcc(CC_Z, normal); x64_jmp(file_capture_label); x64_bind(normal);
    }
    if (target == TARGET_LINUX) {
        x64_mov_imm(RDI, to_err ? 2 : 1);   /* 標準エラー出力・標準出力 */
        x64_mov_imm(RAX, 1);    /* write */
        x64_syscall();
        x64_ret();
        return;
    }

    /* Windows：WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), rsi, rdx, &written, NULL) */
    x64_push(RBP);
    x64_mov(RBP, RSP);
    x64_push(RSI);              /* [rbp-8]  書き出す位置 */
    x64_push(RDX);              /* [rbp-16] 長さ */
    x64_and_imm(RSP, -16);      /* 呼び出し前に 16 バイト境界へそろえる */
    x64_sub_imm(RSP, 48);       /* シャドウ領域 32 + 第5引数 8 + 調整 8 */
    x64_mov_imm(RCX, to_err ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
    x64_call_import(IMP_GET_STD_HANDLE);
    x64_mov(RCX, RAX);
    x64_load(RDX, RBP, -8);
    x64_load(R8, RBP, -16);
    x64_lea(R9, RBP, -24);      /* 書き込んだバイト数の受け取り先 */
    x64_store_imm(RSP, 32, 0);  /* lpOverlapped = NULL */
    x64_call_import(IMP_WRITE_FILE);
    x64_mov(RSP, RBP);
    x64_pop(RBP);
    x64_ret();
}

/* rax の整数を10進数で標準出力に書く */
static void gen_print_int(void)
{
    int positive;
    int loop;
    int out;

    positive = x64_new_label(".L.qy_print_int.positive");
    loop = x64_new_label(".L.qy_print_int.loop");
    out = x64_new_label(".L.qy_print_int.out");

    x64_bind(print_int_label);
    x64_push(RBP);
    x64_mov(RBP, RSP);
    x64_sub_imm(RSP, 32);       /* [rbp-32]〜[rbp-1] を文字の組み立てに使う */

    /* 負の数は絶対値にする（最小値も符号なしとして正しく扱える） */
    x64_mov(R8, RAX);
    x64_test(RAX, RAX);
    x64_jcc(CC_NS, positive);
    x64_neg(RAX);
    x64_bind(positive);

    /* 下の桁から順に、末尾から前へ書いていく */
    x64_mov(RDI, RBP);
    x64_mov_imm(RCX, 10);
    x64_bind(loop);
    x64_xor(RDX, RDX);
    x64_div(RCX);
    x64_add_imm(RDX, '0');
    x64_dec(RDI);
    x64_store8(RDI, 0, RDX);
    x64_test(RAX, RAX);
    x64_jcc(CC_NZ, loop);

    /* 元の値が負なら '-' を付ける */
    x64_test(R8, R8);
    x64_jcc(CC_NS, out);
    x64_dec(RDI);
    x64_store8_imm(RDI, 0, '-');
    x64_bind(out);

    x64_mov(RSI, RDI);
    x64_mov(RDX, RBP);
    x64_sub(RDX, RDI);
    x64_call(write_label);
    x64_mov(RSP, RBP);
    x64_pop(RBP);
    x64_ret();
}

static void gen_print_uint(void)
{
    x64_bind(print_uint_label); x64_push(RBP); x64_mov(RBP, RSP);
    x64_mov_imm(RDI, 10); x64_xor(RSI, RSI); x64_mov_imm(R11, 1); x64_call(integer_format_label);
    x64_call(print_str_label); x64_pop(RBP); x64_ret();
}

/* [rbp+disp] の整数に add を足す */
static void gen_add_local(int32_t disp, int32_t add)
{
    x64_load(RAX, RBP, disp);
    x64_add_imm(RAX, add);
    x64_store(RBP, disp, RAX);
}

/*
 * rax（double のビット列）を10進数で標準出力に書く
 *
 * 小数点以下は FRAC_DIGITS 桁に丸め、末尾の 0 を省く（整数値なら ".0" を付ける）。
 * float（.L.qy_print_float から入った場合）は有効数字 FLOAT_DIGITS 桁になるよう、
 * 整数部の桁数に応じて小数点以下の桁数を減らす。
 * 絶対値が 1e12 以上、または 1e-4 未満（0 を除く）のときは「仮数e指数」の形にする。
 * NaN は nan、無限大は inf と書く。
 *
 * [rbp-8]  絶対値のビット列  [rbp-16] 指数  [rbp-24] 小数部  [rbp-32] 小数部の桁数
 * [rbp-48]〜[rbp-41] 小数部の文字の組み立て  [rbp-56] float なら 1  [rbp-64] 10 の桁数乗
 */
static void gen_print_double(void)
{
    int common;
    int float_digits;
    int decide_scale;
    int pow_loop;
    int scaled;
    int nonneg;
    int small_loop;
    int check_big;
    int big_loop;
    int fixed;
    int print_ip;
    int frac;
    int trim;
    int emit_frac;
    int frac_loop;
    int exponent;
    int exp_neg;
    int done;
    int nan;
    int inf;

    nonneg = x64_new_label(".L.qy_print_double.nonneg");
    small_loop = x64_new_label(".L.qy_print_double.small");
    check_big = x64_new_label(".L.qy_print_double.check_big");
    big_loop = x64_new_label(".L.qy_print_double.big");
    fixed = x64_new_label(".L.qy_print_double.fixed");
    print_ip = x64_new_label(".L.qy_print_double.print_ip");
    frac = x64_new_label(".L.qy_print_double.frac");
    trim = x64_new_label(".L.qy_print_double.trim");
    emit_frac = x64_new_label(".L.qy_print_double.emit_frac");
    frac_loop = x64_new_label(".L.qy_print_double.frac_loop");
    exponent = x64_new_label(".L.qy_print_double.exponent");
    exp_neg = x64_new_label(".L.qy_print_double.exp_neg");
    done = x64_new_label(".L.qy_print_double.done");
    nan = x64_new_label(".L.qy_print_double.nan");
    inf = x64_new_label(".L.qy_print_double.inf");
    common = x64_new_label(".L.qy_print_double.common");
    float_digits = x64_new_label(".L.qy_print_double.float_digits");
    decide_scale = x64_new_label(".L.qy_print_double.decide_scale");
    pow_loop = x64_new_label(".L.qy_print_double.pow");
    scaled = x64_new_label(".L.qy_print_double.scaled");

    /* 入口：float か double かを r9 で区別する */
    x64_bind(print_float_label);
    x64_mov_imm(R9, 1);
    x64_jmp(common);
    x64_bind(print_double_label);
    x64_xor(R9, R9);
    x64_bind(common);
    x64_push(RBP);
    x64_mov(RBP, RSP);
    x64_sub_imm(RSP, 64);
    x64_store(RBP, -56, R9);

    /* NaN（自分自身と比べて順序が付かない） */
    x64_movq_to_xmm(0, RAX);
    x64_ucomisd(0, 0);
    x64_jcc(CC_P, nan);

    /* 符号：負なら '-' を書いて絶対値にする */
    x64_test(RAX, RAX);
    x64_jcc(CC_NS, nonneg);
    x64_btr(RAX, 63);
    x64_store(RBP, -8, RAX);
    gen_write_const("-");
    x64_load(RAX, RBP, -8);
    x64_bind(nonneg);
    x64_store(RBP, -8, RAX);

    /* 無限大 */
    x64_mov_imm(RCX, 0x7FF0000000000000LL);
    x64_cmp(RAX, RCX);
    x64_jcc(CC_Z, inf);

    /* 指数を決める（0 はそのまま） */
    x64_store_imm(RBP, -16, 0);
    x64_movq_to_xmm(0, RAX);
    x64_test(RAX, RAX);
    x64_jcc(CC_Z, fixed);

    /* 1e-4 未満なら 1 以上になるまで 10 倍する */
    x64_mov_imm(RCX, double_bits(1e-4));
    x64_movq_to_xmm(1, RCX);
    x64_ucomisd(0, 1);
    x64_jcc(CC_AE, check_big);
    x64_mov_imm(RCX, double_bits(10.0));
    x64_movq_to_xmm(1, RCX);
    x64_mov_imm(RCX, double_bits(1.0));
    x64_movq_to_xmm(2, RCX);
    x64_bind(small_loop);
    x64_sse_sd(SSE_MUL, 0, 1);
    gen_add_local(-16, -1);
    x64_ucomisd(0, 2);
    x64_jcc(CC_B, small_loop);
    x64_jmp(fixed);

    /* 1e12 以上なら 10 未満になるまで 10 で割る */
    x64_bind(check_big);
    x64_mov_imm(RCX, double_bits(1e12));
    x64_movq_to_xmm(1, RCX);
    x64_ucomisd(0, 1);
    x64_jcc(CC_B, fixed);
    x64_mov_imm(RCX, double_bits(10.0));
    x64_movq_to_xmm(1, RCX);
    x64_bind(big_loop);
    x64_sse_sd(SSE_DIV, 0, 1);
    gen_add_local(-16, 1);
    x64_ucomisd(0, 1);
    x64_jcc(CC_AE, big_loop);

    /* 小数点以下の桁数を決める（float は整数部の桁数ぶん減らして有効数字7桁にする） */
    x64_bind(fixed);
    x64_store_imm(RBP, -32, FRAC_DIGITS);
    x64_load(RAX, RBP, -56);
    x64_test(RAX, RAX);
    x64_jcc(CC_Z, decide_scale);
    x64_store_imm(RBP, -32, FLOAT_DIGITS - 1);
    x64_mov_imm(RCX, double_bits(10.0));
    x64_movq_to_xmm(1, RCX);
    x64_movq_to_xmm(2, RCX);
    x64_bind(float_digits);
    x64_ucomisd(0, 1);
    x64_jcc(CC_B, decide_scale);
    gen_add_local(-32, -1);
    x64_jcc(CC_Z, decide_scale);
    x64_sse_sd(SSE_MUL, 1, 2);
    x64_jmp(float_digits);

    /* scale = 10 の桁数乗 */
    x64_bind(decide_scale);
    x64_mov_imm(RAX, 1);
    x64_load(RCX, RBP, -32);
    x64_test(RCX, RCX);
    x64_jcc(CC_Z, scaled);
    x64_mov_imm(RDX, 10);
    x64_bind(pow_loop);
    x64_imul(RAX, RDX);
    x64_dec(RCX);
    x64_jcc(CC_NZ, pow_loop);
    x64_bind(scaled);
    x64_store(RBP, -64, RAX);

    /* 整数部と小数部：round(x * scale) を scale で割る */
    x64_cvtsi2sd(1, RAX);
    x64_sse_sd(SSE_MUL, 0, 1);
    x64_cvtsd2si(RAX, 0);
    x64_xor(RDX, RDX);
    x64_load(RCX, RBP, -64);
    x64_div(RCX);

    /* 指数表記で仮数が丸めにより 10 になった場合は 1 に直す */
    x64_load(RCX, RBP, -16);
    x64_test(RCX, RCX);
    x64_jcc(CC_Z, print_ip);
    x64_mov_imm(RCX, 10);
    x64_cmp(RAX, RCX);
    x64_jcc(CC_B, print_ip);
    x64_mov_imm(RAX, 1);
    x64_xor(RDX, RDX);
    x64_store(RBP, -24, RDX);
    gen_add_local(-16, 1);
    x64_mov_imm(RAX, 1);

    /* 整数部と "." */
    x64_bind(print_ip);
    x64_store(RBP, -24, RDX);
    x64_call(print_int_label);
    gen_write_const(".");

    /* 小数部が 0 なら "0" */
    x64_load(RAX, RBP, -24);
    x64_test(RAX, RAX);
    x64_jcc(CC_NZ, frac);
    gen_write_const("0");
    x64_jmp(exponent);

    /* 末尾の 0 を省き、残りの桁数を数える（[rbp-32] は決めた桁数から始める） */
    x64_bind(frac);
    x64_bind(trim);
    x64_load(RAX, RBP, -24);
    x64_xor(RDX, RDX);
    x64_mov_imm(RCX, 10);
    x64_div(RCX);
    x64_test(RDX, RDX);
    x64_jcc(CC_NZ, emit_frac);
    x64_store(RBP, -24, RAX);
    gen_add_local(-32, -1);
    x64_jmp(trim);

    /* 小数部を、先頭の 0 も含めて桁数ぶん書く */
    x64_bind(emit_frac);
    x64_lea(RDI, RBP, -40);
    x64_load(R8, RBP, -32);
    x64_load(RAX, RBP, -24);
    x64_mov_imm(RCX, 10);
    x64_bind(frac_loop);
    x64_xor(RDX, RDX);
    x64_div(RCX);
    x64_add_imm(RDX, '0');
    x64_dec(RDI);
    x64_store8(RDI, 0, RDX);
    x64_dec(R8);
    x64_jcc(CC_NZ, frac_loop);
    x64_mov(RSI, RDI);
    x64_lea(RDX, RBP, -40);
    x64_sub(RDX, RDI);
    x64_call(write_label);

    /* 指数 */
    x64_bind(exponent);
    x64_load(RAX, RBP, -16);
    x64_test(RAX, RAX);
    x64_jcc(CC_Z, done);
    gen_write_const("e");
    x64_load(RAX, RBP, -16);
    x64_test(RAX, RAX);
    x64_jcc(CC_S, exp_neg);
    gen_write_const("+");
    x64_bind(exp_neg);
    x64_load(RAX, RBP, -16);
    x64_call(print_int_label);
    x64_jmp(done);

    x64_bind(nan);
    gen_write_const("nan");
    x64_jmp(done);

    x64_bind(inf);
    gen_write_const("inf");

    x64_bind(done);
    x64_mov(RSP, RBP);
    x64_pop(RBP);
    x64_ret();
}

/* rax が 0 なら false、それ以外なら true と書く */
static void gen_print_bool(void)
{
    int is_false;
    size_t true_offset;
    size_t false_offset;

    is_false = x64_new_label(".L.qy_print_bool.false");
    true_offset = prog_add_data(prog, "true", 4);
    false_offset = prog_add_data(prog, "false", 5);

    x64_bind(print_bool_label);
    x64_test(RAX, RAX);
    x64_jcc(CC_Z, is_false);
    x64_lea_data(RSI, true_offset);
    x64_mov_imm(RDX, 4);
    x64_jmp(write_label);
    x64_bind(is_false);
    x64_lea_data(RSI, false_offset);
    x64_mov_imm(RDX, 5);
    x64_jmp(write_label);
}

/* rax が指す str（[長さ][本体]）を書く */
static void gen_print_str(void)
{
    x64_bind(print_str_label);
    x64_load(RDX, RAX, 0);
    x64_lea(RSI, RAX, 8);
    x64_jmp(write_label);
}

/*
 * rax の場所を表示する（00.24.0）。何も指していなければ none、それ以外は 0x から始まる16進数（上位の 0 は省く）。
 * [rbp-32]〜[rbp-1] に下の桁から文字を組み立てる。
 */
static void gen_print_addr(void)
{
    int nonzero;
    int loop;
    int digit;
    size_t none_text;

    none_text = prog_add_data(prog, "none", 4);
    nonzero = x64_new_label(".L.qy_print_addr.nonzero");
    loop = x64_new_label(".L.qy_print_addr.loop");
    digit = x64_new_label(".L.qy_print_addr.digit");
    x64_bind(print_addr_label);
    x64_test(RAX, RAX);
    x64_jcc(CC_NZ, nonzero);
    x64_lea_data(RSI, none_text);
    x64_mov_imm(RDX, 4);
    x64_jmp(write_label);
    x64_bind(nonzero);
    x64_push(RBP);
    x64_mov(RBP, RSP);
    x64_sub_imm(RSP, 32);
    x64_lea(RSI, RBP, 0);               /* rsi = 次に書く位置の1つ後ろ */
    x64_bind(loop);
    x64_mov(RCX, RAX);
    x64_and_imm(RCX, 0xF);
    x64_mov_imm(RDX, 10);
    x64_cmp(RCX, RDX);
    x64_jcc(CC_B, digit);
    x64_add_imm(RCX, 'a' - '0' - 10);
    x64_bind(digit);
    x64_add_imm(RCX, '0');
    x64_sub_imm(RSI, 1);
    x64_store8(RSI, 0, RCX);
    x64_shr_imm(RAX, 4);
    x64_test(RAX, RAX);
    x64_jcc(CC_NZ, loop);
    x64_sub_imm(RSI, 1);
    x64_store8_imm(RSI, 0, 'x');
    x64_sub_imm(RSI, 1);
    x64_store8_imm(RSI, 0, '0');
    x64_lea(RDX, RBP, 0);
    x64_sub(RDX, RSI);
    x64_call(write_label);
    x64_mov(RSP, RBP);
    x64_pop(RBP);
    x64_ret();
}

#include "memory_codegen.inc"

/*
 * rax と rdi の str を連結した新しい str を rax に返す
 * [rbp-8] 左  [rbp-16] 右  [rbp-24] 合計の長さ  [rbp-32] 結果
 */
static void gen_concat(void)
{
    x64_bind(concat_label);
    x64_push(RBP);
    x64_mov(RBP, RSP);
    x64_sub_imm(RSP, 32);
    x64_store(RBP, -8, RAX);
    x64_store(RBP, -16, RDI);

    /* 合計の長さを求め、[長さ 8バイト][本体] の領域を確保する */
    x64_load(RCX, RAX, 0);
    x64_load(RDX, RDI, 0);
    x64_add(RCX, RDX);
    x64_store(RBP, -24, RCX);
    x64_lea(RDI, RCX, 8);
    x64_call(alloc_label);
    x64_store(RBP, -32, RAX);
    x64_load(RCX, RBP, -24);
    x64_store(RAX, 0, RCX);

    /* 左、右の順に本体を複写する */
    x64_lea(RDI, RAX, 8);
    x64_load(RSI, RBP, -8);
    x64_load(RCX, RSI, 0);
    x64_add_imm(RSI, 8);
    x64_rep_movsb();
    x64_load(RSI, RBP, -16);
    x64_load(RCX, RSI, 0);
    x64_add_imm(RSI, 8);
    x64_rep_movsb();

    x64_load(RAX, RBP, -32);
    x64_mov(RSP, RBP);
    x64_pop(RBP);
    x64_ret();
}

/* rdi と rax の str が等しければ 1、違えば 0 を rax に返す */
static void gen_str_eq(void)
{
    int not_equal;

    not_equal = x64_new_label(".L.qy_str_eq.ne");
    x64_bind(str_eq_label);
    x64_load(RCX, RDI, 0);
    x64_load(RDX, RAX, 0);
    x64_cmp(RCX, RDX);
    x64_jcc(CC_NZ, not_equal);  /* 長さが違う */
    x64_lea(RSI, RDI, 8);
    x64_lea(RDI, RAX, 8);
    x64_repe_cmpsb();           /* 長さ 0 なら何もせず、直前の比較（等しい）のまま */
    x64_jcc(CC_NZ, not_equal);
    x64_mov_imm(RAX, 1);
    x64_ret();
    x64_bind(not_equal);
    x64_xor(RAX, RAX);
    x64_ret();
}

/* Unicode の比較に使う即値。幅を持つ rcx は変更しない。 */
static void text_cmp(Reg value, long long immediate)
{
    x64_mov_imm(R9, immediate);
    x64_cmp(value, R9);
}

/* rsi・残りバイト数 rdx を保持し、rax=スカラー、rcx=幅、r8=不正なら1。 */
static void gen_utf8_next_checked(void)
{
    int widths[5];
    int invalid = x64_new_label(".L.qy_utf8_checked.invalid");
    int valid = x64_new_label(".L.qy_utf8_checked.valid");
    int n;
    int i;
    char name[64];

    for (n = 2; n <= 4; n++) {
        snprintf(name, sizeof(name), ".L.qy_utf8_checked.width%d", n);
        widths[n] = x64_new_label(name);
    }
    x64_bind(utf8_next_checked_label);
    x64_test(RDX, RDX);
    x64_jcc(CC_Z, invalid);
    x64_load8(RAX, RSI, 0);
    x64_mov_imm(RCX, 1);
    text_cmp(RAX, 0x80);
    x64_jcc(CC_B, valid);
    text_cmp(RAX, 0xC2);
    x64_jcc(CC_B, invalid);
    text_cmp(RAX, 0xE0);
    x64_jcc(CC_B, widths[2]);
    text_cmp(RAX, 0xF0);
    x64_jcc(CC_B, widths[3]);
    text_cmp(RAX, 0xF4);
    x64_jcc(CC_BE, widths[4]);
    x64_jmp(invalid);
    for (n = 2; n <= 4; n++) {
        x64_bind(widths[n]);
        text_cmp(RDX, n);
        x64_jcc(CC_B, invalid);
        x64_mov_imm(RCX, n);
        x64_and_imm(RAX, n == 2 ? 0x1F : n == 3 ? 0x0F : 0x07);
        for (i = 1; i < n; i++) {
            x64_load8(R8, RSI, i);
            text_cmp(R8, 0x80);
            x64_jcc(CC_B, invalid);
            text_cmp(R8, 0xBF);
            x64_jcc(CC_A, invalid);
            x64_shl_imm(RAX, 6);
            x64_and_imm(R8, 0x3F);
            x64_or(RAX, R8);
        }
        text_cmp(RAX, n == 2 ? 0x80 : n == 3 ? 0x800 : 0x10000);
        x64_jcc(CC_B, invalid);
        if (n == 3) {
            int nonsurrogate = x64_new_label(".L.qy_utf8_checked.nonsurrogate");
            text_cmp(RAX, 0xD800);
            x64_jcc(CC_B, nonsurrogate);
            text_cmp(RAX, 0xDFFF);
            x64_jcc(CC_BE, invalid);
            x64_bind(nonsurrogate);
        }
        text_cmp(RAX, 0x10FFFF);
        x64_jcc(CC_A, invalid);
        x64_jmp(valid);
    }
    x64_bind(invalid);
    x64_mov_imm(R8, 1);
    x64_ret();
    x64_bind(valid);
    x64_xor(R8, R8);
    x64_ret();
}

/* str rax を全体検証。rdi=添字（負なら数えるだけ）。
 * 結果 rax=文字数、rdx=指定文字の先頭（範囲外なら0）、rcx=そのバイト数、r8=不正なら1。
 */
static void gen_string_scan(void)
{
    int done=x64_new_label(".L.qy_string_scan.done");
    rt_enter(string_scan_label,2);rt_save(1,RAX);
    x64_load(RDX,RAX,0);x64_lea(RSI,RAX,8);rt_save(2,RSI);x64_add(RDX,RSI);
    gen_grapheme_call();x64_mov(RCX,R9);x64_mov(RDI,RAX);x64_mov(RAX,R10);x64_xor(RDX,RDX);
    x64_test(R8,R8);x64_jcc(CC_NZ,done);x64_test(RCX,RCX);x64_jcc(CC_Z,done);
    rt_load(RDX,2);x64_add(RDX,RDI);x64_bind(done);rt_leave();
}

/*
 * rax（int）の rdi 乗を rax に返す
 * 指数を2進数で見て、1のビットのところで掛ける（r8 が結果、r9 が底の2乗の列）。
 * 負の指数は底 1 と -1 だけを受け付ける（呼出し元で定義域を検査済み）。
 */
static void gen_pow_int(void)
{
    int loop;
    int skip;
    int done;
    int negative;
    int ret1;

    loop = x64_new_label(".L.qy_pow_int.loop");
    skip = x64_new_label(".L.qy_pow_int.skip");
    done = x64_new_label(".L.qy_pow_int.done");
    negative = x64_new_label(".L.qy_pow_int.negative");
    ret1 = x64_new_label(".L.qy_pow_int.ret1");

    x64_bind(pow_int_label);
    x64_mov(RCX, RDI);
    x64_test(RCX, RCX);
    x64_jcc(CC_S, negative);
    x64_mov_imm(R8, 1);
    x64_mov(R9, RAX);
    x64_bind(loop);
    x64_test(RCX, RCX);
    x64_jcc(CC_Z, done);
    x64_mov(RDX, RCX);
    x64_and_imm(RDX, 1);
    x64_jcc(CC_Z, skip);
    x64_imul(R8, R9);
    x64_bind(skip);
    x64_imul(R9, R9);
    x64_shr_imm(RCX, 1);
    x64_jmp(loop);
    x64_bind(done);
    x64_mov(RAX, R8);
    x64_ret();

    x64_bind(negative);
    x64_mov_imm(RDX, 1);
    x64_cmp(RAX, RDX);
    x64_jcc(CC_Z, ret1);
    x64_and_imm(RDI, 1);        /* 底が -1：指数が偶数なら 1、奇数なら -1 */
    x64_jcc(CC_Z, ret1);
    x64_mov_imm(RAX, -1);
    x64_ret();
    x64_bind(ret1);
    x64_mov_imm(RAX, 1);
    x64_ret();
}

/*
 * rax（double または float のビット列）の rdi 乗を rax に返す
 * xmm0 が結果、xmm1 が底の2乗の列。負の指数は最後に逆数にする。
 */
static void gen_pow_real(int is_float)
{
    int positive;
    int loop;
    int skip;
    int done;
    int out;
    const char *prefix;
    char name[64];

    prefix = is_float ? ".L.qy_pow_float" : ".L.qy_pow_double";
    snprintf(name, sizeof(name), "%s.positive", prefix);
    positive = x64_new_label(name);
    snprintf(name, sizeof(name), "%s.loop", prefix);
    loop = x64_new_label(name);
    snprintf(name, sizeof(name), "%s.skip", prefix);
    skip = x64_new_label(name);
    snprintf(name, sizeof(name), "%s.done", prefix);
    done = x64_new_label(name);
    snprintf(name, sizeof(name), "%s.out", prefix);
    out = x64_new_label(name);

    x64_bind(is_float ? pow_float_label : pow_double_label);
    if (is_float) {
        x64_movd_to_xmm(1, RAX);
        x64_mov_imm(RAX, float_bits(1.0f));
        x64_movd_to_xmm(0, RAX);
    } else {
        x64_movq_to_xmm(1, RAX);
        x64_mov_imm(RAX, double_bits(1.0));
        x64_movq_to_xmm(0, RAX);
    }
    x64_mov(R8, RDI);           /* 指数の符号を残しておく */
    x64_mov(RCX, RDI);
    x64_test(RCX, RCX);
    x64_jcc(CC_NS, positive);
    x64_neg(RCX);
    x64_bind(positive);

    x64_bind(loop);
    x64_test(RCX, RCX);
    x64_jcc(CC_Z, done);
    x64_mov(RDX, RCX);
    x64_and_imm(RDX, 1);
    x64_jcc(CC_Z, skip);
    if (is_float) {
        x64_sse_ss(SSE_MUL, 0, 1);
    } else {
        x64_sse_sd(SSE_MUL, 0, 1);
    }
    x64_bind(skip);
    if (is_float) {
        x64_sse_ss(SSE_MUL, 1, 1);
    } else {
        x64_sse_sd(SSE_MUL, 1, 1);
    }
    x64_shr_imm(RCX, 1);
    x64_jmp(loop);

    /* 負の指数なら 1 / 結果 */
    x64_bind(done);
    x64_test(R8, R8);
    x64_jcc(CC_NS, out);
    if (is_float) {
        x64_mov_imm(RAX, float_bits(1.0f));
        x64_movd_to_xmm(1, RAX);
        x64_sse_ss(SSE_DIV, 1, 0);
        x64_movd_from_xmm(RAX, 1);
    } else {
        x64_mov_imm(RAX, double_bits(1.0));
        x64_movq_to_xmm(1, RAX);
        x64_sse_sd(SSE_DIV, 1, 0);
        x64_movq_from_xmm(RAX, 1);
    }
    x64_ret();
    x64_bind(out);
    if (is_float) {
        x64_movd_from_xmm(RAX, 0);
    } else {
        x64_movq_from_xmm(RAX, 0);
    }
    x64_ret();
}

/* rsi・rdx のメッセージを標準エラー出力に書き、終了コード 1 でエントリ処理の終了処理へ移る */
static void gen_runtime_error(void)
{
    x64_bind(runtime_error_label);
    x64_push(R8);               /* 終了コード */
    x64_call(write_err_label);
    x64_pop(RAX);
    x64_jmp(exit_label);
}

/*
 * xmm0 の double を整数値に丸めて xmm0 に返す（mode：四捨五入・切り上げ・切り捨て）
 * 絶対値が 2^52 以上・無限大・NaN はそのまま返す。xmm2 は壊さない。
 */
static void gen_round_integral(int label, Builtin mode)
{
    int done;
    int plus;
    int minus;
    int keep;
    char name[64];

    snprintf(name, sizeof(name), ".L.qy_round_int.%d.plus", (int)mode);
    plus = x64_new_label(name);
    snprintf(name, sizeof(name), ".L.qy_round_int.%d.minus", (int)mode);
    minus = x64_new_label(name);
    snprintf(name, sizeof(name), ".L.qy_round_int.%d.done", (int)mode);
    done = x64_new_label(name);
    snprintf(name, sizeof(name), ".L.qy_round_int.%d.keep", (int)mode);
    keep = x64_new_label(name);

    x64_bind(label);
    x64_movq_from_xmm(RAX, 0);
    x64_mov(RCX, RAX);
    x64_btr(RCX, 63);
    x64_mov_imm(RDX, TWO_POW_52_BITS);
    x64_cmp(RCX, RDX);
    x64_jcc(CC_AE, keep);           /* すでに整数（または NaN・無限大） */

    x64_cvttsd2si(RCX, 0);
    x64_cvtsi2sd(1, RCX);           /* xmm1 = 0 の方向へ切り捨てた値 t */
    x64_movq_to_xmm(3, RAX);        /* xmm3 = x */
    if (mode == BI_ROUND) {
        x64_sse_sd(SSE_SUB, 3, 1);  /* xmm3 = x - t（-1 より大きく 1 より小さい） */
        x64_mov_imm(RDX, double_bits(0.5));
        x64_movq_to_xmm(0, RDX);
        x64_ucomisd(3, 0);
        x64_jcc(CC_AE, plus);
        x64_mov_imm(RDX, double_bits(-0.5));
        x64_movq_to_xmm(0, RDX);
        x64_ucomisd(3, 0);
        x64_jcc(CC_BE, minus);
        x64_jmp(done);
    } else if (mode == BI_ROUNDUP) {
        x64_ucomisd(1, 3);          /* t >= x なら t のまま、t < x なら t + 1 */
        x64_jcc(CC_AE, done);
        x64_jmp(plus);
    } else {
        x64_ucomisd(1, 3);          /* t <= x なら t のまま、t > x なら t - 1 */
        x64_jcc(CC_BE, done);
        x64_jmp(minus);
    }
    x64_bind(plus);
    x64_mov_imm(RDX, double_bits(1.0));
    x64_movq_to_xmm(3, RDX);
    x64_sse_sd(SSE_ADD, 1, 3);
    x64_jmp(done);
    x64_bind(minus);
    x64_mov_imm(RDX, double_bits(1.0));
    x64_movq_to_xmm(3, RDX);
    x64_sse_sd(SSE_SUB, 1, 3);
    x64_bind(done);
    x64_movq_from_xmm(RAX, 1);
    x64_movq_to_xmm(0, RAX);
    x64_bind(keep);
    x64_ret();
}

/*
 * rax（double）を rdi 桁で丸めて rax に返す
 * s = 10^|桁数|。桁数が 0 以上なら x * s を丸めて s で割り、負なら x / s を丸めて s を掛ける。
 * [rbp-8] x  [rbp-16] 桁数
 */
static void gen_round(Builtin mode)
{
    int integral;
    int positive;
    int negative_digits;
    int out;
    char name[64];

    snprintf(name, sizeof(name), ".L.qy_round.%d.integral", (int)mode);
    integral = x64_new_label(name);
    snprintf(name, sizeof(name), ".L.qy_round.%d.positive", (int)mode);
    positive = x64_new_label(name);
    snprintf(name, sizeof(name), ".L.qy_round.%d.negative_digits", (int)mode);
    negative_digits = x64_new_label(name);
    snprintf(name, sizeof(name), ".L.qy_round.%d.out", (int)mode);
    out = x64_new_label(name);

    x64_bind(round_labels[mode - BI_ROUND]);
    x64_push(RBP);
    x64_mov(RBP, RSP);
    x64_sub_imm(RSP, 16);
    x64_store(RBP, -8, RAX);
    x64_store(RBP, -16, RDI);

    /* s = 10^|桁数|（累乗ルーチンを使う） */
    x64_mov(RCX, RDI);
    x64_test(RCX, RCX);
    x64_jcc(CC_NS, positive);
    x64_neg(RCX);
    x64_bind(positive);
    x64_mov(RDI, RCX);
    x64_mov_imm(RAX, double_bits(10.0));
    x64_call(pow_double_label);
    x64_movq_to_xmm(2, RAX);        /* xmm2 = s */

    x64_load(RAX, RBP, -8);
    x64_movq_to_xmm(0, RAX);
    x64_load(RCX, RBP, -16);
    x64_test(RCX, RCX);
    x64_jcc(CC_S, negative_digits);
    x64_sse_sd(SSE_MUL, 0, 2);
    x64_call(integral);
    x64_sse_sd(SSE_DIV, 0, 2);
    x64_jmp(out);
    x64_bind(negative_digits);
    x64_sse_sd(SSE_DIV, 0, 2);
    x64_call(integral);
    x64_sse_sd(SSE_MUL, 0, 2);
    x64_bind(out);
    x64_movq_from_xmm(RAX, 0);
    x64_mov(RSP, RBP);
    x64_pop(RBP);
    x64_ret();

    gen_round_integral(integral, mode);
}

/* int 専用の丸めで使う、一意なラベルを作る */
static int integer_round_label(Builtin mode, int unsign, const char *suffix)
{
    char name[96];

    snprintf(name, sizeof(name), ".L.qy_round_integer.%d.%d.%s", (int)mode, unsign, suffix);
    return x64_new_label(name);
}

/*
 * rax の int を rdi 桁で丸める。成功は rdx=0、範囲外は rdx=1。
 * type.c の round_integer と同じ商・余りの算法を機械語にする。
 * r8=元の値、r9=絶対値、r10=10^k。k=19 も符号なしで扱う。
 */
static void gen_round_integer(Builtin mode, int unsign)
{
    int magnitude;
    int scale_loop;
    int huge;
    int rounded;
    int limit;
    int success;
    int zero;
    int overflow;

    magnitude = integer_round_label(mode, unsign, "magnitude");
    scale_loop = integer_round_label(mode, unsign, "scale_loop");
    huge = integer_round_label(mode, unsign, "huge");
    rounded = integer_round_label(mode, unsign, "rounded");
    limit = integer_round_label(mode, unsign, "limit");
    success = integer_round_label(mode, unsign, "success");
    zero = integer_round_label(mode, unsign, "zero");
    overflow = integer_round_label(mode, unsign, "overflow");

    x64_bind(round_integer_labels[unsign][mode - BI_ROUND]);
    x64_test(RDI, RDI);
    x64_jcc(CC_NS, success);           /* 非負の桁数は元の整数を保持 */
    x64_test(RAX, RAX);
    x64_jcc(CC_Z, success);
    x64_mov(R8, RAX);
    if (!unsign) { x64_jcc(CC_NS, magnitude); x64_neg(RAX); }                     /* |最小値| も符号なしなら表現できる */
    x64_bind(magnitude);
    x64_mov(R9, RAX);
    x64_mov_imm(RCX, -19);
    x64_cmp(RDI, RCX);
    x64_jcc(CC_L, huge);              /* 最小の桁数を neg しない */
    x64_neg(RDI);
    x64_mov_imm(R10, 1);
    x64_mov_imm(RCX, 10);
    x64_bind(scale_loop);
    x64_imul(R10, RCX);
    x64_dec(RDI);
    x64_jcc(CC_NZ, scale_loop);

    x64_mov(RAX, R9);
    x64_xor(RDX, RDX);
    x64_div(R10);                     /* rax=商、rdx=余り */
    if (mode == BI_ROUND) {
        x64_mov(RCX, R10);
        x64_shr1(RCX);
        x64_cmp(RDX, RCX);
        x64_jcc(CC_B, rounded);
    } else {
        x64_test(RDX, RDX);
        x64_jcc(CC_Z, rounded);
        if (unsign) { if (mode == BI_ROUNDDOWN) x64_jmp(rounded); }
        else { x64_test(R8, R8); x64_jcc(mode == BI_ROUNDUP ? CC_S : CC_NS, rounded); }
    }
    x64_add_imm(RAX, 1);
    x64_bind(rounded);
    x64_mul(R10);                     /* rdx:rax = 商 × 10^k */
    x64_test(RDX, RDX);
    x64_jcc(CC_NZ, overflow);
    if (!unsign) {
        x64_mov_imm(R11, 0x7FFFFFFFFFFFFFFFLL);
        x64_test(R8, R8);
        x64_jcc(CC_NS, limit);
        x64_mov_imm(R11, (long long)0x8000000000000000ULL);
        x64_bind(limit);
        x64_cmp(RAX, R11);
        x64_jcc(CC_A, overflow);
        x64_test(R8, R8);
        x64_jcc(CC_NS, success);
        x64_neg(RAX);
    } else x64_bind(limit);
    x64_bind(success);
    x64_xor(RDX, RDX);
    x64_ret();

    x64_bind(huge);
    if (mode != BI_ROUND) {
        if (unsign) { if (mode == BI_ROUNDUP) x64_jmp(overflow); }
        else { x64_test(R8, R8); x64_jcc(mode == BI_ROUNDUP ? CC_NS : CC_S, overflow); }
    }
    x64_bind(zero);
    x64_xor(RAX, RAX);
    x64_jmp(success);
    x64_bind(overflow);
    x64_mov_imm(RDX, 1);
    x64_ret();
}

/* ---------- 式 ---------- */

/* 重ならないラベルを作る */
static int new_label(const char *kind)
{
    char name[64];

    snprintf(name, sizeof(name), ".L.%s.%d", kind, label_seq++);
    return x64_new_label(name);
}

static void push(void)
{
    x64_push(RAX);
    depth++;
}

static void pop(Reg r)
{
    x64_pop(r);
    depth--;
}

static void gen_expr(Node *node);
static void gen_input_state_init(void);

#include "wide_codegen.inc"

/* All integer values are canonical sign/zero extended in the scalar ABI. */
static void gen_integer_normalize(Type ty)
{
    int bits = integer_width(ty);
    if (bits >= 64) return;
    x64_mov_imm(R10, integer_bits((UINT64_C(1) << bits) - 1)); x64_and(RAX, R10);
    if (!integer_unsigned(ty)) {
        x64_mov_imm(R10, INT64_C(1) << (bits - 1)); x64_xor(RAX, R10); x64_sub(RAX, R10);
    }
}
static void gen_integer_range(Type from, Type to, int bad)
{
    int bits = integer_width(to), u = integer_unsigned(to);
    if (integer_unsigned(from)) {
        uint64_t max = u ? (bits == 64 ? UINT64_MAX : (UINT64_C(1) << bits) - 1) :
                          (bits == 64 ? INT64_MAX : (UINT64_C(1) << (bits - 1)) - 1);
        x64_mov_imm(R10, integer_bits(max)); x64_cmp(RAX, R10); x64_jcc(CC_A, bad);
    } else if (u) {
        x64_test(RAX, RAX); x64_jcc(CC_S, bad);
        if (bits < 64) { x64_mov_imm(R10, integer_bits((UINT64_C(1) << bits) - 1)); x64_cmp(RAX, R10); x64_jcc(CC_G, bad); }
    } else if (bits < 64) {
        x64_mov_imm(R10, -(INT64_C(1) << (bits - 1))); x64_cmp(RAX, R10); x64_jcc(CC_L, bad);
        x64_mov_imm(R10, (INT64_C(1) << (bits - 1)) - 1); x64_cmp(RAX, R10); x64_jcc(CC_G, bad);
    }
}
static void gen_integer_checked(Type from, Type to, Token *tok)
{
    if(type_is_wide(from)||type_is_wide(to)){gen_wide(W_CONVERT,to,from,0,tok);return;}
    int bad = new_label("integer.cast.range"), done = new_label("integer.cast.done");
    gen_integer_range(from, to, bad); x64_jmp(done);
    x64_bind(bad); gen_fail(tok, "整数変換の結果が対象型の範囲外です", EX_SOFTWARE);
    x64_bind(done);
}
static void gen_format_integer(Type ty)
{
    if(type_is_wide(ty)){gen_wide(W_FORMAT,ty,ty,0,NULL);return;}
    if (integer_width(ty) < 64) {
        int decimal = new_label("integer.format.decimal");
        x64_mov_imm(R10, 10); x64_cmp(RDI, R10); x64_jcc(CC_Z, decimal);
        x64_mov_imm(R10, integer_bits((UINT64_C(1) << integer_width(ty)) - 1)); x64_and(RAX, R10);
        x64_bind(decimal);
    }
    x64_mov_imm(R11, integer_unsigned(ty)); x64_call(integer_format_label);
}
static void gen_parse_integer(Type ty)
{
    if(type_is_wide(ty)){gen_wide_parse(ty);return;}
    x64_mov_imm(R11, ty); x64_call(parse_int_label);
}
static void gen_complex_unpack(Reg pointer, int re, int im)
{
    int zero = new_label("complex.unpack.zero"), done = new_label("complex.unpack.done");
    x64_test(pointer, pointer); x64_jcc(CC_Z, zero);
    x64_load(R10, pointer, 0); x64_movq_to_xmm(re, R10); x64_load(R10, pointer, 8); x64_movq_to_xmm(im, R10); x64_jmp(done);
    x64_bind(zero); x64_xor(R10, R10); x64_movq_to_xmm(re, R10); x64_movq_to_xmm(im, R10); x64_bind(done);
}
/* rax の値を from 型から to 型に変換する */
static void gen_convert(Type from, Type to)
{
    if (from == to) {
        return;
    }
    if (type_is_complex(to)) {
        if (type_is_complex(from)) {
            if (to == TY_C128) return;
            gen_complex_unpack(RAX, 0, 1); x64_movq_from_xmm(RAX, 0); x64_movq_from_xmm(RDI, 1);
        } else {
            gen_convert(from, complex_component(to)); gen_convert(complex_component(to), TY_DOUBLE); x64_xor(RDI, RDI);
        }
        x64_call(complex_make_labels[to == TY_C128]); return;
    }
    if((type_is_wide(from)||type_is_wide(to)) && !type_is_complex(from) && !type_is_text(from) && !type_is_text(to)){gen_wide(W_CONVERT,to,from,0,NULL);wide_check(NULL);return;}
    if (type_is_text(from) && type_is_text(to)) {
        return;  /* 表現は共通。char の制約検査は gen_cast で行う。 */
    }
    if (type_is_integer(from) && type_is_integer(to)) return;
    if (type_is_integer(from) && (to == TY_DOUBLE || to == TY_FLOAT)) {
        int ordinary = new_label("integer.real.low"), done = new_label("integer.real.done");
        if (integer_unsigned(from) && integer_width(from) == 64) {
            x64_test(RAX, RAX); x64_jcc(CC_NS, ordinary);
            x64_mov(R10, RAX); x64_shl_imm(R10, 63); x64_shr_imm(R10, 63); x64_shr_imm(RAX, 1); x64_or(RAX, R10);
            if (to == TY_DOUBLE) { x64_cvtsi2sd(0, RAX); x64_sse_sd(SSE_ADD, 0, 0); x64_movq_from_xmm(RAX, 0); }
            else { x64_cvtsi2ss(0, RAX); x64_sse_ss(SSE_ADD, 0, 0); x64_movd_from_xmm(RAX, 0); }
            x64_jmp(done);
        }
        x64_bind(ordinary);
        if (to == TY_DOUBLE) { x64_cvtsi2sd(0, RAX); x64_movq_from_xmm(RAX, 0); }
        else { x64_cvtsi2ss(0, RAX); x64_movd_from_xmm(RAX, 0); }
        x64_bind(done); return;
    }
    if (from == TY_FLOAT && to == TY_DOUBLE) {
        x64_movd_to_xmm(0, RAX);
        x64_cvtss2sd(0, 0);
        x64_movq_from_xmm(RAX, 0);
        return;
    }
    if (from == TY_DOUBLE && to == TY_FLOAT) {
        x64_movq_to_xmm(0, RAX);
        x64_cvtsd2ss(0, 0);
        x64_movd_from_xmm(RAX, 0);
        return;
    }
    fatal("内部エラー: %s から %s への変換は未対応です", type_name(from), type_name(to));
}

/* rax の文字列を保持し、r9=文字数、r8=UTF-8不正の状態を得る。 */
static void gen_text_measure_preserving(void)
{
    x64_push(RAX);
    x64_mov_imm(RDI, -1);
    x64_call(string_scan_label);
    x64_mov(R9, RAX);
    x64_pop(RAX);
}

static void gen_char_check(Token *tok, int code)
{
    int utf8_ok = new_label("char.utf8.ok");
    int one = new_label("char.one");

    gen_text_measure_preserving();
    x64_test(R8, R8);
    x64_jcc(CC_Z, utf8_ok);
    gen_fail(tok, "不正な UTF-8 文字列です", code);
    x64_bind(utf8_ok);
    x64_mov_imm(RCX, 1);
    x64_cmp(R9, RCX);
    x64_jcc(CC_Z, one);
    gen_fail(tok, "char には 書記素クラスタをちょうど1つ含む文字列が必要です", code);
    x64_bind(one);
}

#include "collections_codegen.inc"
#include "file_runtime.inc"
#include "complex_codegen.inc"

static void gen_cast(Node *node)
{
    if((type_is_wide(node->lhs->ty)||type_is_wide(node->ty)) && !type_is_complex(node->ty)){gen_expr(node->lhs);gen_wide(W_CONVERT,node->ty,node->lhs->ty,0,node->tok);return;}
    /* 小数リテラルを float にする場合は、コンパイル時に変換する */
    if (node->lhs->kind == ND_FNUM && node->ty == TY_FLOAT) {
        x64_mov_imm(RAX, float_bits((float)node->lhs->fval));
        return;
    }
    gen_expr(node->lhs);
    if (node->ty == TY_CHAR && node->lhs->ty != TY_CHAR) {
        gen_char_check(node->tok, node->lhs->kind == ND_INPUT ? EX_DATAERR : EX_SOFTWARE);
        return;
    }
    if (type_is_integer(node->lhs->ty) && type_is_integer(node->ty)) gen_integer_checked(node->lhs->ty, node->ty, node->tok);
    gen_convert(node->lhs->ty, node->ty);
}

static void gen_text_operation(Node *node)
{
    int valid = new_label("text.utf8.ok");
    int in_range = new_label("text.index.ok");
    int allocated = new_label("text.allocated");

    gen_expr(node->lhs);
    if (node->kind == ND_INDEX) {
        push();
        gen_expr(node->rhs);
        x64_mov(RDI, RAX);
        pop(RAX);
        x64_test(RDI, RDI);
        x64_jcc(CC_NS, in_range);
        gen_fail(node->tok, "文字列の添字が範囲外です", EX_SOFTWARE);
        x64_bind(in_range);
    } else {
        x64_mov_imm(RDI, -1);
    }
    x64_call(string_scan_label);
    x64_test(R8, R8);
    x64_jcc(CC_Z, valid);
    gen_fail(node->tok, "不正な UTF-8 文字列です", EX_SOFTWARE);
    x64_bind(valid);
    if (node->kind == ND_LENGTH) {
        return;
    }
    in_range = new_label("text.index.found");
    x64_test(RDX, RDX);
    x64_jcc(CC_NZ, in_range);
    gen_fail(node->tok, "文字列の添字が範囲外です", EX_SOFTWARE);
    x64_bind(in_range);
    x64_push(RDX);
    x64_push(RCX);
    x64_lea(RDI, RCX, 8);
    x64_call(alloc_label);
    if (target == TARGET_LINUX) {
        x64_mov_imm(RDI, -4095);
        x64_cmp(RAX, RDI);
        x64_jcc(CC_B, allocated);
    } else {
        x64_test(RAX, RAX);
        x64_jcc(CC_NZ, allocated);
    }
    gen_fail(node->tok, "文字列のメモリを確保できません", EX_SOFTWARE);
    x64_bind(allocated);
    x64_pop(RCX);
    x64_pop(RSI);
    x64_store(RAX, 0, RCX);
    x64_push(RAX);
    x64_lea(RDI, RAX, 8);
    x64_rep_movsb();
    x64_pop(RAX);
}

static void gen_neg(Node *node)
{
    gen_expr(node->lhs);
    if(type_is_wide(node->ty)){gen_wide(W_NEG,node->ty,node->ty,0,node->tok);return;}
    if (type_is_complex(node->ty)) {
        gen_complex_unpack(RAX, 0, 1); x64_movq_from_xmm(RAX, 0); x64_movq_from_xmm(RDI, 1);
        x64_btc(RAX, 63); x64_btc(RDI, 63); x64_call(complex_make_labels[node->ty == TY_C128]); return;
    }
    if (type_is_integer(node->ty)) { x64_neg(RAX); return; }
    switch (node->ty) {
    case TY_DOUBLE:
        x64_btc(RAX, 63);       /* 符号ビットを反転 */
        return;
    case TY_FLOAT:
        x64_btc(RAX, 31);
        return;
    default:
        fatal("内部エラー: 単項 - の型が不正です");
    }
}

static SseOp sse_op(NodeKind kind)
{
    switch (kind) {
    case ND_ADD:
        return SSE_ADD;
    case ND_SUB:
        return SSE_SUB;
    case ND_MUL:
        return SSE_MUL;
    default:
        return SSE_DIV;
    }
}

/*
 * 整数の割り算の前に、0 で割っていないか、int の最小値を -1 で割っていないかを確かめる
 * （rax が割られる数、rdi が割る数）。どちらも実行時エラーとして終了コード 70 で止める。
 */
static void gen_div_check(Node *node)
{
    int nonzero;
    int ok;

    nonzero = new_label("div.nonzero");
    ok = new_label("div.ok");
    x64_test(RDI, RDI);
    x64_jcc(CC_NZ, nonzero);
    gen_fail(node->tok, node->kind == ND_MOD ? "0 で割った余りを求めようとしました" : "0 で割ろうとしました", EX_SOFTWARE);
    x64_bind(nonzero);
    if (integer_unsigned(node->ty)) return;
    x64_mov_imm(RCX, -1);
    x64_cmp(RDI, RCX);
    x64_jcc(CC_NZ, ok);
    x64_mov_imm(RCX, -(INT64_C(1) << (integer_width(node->ty) - 2)) * 2);
    x64_cmp(RAX, RCX);
    x64_jcc(CC_NZ, ok);
    if (node->kind == ND_MOD) {
        x64_xor(RAX, RAX);              /* 余りは 0（数学どおり）。0 ÷ 1 にして idiv の例外を避ける */
        x64_mov_imm(RDI, 1);
        x64_jmp(ok);
    } else {
        gen_fail(node->tok, "割り算の結果が int の範囲を超えます", EX_SOFTWARE);
    }
    x64_bind(ok);
}

/* 累乗の定義域を検査する。rax は底、rdi は指数。正常時は両方を保持する。 */
static void gen_pow_check(Node *node)
{
    int ok = new_label("pow.domain.ok");
    int nonzero = new_label("pow.domain.nonzero");

    x64_test(RDI, RDI);
    x64_jcc(CC_NS, ok);
    x64_mov(RCX, RAX);
    if (node->ty == TY_FLOAT) {
        x64_btr(RCX, 31);
    } else if (node->ty == TY_DOUBLE) {
        x64_btr(RCX, 63);
    }
    x64_test(RCX, RCX);
    x64_jcc(CC_NZ, nonzero);
    gen_fail(node->tok, "0 の負の累乗は定義されていません", EX_SOFTWARE);
    x64_bind(nonzero);
    if (type_is_integer(node->ty)) {
        x64_mov_imm(RCX, 1);
        x64_cmp(RAX, RCX);
        x64_jcc(CC_Z, ok);
        if (!integer_unsigned(node->ty)) { x64_mov_imm(RCX, -1); x64_cmp(RAX, RCX); x64_jcc(CC_Z, ok); }
        gen_fail(node->tok, "負の累乗の結果を int で表せません（2.0^(-1) のように底を小数にしてください）", EX_SOFTWARE);
    }
    x64_bind(ok);
}

/* 二項演算：左辺を先に評価して退避し、右辺を評価してから左 rax・右 rdi で計算する */
static void gen_binary(Node *node)
{
    gen_expr(node->lhs);
    push();
    gen_expr(node->rhs);
    x64_mov(RDI, RAX);
    pop(RAX);

    if(type_is_wide(node->ty)){gen_wide(wide_codegen_op(node->kind),node->ty,node->ty,0,node->tok);return;}
    if (type_is_complex(node->ty)) {
        int op = node->kind == ND_ADD ? 0 : node->kind == ND_SUB ? 1 : node->kind == ND_MUL ? 2 : 3;
        collection_context(node->tok); x64_call(complex_op_labels[node->ty == TY_C128][op]); return;
    }
    if (type_is_integer(node->ty)) {
        switch (node->kind) {
        case ND_ADD:
            x64_add(RAX, RDI);
            return;
        case ND_SUB:
            x64_sub(RAX, RDI);
            return;
        case ND_MUL:
            x64_imul(RAX, RDI);
            return;
        case ND_MOD:
            gen_div_check(node);
            if (integer_unsigned(node->ty)) { x64_xor(RDX, RDX); x64_div(RDI); }
            else { x64_cqo(); x64_idiv(RDI); }
            x64_mov(RAX, RDX);  /* 余りは rdx に入る */
            return;
        default:
            gen_div_check(node);
            if (integer_unsigned(node->ty)) { x64_xor(RDX, RDX); x64_div(RDI); }
            else { x64_cqo(); x64_idiv(RDI); }
            return;
        }
    }
    switch (node->ty) {
    case TY_DOUBLE:
        x64_movq_to_xmm(0, RAX);
        x64_movq_to_xmm(1, RDI);
        x64_sse_sd(sse_op(node->kind), 0, 1);
        x64_movq_from_xmm(RAX, 0);
        return;
    case TY_FLOAT:
        x64_movd_to_xmm(0, RAX);
        x64_movd_to_xmm(1, RDI);
        x64_sse_ss(sse_op(node->kind), 0, 1);
        x64_movd_from_xmm(RAX, 0);
        return;
    case TY_STR:
        x64_call(concat_label);
        return;
    default:
        fatal("内部エラー: 二項演算の型が不正です");
    }
}

/*
 * 比較の前に rax の値を from 型から比べる型に変換する。
 * float で比べる場合は、float に丸めてから double にする（float→double は値が変わらない）。
 */
static void gen_convert_for_compare(Type from, Type cmp_ty)
{
    if (cmp_ty == TY_FLOAT) {
        if (from != TY_FLOAT) {
            gen_convert(from, TY_FLOAT);
        }
        gen_convert(TY_FLOAT, TY_DOUBLE);
        return;
    }
    gen_convert(from, cmp_ty);
}

static Cond int_cond(CmpKind kind)
{
    switch (kind) {
    case CMP_EQ:
        return CC_Z;
    case CMP_NE:
        return CC_NZ;
    case CMP_LT:
        return CC_L;
    case CMP_LE:
        return CC_LE;
    case CMP_GT:
        return CC_G;
    default:
        return CC_GE;
    }
}

/* 左辺 rdi と右辺 rax を比べ、結果（0 か 1）を rax に入れる */
static void gen_compare(Node *cmp)
{
    CmpKind kind;
    Type ty;

    kind = (CmpKind)cmp->val;
    ty = cmp->cmp_ty;
    if (type_is_complex(ty)) {
        push(); x64_mov(RAX, RDI); gen_convert(cmp->lhs->ty, ty); push();
        x64_load(RAX, RSP, 8); gen_convert(cmp->rhs->ty, ty); pop(RDI); x64_add_imm(RSP, 8); depth--;
        x64_call(complex_equal_label); if (kind == CMP_NE) x64_xor_imm(RAX, 1); return;
    }
    if(type_is_wide(ty)||type_is_wide(cmp->lhs->ty)||type_is_wide(cmp->rhs->ty)){
        push();x64_mov(RAX,RDI);gen_convert(cmp->lhs->ty,ty);push();x64_load(RAX,RSP,8);gen_convert(cmp->rhs->ty,ty);x64_mov(RDI,RAX);pop(RAX);x64_add_imm(RSP,8);depth--;gen_wide(W_CMP,ty,ty,kind,NULL);return;
    }
    if (type_is_model(ty)) {
        x64_mov(RCX, RAX); x64_mov(RAX, RDI); x64_mov(RDI, RCX);
        x64_call(type_model(ty)->equal_label);
        if (kind == CMP_NE) x64_xor_imm(RAX, 1);
        return;
    }
    if (type_is_array(ty)) {
        x64_mov(RCX, RAX); x64_mov(RAX, RDI); x64_mov(RDI, RCX);
        x64_call(array_equal_label);
        if (kind == CMP_NE) x64_xor_imm(RAX, 1);
        return;
    }

    /* 左右を比べる型にそろえる（左辺は rax に移して変換する） */
    if (cmp->lhs->ty != ty || ty == TY_FLOAT) {
        x64_mov(RCX, RAX);
        x64_mov(RAX, RDI);
        gen_convert_for_compare(cmp->lhs->ty, ty);
        x64_mov(RDI, RAX);
        x64_mov(RAX, RCX);
    }
    if (cmp->rhs->ty != ty || ty == TY_FLOAT) {
        gen_convert_for_compare(cmp->rhs->ty, ty);
    }

    if (type_is_integer(ty)) {
        Cond cc = int_cond(kind);
        if (integer_unsigned(ty)) cc = kind == CMP_LT ? CC_B : kind == CMP_LE ? CC_BE : kind == CMP_GT ? CC_A : kind == CMP_GE ? CC_AE : cc;
        x64_cmp(RDI, RAX); x64_setcc(cc, RAX); x64_movzx8(RAX, RAX); return;
    }
    switch (ty) {
    case TY_VOID:x64_mov_imm(RAX,kind==CMP_EQ);return;
    case TY_BOOL:
    case TY_FILE:
    case TY_ADDR:
        x64_cmp(RDI, RAX);
        x64_setcc(int_cond(kind), RAX);
        x64_movzx8(RAX, RAX);
        return;
    case TY_STR:
        x64_call(str_eq_label);
        if (kind == CMP_NE) {
            x64_xor_imm(RAX, 1);
        }
        return;
    default:
        break;
    }

    /* float・double：NaN を含む比較は == 以外すべて偽、!= は真になるようにする */
    x64_movq_to_xmm(0, RDI);
    x64_movq_to_xmm(1, RAX);
    switch (kind) {
    case CMP_EQ:
        x64_ucomisd(0, 1);
        x64_setcc(CC_Z, RAX);
        x64_setcc(CC_NP, RCX);
        x64_and8(RAX, RCX);
        break;
    case CMP_NE:
        x64_ucomisd(0, 1);
        x64_setcc(CC_NZ, RAX);
        x64_setcc(CC_P, RCX);
        x64_or8(RAX, RCX);
        break;
    case CMP_LT:
        x64_ucomisd(1, 0);      /* 右 > 左 */
        x64_setcc(CC_A, RAX);
        break;
    case CMP_LE:
        x64_ucomisd(1, 0);      /* 右 >= 左 */
        x64_setcc(CC_AE, RAX);
        break;
    case CMP_GT:
        x64_ucomisd(0, 1);
        x64_setcc(CC_A, RAX);
        break;
    default:
        x64_ucomisd(0, 1);
        x64_setcc(CC_AE, RAX);
        break;
    }
    x64_movzx8(RAX, RAX);
}

/*
 * 比較の連鎖：a < b < c は a < b かつ b < c。
 * 各項は1回だけ評価し、途中で偽になったら残りは評価しない。
 */
static void gen_chain(Node *node)
{
    Node *cmp;
    int is_false;
    int end;
    int chained;

    chained = node->lhs->next != NULL;
    is_false = chained ? new_label("cmp.false") : -1;
    end = chained ? new_label("cmp.end") : -1;

    gen_expr(node->lhs->lhs);
    if (type_is_model(node->lhs->lhs->ty)) x64_call(type_model(node->lhs->lhs->ty)->copy_label);
    if (type_is_array(node->lhs->lhs->ty)) {
        x64_load(RDI, RAX, 16); x64_xor(RSI, RSI); x64_call(array_copy_label);
    }
    push();
    for (cmp = node->lhs; cmp != NULL; cmp = cmp->next) {
        gen_expr(cmp->rhs);
        if (cmp->next != NULL && type_is_model(cmp->rhs->ty)) x64_call(type_model(cmp->rhs->ty)->copy_label);
        if (cmp->next != NULL && type_is_array(cmp->rhs->ty)) {
            x64_load(RDI, RAX, 16); x64_xor(RSI, RSI); x64_call(array_copy_label);
        }
        pop(RDI);
        if (cmp->next != NULL) {
            push();             /* 右辺は次の比較の左辺になる */
        }
        gen_compare(cmp);
        if (cmp->next != NULL) {
            x64_test(RAX, RAX);
            x64_jcc(CC_Z, is_false);
        }
    }
    if (!chained) {
        return;
    }
    x64_jmp(end);

    /* 途中で偽になった：退避していた値を捨てて 0 にする */
    x64_bind(is_false);
    x64_pop(RDI);
    x64_mov_imm(RAX, 0);
    x64_bind(end);
}

/* and・or：左辺で結果が決まれば右辺は評価しない */
static void gen_logic(Node *node)
{
    int end;

    end = new_label(node->kind == ND_AND ? "and.end" : "or.end");
    gen_expr(node->lhs);
    x64_test(RAX, RAX);
    x64_jcc(node->kind == ND_AND ? CC_Z : CC_NZ, end);
    gen_expr(node->rhs);
    x64_bind(end);
}

/* rax の値の絶対値（ty の型として） */
static void gen_abs_rax(Type ty)
{
    int done;

    if (type_is_integer(ty)) {
        if (integer_unsigned(ty)) return;
        done = new_label("abs.integer.done"); x64_test(RAX, RAX); x64_jcc(CC_NS, done); x64_neg(RAX); x64_bind(done); return;
    }
    switch (ty) {
    case TY_DOUBLE:
        x64_btr(RAX, 63);       /* 符号ビットを 0 にする */
        return;
    default:
        x64_btr(RAX, 31);
        return;
    }
}

/* rax（double）を int にする。NaN や int の範囲外なら実行時エラーで止める */
static void gen_real_to_integer(Type from, Type to, Token *tok)
{
    int bad = new_label("real.integer.bad"), low = new_label("real.integer.low"), done = new_label("real.integer.done");
    int bits = integer_width(to); double high = ldexp(1.0, bits - !integer_unsigned(to));
    gen_convert(from, TY_DOUBLE); x64_movq_to_xmm(0, RAX);
    x64_ucomisd(0, 0); x64_jcc(CC_P, bad);
    x64_mov_imm(R10, double_bits(integer_unsigned(to) ? 0.0 : -high)); x64_movq_to_xmm(1, R10);
    x64_ucomisd(0, 1); x64_jcc(CC_B, bad);
    x64_mov_imm(R10, double_bits(high)); x64_movq_to_xmm(1, R10);
    x64_ucomisd(0, 1); x64_jcc(CC_AE, bad);
    if (integer_unsigned(to) && bits == 64) {
        x64_mov_imm(R10, double_bits(9223372036854775808.0)); x64_movq_to_xmm(1, R10);
        x64_ucomisd(0, 1); x64_jcc(CC_B, low); x64_sse_sd(SSE_SUB, 0, 1); x64_cvttsd2si(RAX, 0); x64_btc(RAX, 63); x64_jmp(done);
    }
    x64_bind(low); x64_cvttsd2si(RAX, 0); x64_jmp(done);
    x64_bind(bad); gen_fail(tok, "整数変換の結果が対象型の範囲外です", EX_SOFTWARE);
    x64_bind(done);
}
static void gen_builtin(Node *node)
{
    Builtin b;
    Node *x;
    Node *digits;
    Type ty;

    b = (Builtin)node->val;
    x = node->lhs;
    digits = x->next;
    ty = x->ty;
    if((b==BI_HALF||b==BI_QUAD)&&type_is_text(ty)){
        int bad=new_label("wide.text.bad"),done=new_label("wide.text.done");
        gen_expr(x);x64_load(RDX,RAX,0);x64_lea(RSI,RAX,8);x64_add(RDX,RSI);x64_xor(RDI,RDI);gen_wide_parse(node->ty);
        x64_test(RCX,RCX);x64_jcc(CC_NZ,bad);push();x64_call(skip_ws_label);x64_cmp(RSI,RDX);pop(RAX);x64_jcc(CC_Z,done);
        x64_bind(bad);gen_fail(node->tok,"文字列を指定した小数型として読み取れません",EX_DATAERR);x64_bind(done);return;
    }
    if(type_is_wide(ty)||type_is_wide(node->ty)){
        if(b==BI_INT||b==BI_FLOAT||b==BI_DOUBLE||b==BI_HALF||b==BI_QUAD){gen_expr(x);gen_wide(W_CONVERT,node->ty,ty,0,node->tok);return;}
        if(b==BI_ABS||b==BI_SQRT||b==BI_ROUND||b==BI_ROUNDUP||b==BI_ROUNDDOWN){
            gen_expr(x);push();if(digits)gen_expr(digits);else x64_xor(RAX,RAX);x64_mov(RDI,RAX);pop(RAX);
            gen_wide(b==BI_ABS?W_ABS:b==BI_SQRT?W_SQRT:W_ROUND,ty,ty,b-BI_ROUND,node->tok);return;
        }
    }

    switch (b) {
    case BI_CHAR:
        gen_expr(x);
        if (ty != TY_CHAR) {
            gen_char_check(node->tok, EX_SOFTWARE);
        }
        return;
    case BI_STR:
        gen_expr(x);if(ty==TY_VOID)x64_lea_data(RAX,prog_add_str(prog,"()",2));
        return;
    case BI_SQRT:
        gen_expr(x);
        if (type_is_complex(ty)) { x64_call(complex_sqrt_labels[ty == TY_C128]); return; }
        if (ty == TY_FLOAT) {
            x64_movd_to_xmm(0, RAX);
            x64_sqrtss(0, 0);
            x64_movd_from_xmm(RAX, 0);
        } else {
            x64_movq_to_xmm(0, RAX);
            x64_sqrtsd(0, 0);
            x64_movq_from_xmm(RAX, 0);
        }
        return;
    case BI_ABS:
        gen_expr(x);
        if (type_is_complex(ty)) { x64_call(complex_abs_label); gen_convert(TY_DOUBLE, node->ty); return; }
        gen_abs_rax(ty);
        return;
    case BI_ROUND:
    case BI_ROUNDUP:
    case BI_ROUNDDOWN:
        gen_expr(x);
        if (type_is_integer(ty) && digits == NULL) {
            return;             /* int はすでに整数 */
        }
        if (!type_is_integer(ty)) {
            gen_convert(ty, TY_DOUBLE);
        }
        if (digits != NULL) {
            push();
            gen_expr(digits);
            x64_mov(RDI, RAX);
            pop(RAX);
        } else {
            x64_xor(RDI, RDI);
        }
        if (type_is_integer(ty)) {
            int ok = new_label("round_integer.ok");
            x64_call(round_integer_labels[integer_unsigned(ty)][b - BI_ROUND]);
            x64_test(RDX, RDX);
            x64_jcc(CC_Z, ok);
            gen_fail(node->tok, "整数の丸め結果が int の範囲を超えます", EX_SOFTWARE);
            x64_bind(ok);
            gen_integer_checked(ty, ty, node->tok);
            return;
        }
        x64_call(round_labels[b - BI_ROUND]);
        if (ty == TY_FLOAT) {
            gen_convert(TY_DOUBLE, TY_FLOAT);
        }
        return;
    case BI_INT:
        gen_expr(x);
        if (type_is_integer(ty)) gen_integer_checked(ty, node->ty, node->tok);
        else gen_real_to_integer(ty, node->ty, node->tok);
        return;
    case BI_FLOAT:
        gen_expr(x);
        gen_convert(ty, TY_FLOAT);
        return;
    default:
        gen_expr(x);
        gen_convert(ty, TY_DOUBLE);
        return;
    }
}

/* ---------- 入力（00.22） ---------- */

/*
 * 実行時の状態の領域（r14 が指す。エントリ処理で確保する）
 * 生成するコードは r14 を使わず、Windows の API もシステムコールも r14 を壊さない。
 */
#define ST_STDIN      0     /* Windows：標準入力のハンドル */
#define ST_CONSOLE    8     /* Windows：標準入力がコンソールなら 1 */
#define ST_PEND_N     16    /* Windows：まだ返していない UTF-8 のバイトの数 */
#define ST_PEND       24    /* Windows：まだ返していない UTF-8 のバイト（下位から順に返す） */
#define ST_COUNT      32    /* Windows：読んだ数の受け取り先 */
#define ST_CHAR       40    /* Windows：読んだ文字の受け取り先 */
#define ST_MODE       48    /* Windows：標準入力のコンソールの元のモード */
#define ST_EDIT       56    /* 標準入力と標準出力がどちらも端末なら 1（行の編集を使う） */
#define ST_OUTB       64    /* 1 バイトを書くときの置き場 */
#define ST_HIGH       72    /* Windows：まだ組み合わせていない上位サロゲート */
#define ST_REC        80    /* Windows：INPUT_RECORD（20 バイト）の受け取り先 */
#define ST_TERMIOS    112   /* Linux：端末の元の設定（termios、36 バイト） */
#define ST_RAW        152   /* Linux：行の編集のための端末の設定 */
#define ST_TMP        192   /* 作業用 */
#define ST_SUFFIX     200   /* 次に読む行の右の枠（00.23.1） */
#define ST_SUFFIX_LEN 208   /* 右の枠のバイト数（0 なら枠なし） */
/* Windows では Linux 用の領域を、カーソルの位置の計算に使う */
#define ST_CSBI       ST_TERMIOS        /* CONSOLE_SCREEN_BUFFER_INFO（22 バイト） */
#define ST_WSTARTX    ST_RAW            /* 行の編集を始めたときのカーソルの列 */
#define ST_WSTARTY    (ST_RAW + 8)      /* 同じく行 */
#define ST_WWIDTH     (ST_RAW + 16)     /* 画面の幅 */
#define ST_HOUT       (ST_RAW + 24)     /* 標準出力のハンドル */
#define ST_WOLDEND    (ST_RAW + 32)     /* 前回描いた行の終わりの位置（行 × 幅 + 列） */
#define STATE_SIZE    304

#define STD_INPUT_HANDLE (-10)


static int getc_label;
static int readline_label;
static int parse_bool_label;
static int match_sep_label;
static int find_sep_label;
static int make_str_label;
static int input_error_label;
static int dec_to_double_label;

static void input_labels(void)
{
    getc_label = x64_new_label(".L.qy_getc");
    readline_label = x64_new_label(".L.qy_readline");
    skip_ws_label = x64_new_label(".L.qy_skip_ws");
    word_follows_label = x64_new_label(".L.qy_word_follows");
    parse_int_label = x64_new_label(".L.qy_parse_int");
    parse_real_label = x64_new_label(".L.qy_parse_real");
    parse_bool_label = x64_new_label(".L.qy_parse_bool");
    match_sep_label = x64_new_label(".L.qy_match_sep");
    find_sep_label = x64_new_label(".L.qy_find_sep");
    make_str_label = x64_new_label(".L.qy_make_str");
    input_error_label = x64_new_label(".L.qy_input_error");
    dec_to_double_label = x64_new_label(".L.qy_dec_to_double");
    lineedit_labels();
}

/* エントリ処理で、実行時の状態の領域を確保して r14 に置く */
static void gen_input_state_init(void)
{
    x64_mov_imm(RDI, STATE_SIZE);
    x64_call(alloc_label);
    x64_mov(R14, RAX);
    x64_store_imm(R14, OUTPUT_SINK, 0);
    if (target == TARGET_LINUX) {
        gen_lineedit_init();
        return;
    }
    x64_store_imm(R14, ST_CONSOLE, 0);
    x64_store_imm(R14, ST_PEND_N, 0);
    x64_store_imm(R14, ST_PEND, 0);
    x64_store_imm(R14, ST_MODE, 0);
    x64_mov_imm(RCX, STD_INPUT_HANDLE);
    x64_call_import(IMP_GET_STD_HANDLE);
    x64_store(R14, ST_STDIN, RAX);
    /* GetConsoleMode が成功すれば、標準入力はコンソール */
    x64_mov(RCX, RAX);
    x64_lea(RDX, R14, ST_MODE);
    x64_call_import(IMP_GET_CONSOLE_MODE);
    x64_test(RAX, RAX);
    x64_setcc(CC_NZ, RAX);
    x64_movzx8(RAX, RAX);
    x64_store(R14, ST_CONSOLE, RAX);
    gen_lineedit_init();
}

/* rax と即値を比べる（rcx を使う） */
static void cmp_rax_imm(long long imm)
{
    x64_mov_imm(RCX, imm);
    x64_cmp(RAX, RCX);
}

/*
 * Windows のコンソールから UTF-16 の文字を1つ読み、rax に入れる（読めなければ eof へ）
 * ReadConsoleW(ハンドル, &文字, 1, &読んだ数, NULL)
 */
static void gen_read_console_unit(int eof)
{
    x64_store_imm(R14, ST_CHAR, 0);
    x64_load(RCX, R14, ST_STDIN);
    x64_lea(RDX, R14, ST_CHAR);
    x64_mov_imm(R8, 1);
    x64_lea(R9, R14, ST_COUNT);
    x64_store_imm(RSP, 32, 0);
    x64_call_import(IMP_READ_CONSOLE_W);
    x64_test(RAX, RAX);
    x64_jcc(CC_Z, eof);
    x64_load(RCX, R14, ST_COUNT);
    x64_test(RCX, RCX);
    x64_jcc(CC_Z, eof);
    x64_load(RAX, R14, ST_CHAR);
    x64_and_imm(RAX, 0xFFFF);
}

/* rcx = 0x80 | ((rax >> shift) and 0x3F)（UTF-8 の続きのバイト） */
static void utf8_cont(int shift)
{
    x64_mov(RCX, RAX);
    if (shift > 0) {
        x64_shr_imm(RCX, shift);
    }
    x64_and_imm(RCX, 0x3F);
    x64_mov_imm(RDX, 0x80);
    x64_or(RCX, RDX);
}

/*
 * 標準入力から1バイト読み、rax に入れる（0〜255。入力の終わりは -1、読み込みの失敗は -2）
 * Windows のコンソールでは UTF-16 で読んだ文字を UTF-8 に直し、2バイト目以降を状態の領域に残す。
 */
static void gen_getc(void)
{
    int eof;
    int err;
    int out;
    int nopend;
    int console;
    int encode;
    int two;
    int three;
    int bad;
    int surrogate;

    eof = x64_new_label(".L.qy_getc.eof");
    out = x64_new_label(".L.qy_getc.out");
    x64_bind(getc_label);
    x64_push(RBP);
    x64_mov(RBP, RSP);

    if (target == TARGET_LINUX) {
        err = x64_new_label(".L.qy_getc.err");
        x64_sub_imm(RSP, 16);
        x64_store_imm(RBP, -8, 0);
        x64_xor(RDI, RDI);              /* 標準入力 */
        x64_lea(RSI, RBP, -8);
        x64_mov_imm(RDX, 1);
        x64_xor(RAX, RAX);              /* read */
        x64_syscall();
        x64_test(RAX, RAX);
        x64_jcc(CC_S, err);
        x64_jcc(CC_Z, eof);
        x64_load(RAX, RBP, -8);
        x64_jmp(out);
        x64_bind(err);
        x64_mov_imm(RAX, -2);
        x64_jmp(out);
        x64_bind(eof);
        x64_mov_imm(RAX, -1);
        x64_bind(out);
        x64_mov(RSP, RBP);
        x64_pop(RBP);
        x64_ret();
        return;
    }

    nopend = x64_new_label(".L.qy_getc.nopend");
    console = x64_new_label(".L.qy_getc.console");
    encode = x64_new_label(".L.qy_getc.encode");
    two = x64_new_label(".L.qy_getc.two");
    three = x64_new_label(".L.qy_getc.three");
    bad = x64_new_label(".L.qy_getc.bad");
    surrogate = x64_new_label(".L.qy_getc.surrogate");
    x64_sub_imm(RSP, 16);               /* [rbp-8] 上位サロゲート */
    x64_and_imm(RSP, -16);
    x64_sub_imm(RSP, 48);

    /* 前の文字の残りのバイトがあれば、それを返す */
    x64_load(RAX, R14, ST_PEND_N);
    x64_test(RAX, RAX);
    x64_jcc(CC_Z, nopend);
    x64_load(RAX, R14, ST_PEND);
    x64_mov(RCX, RAX);
    x64_shr_imm(RCX, 8);
    x64_store(R14, ST_PEND, RCX);
    x64_and_imm(RAX, 0xFF);
    x64_load(RCX, R14, ST_PEND_N);
    x64_sub_imm(RCX, 1);
    x64_store(R14, ST_PEND_N, RCX);
    x64_jmp(out);

    x64_bind(nopend);
    x64_load(RAX, R14, ST_CONSOLE);
    x64_test(RAX, RAX);
    x64_jcc(CC_NZ, console);

    /* ファイルやパイプ：ReadFile(ハンドル, &文字, 1, &読んだ数, NULL) */
    x64_store_imm(R14, ST_CHAR, 0);
    x64_load(RCX, R14, ST_STDIN);
    x64_lea(RDX, R14, ST_CHAR);
    x64_mov_imm(R8, 1);
    x64_lea(R9, R14, ST_COUNT);
    x64_store_imm(RSP, 32, 0);
    x64_call_import(IMP_READ_FILE);
    x64_test(RAX, RAX);
    x64_jcc(CC_Z, eof);
    x64_load(RCX, R14, ST_COUNT);
    x64_test(RCX, RCX);
    x64_jcc(CC_Z, eof);
    x64_load(RAX, R14, ST_CHAR);
    x64_and_imm(RAX, 0xFF);
    x64_jmp(out);

    /* コンソール：UTF-16 で読み、サロゲートペアを組み合わせてから UTF-8 に直す */
    x64_bind(console);
    gen_read_console_unit(eof);
    cmp_rax_imm(0x1A);                  /* Ctrl+Z は入力の終わり */
    x64_jcc(CC_Z, eof);
    x64_mov(RCX, RAX);
    x64_sub_imm(RCX, 0xD800);
    x64_mov_imm(RDX, 0x800);
    x64_cmp(RCX, RDX);
    x64_jcc(CC_AE, encode);             /* サロゲートでない */
    x64_mov_imm(RDX, 0x400);
    x64_cmp(RCX, RDX);
    x64_jcc(CC_AE, bad);                /* 下位サロゲートだけが来た */
    x64_store(RBP, -8, RCX);
    gen_read_console_unit(eof);
    x64_mov(RDX, RAX);
    x64_sub_imm(RDX, 0xDC00);
    x64_mov_imm(RCX, 0x400);
    x64_cmp(RDX, RCX);
    x64_jcc(CC_AE, bad);
    x64_bind(surrogate);
    x64_load(RAX, RBP, -8);
    x64_shl_imm(RAX, 10);
    x64_add(RAX, RDX);
    x64_add_imm(RAX, 0x10000);
    x64_jmp(encode);
    x64_bind(bad);
    x64_mov_imm(RAX, 0xFFFD);           /* 置き換え文字 */

    x64_bind(encode);
    cmp_rax_imm(0x80);
    x64_jcc(CC_B, out);                 /* 1バイト */
    cmp_rax_imm(0x800);
    x64_jcc(CC_B, two);
    cmp_rax_imm(0x10000);
    x64_jcc(CC_B, three);
    /* 4バイト */
    utf8_cont(0);
    x64_shl_imm(RCX, 16);
    x64_mov(R8, RCX);
    utf8_cont(6);
    x64_shl_imm(RCX, 8);
    x64_or(R8, RCX);
    utf8_cont(12);
    x64_or(R8, RCX);
    x64_store(R14, ST_PEND, R8);
    x64_store_imm(R14, ST_PEND_N, 3);
    x64_shr_imm(RAX, 18);
    x64_mov_imm(RCX, 0xF0);
    x64_or(RAX, RCX);
    x64_jmp(out);
    x64_bind(three);
    utf8_cont(0);
    x64_shl_imm(RCX, 8);
    x64_mov(R8, RCX);
    utf8_cont(6);
    x64_or(R8, RCX);
    x64_store(R14, ST_PEND, R8);
    x64_store_imm(R14, ST_PEND_N, 2);
    x64_shr_imm(RAX, 12);
    x64_mov_imm(RCX, 0xE0);
    x64_or(RAX, RCX);
    x64_jmp(out);
    x64_bind(two);
    utf8_cont(0);
    x64_store(R14, ST_PEND, RCX);
    x64_store_imm(R14, ST_PEND_N, 1);
    x64_shr_imm(RAX, 6);
    x64_mov_imm(RCX, 0xC0);
    x64_or(RAX, RCX);
    x64_jmp(out);

    x64_bind(eof);
    x64_mov_imm(RAX, -1);
    x64_bind(out);
    x64_mov(RSP, RBP);
    x64_pop(RBP);
    x64_ret();
}

/*
 * 標準入力から1行読み、str を rax に返す（行末の改行と CR は除く）
 * 入力の終わりで1文字も読めなければ 0、読み込みに失敗したら 1 を返す。
 * [rbp-8] 文字列  [rbp-16] 長さ  [rbp-24] 容量  [rbp-32] 1文字でも読んだら 1  [rbp-40] 読んだバイト
 */
static void gen_readline(void)
{
    int loop;
    int room;
    int at_eof;
    int readerr;
    int finish;
    int store_len;
    int out;

    loop = x64_new_label(".L.qy_readline.loop");
    room = x64_new_label(".L.qy_readline.room");
    at_eof = x64_new_label(".L.qy_readline.eof");
    readerr = x64_new_label(".L.qy_readline.err");
    finish = x64_new_label(".L.qy_readline.finish");
    store_len = x64_new_label(".L.qy_readline.store_len");
    out = x64_new_label(".L.qy_readline.out");

    x64_bind(readline_label);
    {
        int plain = x64_new_label(".L.qy_readline.plain");
        x64_load(RAX, R14, ST_EDIT);
        x64_test(RAX, RAX);
        x64_jcc(CC_Z, plain);
        x64_jmp(edit_line_label);       /* 端末なら行の編集で読む */
        x64_bind(plain);
        x64_store_imm(R14, ST_SUFFIX_LEN, 0);   /* 端末でなければ右の枠は表示しない */
    }
    x64_push(RBP);
    x64_mov(RBP, RSP);
    x64_sub_imm(RSP, 48);
    x64_mov_imm(RDI, 64 + 8);
    x64_call(alloc_label);
    x64_store(RBP, -8, RAX);
    x64_store_imm(RBP, -16, 0);
    x64_store_imm(RBP, -24, 64);
    x64_store_imm(RBP, -32, 0);

    x64_bind(loop);
    x64_call(getc_label);
    cmp_rax_imm(-2);
    x64_jcc(CC_Z, readerr);
    cmp_rax_imm(-1);
    x64_jcc(CC_Z, at_eof);
    x64_store_imm(RBP, -32, 1);
    cmp_rax_imm('\n');
    x64_jcc(CC_Z, finish);
    x64_store(RBP, -40, RAX);
    /* 容量が足りなければ2倍の領域に移す */
    x64_load(RCX, RBP, -16);
    x64_load(RDX, RBP, -24);
    x64_cmp(RCX, RDX);
    x64_jcc(CC_B, room);
    x64_shl_imm(RDX, 1);
    x64_store(RBP, -24, RDX);
    x64_mov(RDI, RDX);
    x64_add_imm(RDI, 8);
    x64_call(alloc_label);
    x64_load(RSI, RBP, -8);
    x64_add_imm(RSI, 8);
    x64_lea(RDI, RAX, 8);
    x64_load(RCX, RBP, -16);
    x64_rep_movsb();
    x64_store(RBP, -8, RAX);
    x64_bind(room);
    x64_load(RDI, RBP, -8);
    x64_load(RCX, RBP, -16);
    x64_add(RDI, RCX);
    x64_load(RAX, RBP, -40);
    x64_store8(RDI, 8, RAX);
    x64_add_imm(RCX, 1);
    x64_store(RBP, -16, RCX);
    x64_jmp(loop);

    x64_bind(at_eof);
    x64_load(RAX, RBP, -32);
    x64_test(RAX, RAX);
    x64_jcc(CC_NZ, finish);
    x64_xor(RAX, RAX);
    x64_jmp(out);
    x64_bind(readerr);
    x64_mov_imm(RAX, 1);
    x64_jmp(out);

    x64_bind(finish);
    x64_load(RCX, RBP, -16);
    x64_test(RCX, RCX);
    x64_jcc(CC_Z, store_len);
    x64_load(RDI, RBP, -8);
    x64_add(RDI, RCX);
    x64_load8(RAX, RDI, 7);             /* 最後のバイト */
    cmp_rax_imm('\r');                 /* rcx を使うので、長さは後で読み直す */
    x64_jcc(CC_NZ, store_len);
    x64_load(RCX, RBP, -16);
    x64_sub_imm(RCX, 1);
    x64_store(RBP, -16, RCX);
    x64_bind(store_len);
    x64_load(RCX, RBP, -16);
    x64_load(RAX, RBP, -8);
    x64_store(RAX, 0, RCX);
    x64_bind(out);
    x64_mov(RSP, RBP);
    x64_pop(RBP);
    x64_ret();
}

/* rsi から rdx の手前までの空白（スペースとタブ）を読み飛ばす（rax・rcx を使う） */
static void gen_skip_ws(void)
{
    int loop;
    int adv;
    int done;

    loop = x64_new_label(".L.qy_skip_ws.loop");
    adv = x64_new_label(".L.qy_skip_ws.adv");
    done = x64_new_label(".L.qy_skip_ws.done");
    x64_bind(skip_ws_label);
    x64_bind(loop);
    x64_cmp(RSI, RDX);
    x64_jcc(CC_AE, done);
    x64_load8(RAX, RSI, 0);
    cmp_rax_imm(' ');
    x64_jcc(CC_Z, adv);
    cmp_rax_imm('\t');
    x64_jcc(CC_Z, adv);
    x64_jmp(done);
    x64_bind(adv);
    x64_add_imm(RSI, 1);
    x64_jmp(loop);
    x64_bind(done);
    x64_ret();
}

/*
 * rsi の文字が、数や語の続きになる文字（英数字・_・.・ASCII 以外）なら rax = 1（rax・rcx を使う）
 * 数の直後にこれらが続くと、その型として読めないものとする（例：12abc、3.5 を int として）
 */
static void gen_word_follows(void)
{
    int yes;
    int no;

    yes = x64_new_label(".L.qy_word_follows.yes");
    no = x64_new_label(".L.qy_word_follows.no");
    x64_bind(word_follows_label);
    x64_cmp(RSI, RDX);
    x64_jcc(CC_AE, no);
    x64_load8(RAX, RSI, 0);
    cmp_rax_imm('_');
    x64_jcc(CC_Z, yes);
    cmp_rax_imm('.');
    x64_jcc(CC_Z, yes);
    cmp_rax_imm(0x80);
    x64_jcc(CC_AE, yes);
    x64_sub_imm(RAX, '0');
    cmp_rax_imm(10);
    x64_jcc(CC_B, yes);
    x64_load8(RAX, RSI, 0);
    x64_mov_imm(RCX, 0x20);
    x64_or(RAX, RCX);                   /* 英字を小文字にそろえる */
    x64_sub_imm(RAX, 'a');
    cmp_rax_imm(26);
    x64_jcc(CC_B, yes);
    x64_bind(no);
    x64_xor(RAX, RAX);
    x64_ret();
    x64_bind(yes);
    x64_mov_imm(RAX, 1);
    x64_ret();
}

/*
 * 整数を読む：rsi から rdx の手前まで → rax 値、rsi 読み終えた位置、rcx 状態
 * 状態は 0 成功、1 整数として読めない、2 int の範囲外。失敗したら rsi は読み始めの位置に戻す。
 * 形は [+-] 数字。rdx は変えない。
 */
#include "integer_bases_codegen.inc"

/*
 * 10進数の M × 10^E を、最も近い double にする（1回だけ丸める）
 *   rdi = M の下位 64 ビット、r8 = M の上位 64 ビット（M は 38 桁まで）、rsi = E、
 *   rdx = 捨てた桁に 0 以外があれば 1 → rax（正の double のビット列）
 * E < 0 なら、V = M（128 ビット）を、最上位のビットが立つまで左へずらしてから 10^9 ずつ割っていく。
 * 商は常に 97 ビット以上の精度を保ち、割り切れなかったら「捨てたものがある」にする。
 * E = 0 ならそのまま、0 < E <= 19 で M が 64 ビットに収まるなら M × 10^E を 128 ビットで正確に求め、
 * それ以外の E > 0 は 10^9 ずつ掛けて、積の上位 128 ビットを保つ。
 * 最後に V を 64 ビットに縮め、53 ビットへ偶数への丸めで丸め、2 の累乗を掛ける。
 * 数字は 38 桁まで正確に扱い、それより後ろの桁は「0 以外があったか」だけを使う。
 * [rbp-8] M の下位  [rbp-16] E  [rbp-24] 捨てたものがあれば 1  [rbp-40] 2 の何乗を掛けるか
 * [rbp-48] 残りの |E|  [rbp-56] M の上位
 */
static void gen_dec_to_double(void)
{
    static const unsigned long long pow10[20] = {
        1ULL, 10ULL, 100ULL, 1000ULL, 10000ULL, 100000ULL, 1000000ULL, 10000000ULL, 100000000ULL,
        1000000000ULL, 10000000000ULL, 100000000000ULL, 1000000000000ULL, 10000000000000ULL,
        100000000000000ULL, 1000000000000000ULL, 10000000000000000ULL, 100000000000000000ULL,
        1000000000000000000ULL, 10000000000000000000ULL
    };
    unsigned char table[160];
    size_t table_off;
    int zero;
    int nonzero;
    int negative;
    int positive;
    int reduce;
    int convert;
    int noround;
    int up;
    int down;
    int out;
    int tloop;
    int tchunk;
    int tdiv;
    int thi;
    int tfull;
    int tlow;
    int tstick;
    int mloop_start;
    int i;
    int k;

    for (i = 0; i < 20; i++) {
        for (k = 0; k < 8; k++) {
            table[i * 8 + k] = (unsigned char)(pow10[i] >> (8 * k));
        }
    }
    table_off = prog_add_data(prog, (const char *)table, sizeof(table));
    zero = x64_new_label(".L.qy_dec.zero");
    nonzero = x64_new_label(".L.qy_dec.nonzero");
    negative = x64_new_label(".L.qy_dec.negative");
    positive = x64_new_label(".L.qy_dec.positive");
    reduce = x64_new_label(".L.qy_dec.reduce");
    convert = x64_new_label(".L.qy_dec.convert");
    noround = x64_new_label(".L.qy_dec.noround");
    up = x64_new_label(".L.qy_dec.up");
    down = x64_new_label(".L.qy_dec.down");
    out = x64_new_label(".L.qy_dec.out");
    tloop = x64_new_label(".L.qy_dec.tloop");
    tchunk = x64_new_label(".L.qy_dec.tchunk");
    tdiv = x64_new_label(".L.qy_dec.tdiv");
    thi = x64_new_label(".L.qy_dec.thi");
    tfull = x64_new_label(".L.qy_dec.tfull");
    tlow = x64_new_label(".L.qy_dec.tlow");
    tstick = x64_new_label(".L.qy_dec.tstick");
    mloop_start = x64_new_label(".L.qy_dec.mloop_start");

    x64_bind(dec_to_double_label);
    x64_push(RBP);
    x64_mov(RBP, RSP);
    x64_sub_imm(RSP, 64);
    x64_store(RBP, -8, RDI);
    x64_store(RBP, -16, RSI);
    x64_store(RBP, -24, RDX);
    x64_store(RBP, -56, R8);
    x64_store_imm(RBP, -40, 0);
    x64_mov(RAX, RDI);
    x64_or(RAX, R8);
    x64_jcc(CC_Z, zero);
    x64_bind(nonzero);
    x64_mov(R9, RDI);                   /* r8:r9 = V */
    x64_mov(RAX, RSI);
    x64_test(RAX, RAX);
    x64_jcc(CC_S, negative);
    x64_jcc(CC_Z, reduce);              /* E = 0 */

    /* 0 < E <= 19 で M が 64 ビットに収まるなら、V = M × 10^E */
    x64_bind(positive);
    x64_test(R8, R8);
    x64_jcc(CC_NZ, mloop_start);
    cmp_rax_imm(19);
    x64_jcc(CC_G, mloop_start);
    x64_lea_data(R10, table_off);
    x64_shl_imm(RAX, 3);
    x64_add(R10, RAX);
    x64_load(RCX, R10, 0);
    x64_mov(RAX, R9);
    x64_mul(RCX);
    x64_mov(R8, RDX);
    x64_mov(R9, RAX);
    x64_jmp(reduce);

    /*
     * それ以外の E > 0：V を 10^9 ずつ掛けていく。積は 192 ビットになるので、上位 128 ビットに収まるよう
     * 右へずらし、はみ出したビットは「捨てたものがある」にする。
     */
    x64_bind(mloop_start);
    x64_store(RBP, -48, RAX);
    {
        int mloop = x64_new_label(".L.qy_dec.mloop");
        int mchunk = x64_new_label(".L.qy_dec.mchunk");
        int mfit = x64_new_label(".L.qy_dec.mfit");
        int mnost = x64_new_label(".L.qy_dec.mnost");
        x64_bind(mloop);
        x64_load(RAX, RBP, -48);
        x64_test(RAX, RAX);
        x64_jcc(CC_Z, reduce);
        x64_mov(R10, RAX);
        x64_mov_imm(RCX, 9);
        x64_cmp(R10, RCX);
        x64_jcc(CC_LE, mchunk);
        x64_mov(R10, RCX);
        x64_bind(mchunk);
        x64_sub(RAX, R10);
        x64_store(RBP, -48, RAX);
        x64_lea_data(R11, table_off);
        x64_shl_imm(R10, 3);
        x64_add(R11, R10);
        x64_load(R11, R11, 0);          /* D = 10^c */
        /* (r8:r9) × D = 上位×D × 2^64 + 下位×D → 192 ビット（r10:r8:r9） */
        x64_mov(RAX, R9);
        x64_mul(R11);
        x64_mov(R9, RAX);               /* 0 番目の 64 ビット */
        x64_mov(RCX, RDX);              /* 下位×D の上半分 */
        x64_mov(RAX, R8);
        x64_mul(R11);
        x64_add(RAX, RCX);
        x64_setcc(CC_B, RCX);
        x64_movzx8(RCX, RCX);
        x64_add(RDX, RCX);
        x64_mov(R8, RAX);               /* 1 番目 */
        x64_mov(R10, RDX);              /* 2 番目 */
        x64_test(R10, R10);
        x64_jcc(CC_Z, mloop);
        /* n = 2 番目のビットの長さ（10^9 < 2^30 なので n は 30 以下）だけ右へずらす */
        x64_bsr(RCX, R10);
        x64_add_imm(RCX, 1);
        x64_load(RAX, RBP, -40);
        x64_add(RAX, RCX);
        x64_store(RBP, -40, RAX);
        x64_mov(RAX, R9);
        x64_mov(RDX, RCX);              /* n を退避 */
        x64_push(RDX);
        x64_mov_imm(RDX, 64);
        x64_sub(RDX, RCX);
        x64_mov(RCX, RDX);              /* 64 - n */
        x64_shl_cl(RAX);                /* 0 番目からはみ出すビット */
        x64_test(RAX, RAX);
        x64_jcc(CC_Z, mnost);
        x64_store_imm(RBP, -24, 1);
        x64_bind(mnost);
        x64_mov(RAX, R8);
        x64_shl_cl(RAX);                /* 1 番目 << (64 - n) */
        x64_mov(RDX, R10);
        x64_shl_cl(RDX);                /* 2 番目 << (64 - n) */
        x64_pop(RCX);                   /* n */
        x64_shr_cl(R9);
        x64_or(R9, RAX);                /* 新しい下位 */
        x64_shr_cl(R8);
        x64_or(R8, RDX);                /* 新しい上位 */
        x64_bind(mfit);
        x64_jmp(mloop);
    }

    /* E < 0：10^9 ずつ割る */
    x64_bind(negative);
    x64_neg(RAX);
    x64_store(RBP, -48, RAX);
    x64_bind(tloop);
    x64_load(RAX, RBP, -48);
    x64_test(RAX, RAX);
    x64_jcc(CC_Z, reduce);
    x64_mov(R10, RAX);
    x64_mov_imm(RCX, 9);
    x64_cmp(R10, RCX);
    x64_jcc(CC_LE, tchunk);
    x64_mov(R10, RCX);
    x64_bind(tchunk);
    x64_sub(RAX, R10);
    x64_store(RBP, -48, RAX);
    x64_lea_data(R11, table_off);
    x64_shl_imm(R10, 3);
    x64_add(R11, R10);
    x64_load(R11, R11, 0);              /* D = 10^c（c は 9 以下） */
    /* 上位が 0 なら 64 ビット左へ */
    x64_test(R8, R8);
    x64_jcc(CC_NZ, thi);
    x64_mov(R8, R9);
    x64_xor(R9, R9);
    x64_load(RAX, RBP, -40);
    x64_sub_imm(RAX, 64);
    x64_store(RBP, -40, RAX);
    x64_bind(thi);
    /* 最上位のビットが立つまで左へずらす */
    x64_bsr(RCX, R8);
    x64_mov_imm(RAX, 63);
    x64_sub(RAX, RCX);
    x64_jcc(CC_Z, tdiv);
    x64_load(RDX, RBP, -40);
    x64_sub(RDX, RAX);
    x64_store(RBP, -40, RDX);
    x64_mov(RCX, RAX);
    x64_shl_cl(R8);
    x64_mov(RDX, R9);
    x64_shl_cl(R9);
    x64_mov(R10, RCX);
    x64_mov_imm(RCX, 64);
    x64_sub(RCX, R10);
    x64_shr_cl(RDX);
    x64_or(R8, RDX);
    x64_bind(tdiv);
    /* 128 ビット ÷ 64 ビット を2回の div で求める */
    x64_xor(RDX, RDX);
    x64_mov(RAX, R8);
    x64_div(R11);
    x64_mov(R8, RAX);
    x64_mov(RAX, R9);
    x64_div(R11);
    x64_mov(R9, RAX);
    x64_test(RDX, RDX);
    x64_jcc(CC_Z, tloop);
    x64_store_imm(RBP, -24, 1);
    x64_jmp(tloop);

    /* V（r8:r9）を 64 ビットに縮める（はみ出したビットは「捨てたものがある」に） */
    x64_bind(reduce);
    x64_test(R8, R8);
    x64_jcc(CC_Z, tlow);
    x64_bsr(RCX, R8);
    x64_add_imm(RCX, 1);                /* n = 上位のビットの長さ */
    x64_load(RAX, RBP, -40);
    x64_add(RAX, RCX);
    x64_store(RBP, -40, RAX);
    x64_mov_imm(RAX, 64);
    x64_cmp(RCX, RAX);
    x64_jcc(CC_Z, tfull);
    x64_mov(R10, RCX);
    x64_mov(RAX, R9);
    x64_shr_cl(RAX);                    /* 下位 >> n */
    x64_mov_imm(RCX, 64);
    x64_sub(RCX, R10);
    x64_mov(RDX, R9);
    x64_shl_cl(RDX);                    /* 下位からはみ出すビット */
    x64_test(RDX, RDX);
    x64_jcc(CC_Z, tstick);
    x64_store_imm(RBP, -24, 1);
    x64_bind(tstick);
    x64_shl_cl(R8);                     /* 上位 << (64 - n) */
    x64_or(RAX, R8);
    x64_jmp(convert);
    x64_bind(tfull);
    x64_test(R9, R9);
    {
        int ns = x64_new_label(".L.qy_dec.tns");
        x64_jcc(CC_Z, ns);
        x64_store_imm(RBP, -24, 1);
        x64_bind(ns);
    }
    x64_mov(RAX, R8);
    x64_jmp(convert);
    x64_bind(tlow);
    x64_mov(RAX, R9);

    /* rax（0 でない 64 ビットの整数）を 53 ビットに丸め、2 の [rbp-40] 乗を掛ける */
    x64_bind(convert);
    x64_bsr(RCX, RAX);
    x64_add_imm(RCX, 1);                /* L */
    x64_mov_imm(R8, 53);
    x64_cmp(RCX, R8);
    x64_jcc(CC_LE, noround);
    x64_sub(RCX, R8);                   /* r = L - 53 */
    x64_load(R8, RBP, -40);
    x64_add(R8, RCX);
    x64_store(RBP, -40, R8);
    x64_mov(R9, RAX);
    x64_shr_cl(RAX);                    /* 仮数 */
    x64_mov(R8, RCX);
    x64_mov_imm(RCX, 64);
    x64_sub(RCX, R8);
    x64_shl_cl(R9);                     /* 捨てるビットを上に詰めたもの */
    x64_mov_imm(R10, (long long)0x8000000000000000ULL);
    x64_cmp(R9, R10);
    x64_jcc(CC_B, down);
    x64_jcc(CC_A, up);
    /* ちょうど半分：捨てたものがあれば切り上げ、なければ偶数へ */
    x64_load(R11, RBP, -24);
    x64_test(R11, R11);
    x64_jcc(CC_NZ, up);
    x64_mov(R11, RAX);
    x64_and_imm(R11, 1);
    x64_jcc(CC_Z, down);
    x64_bind(up);
    x64_add_imm(RAX, 1);
    x64_mov_imm(R10, 1LL << 53);
    x64_cmp(RAX, R10);
    x64_jcc(CC_B, down);
    x64_shr_imm(RAX, 1);
    x64_load(R8, RBP, -40);
    x64_add_imm(R8, 1);
    x64_store(RBP, -40, R8);
    x64_bind(down);
    x64_bind(noround);
    x64_cvtsi2sd(0, RAX);               /* 53 ビット以下なので正確 */
    /* 2 の累乗を掛ける。とても小さい数は 2 回に分け、それでも小さすぎれば 0 にする */
    {
        int normal = x64_new_label(".L.qy_dec.normal");
        int underflow = x64_new_label(".L.qy_dec.underflow");
        x64_load(RAX, RBP, -40);
        cmp_rax_imm(-1022);
        x64_jcc(CC_GE, normal);
        x64_mov_imm(RAX, (long long)(1023 - 600) << 52);   /* 2^-600 */
        x64_movq_to_xmm(1, RAX);
        x64_sse_sd(SSE_MUL, 0, 1);
        x64_load(RAX, RBP, -40);
        x64_add_imm(RAX, 600);
        x64_store(RBP, -40, RAX);
        cmp_rax_imm(-1022);
        x64_jcc(CC_L, underflow);
        x64_bind(normal);
        x64_load(RAX, RBP, -40);
        x64_add_imm(RAX, 1023);
        x64_shl_imm(RAX, 52);
        x64_movq_to_xmm(1, RAX);
        x64_sse_sd(SSE_MUL, 0, 1);      /* 2 の累乗を掛けるだけなので正確（小さすぎる数を除く） */
        x64_movq_from_xmm(RAX, 0);
        x64_jmp(out);
        x64_bind(underflow);
        x64_xor(RAX, RAX);
        x64_jmp(out);
    }

    x64_bind(zero);
    x64_xor(RAX, RAX);
    x64_bind(out);
    x64_mov(RSP, RBP);
    x64_pop(RBP);
    x64_ret();
}

/* M（上位 [rbp-64]、下位 r9）が 10^37 未満なら room へ（rcx を使う） */
static void gen_m128_room(int room)
{
    /* 10^37 = 542101086242752217 × 2^64 + 68739955140067328。上位が 542101086242752217 未満なら確実に余裕がある */
    x64_load(RCX, RBP, -64);
    x64_push(RAX);
    x64_mov_imm(RAX, 542101086242752217LL);
    x64_cmp(RCX, RAX);
    x64_pop(RAX);
    x64_jcc(CC_B, room);
}

/* M = M × 10 + rax（M の上位 [rbp-64]、下位 r9。rax は 0〜9。rax・rcx を使い、rdx は [rbp-16] から戻す） */
static void gen_m128_mul10_add(void)
{
    x64_push(RAX);                      /* 加える数字 */
    x64_mov(RAX, R9);
    x64_mov_imm(RCX, 10);
    x64_mul(RCX);                       /* rdx:rax = 下位 × 10 */
    x64_pop(RCX);
    x64_add(RAX, RCX);
    x64_setcc(CC_B, RCX);               /* 繰り上がり */
    x64_movzx8(RCX, RCX);
    x64_add(RDX, RCX);
    x64_mov(R9, RAX);
    x64_load(RAX, RBP, -64);
    x64_mov_imm(RCX, 10);
    x64_imul(RAX, RCX);
    x64_add(RAX, RDX);
    x64_store(RBP, -64, RAX);
    x64_load(RDX, RBP, -16);            /* 行の終わり */
}

/*
 * 小数を読む：rsi から rdx の手前まで → rax double のビット列、rsi 読み終えた位置、rcx 状態
 * 状態は 0 成功、1 小数として読めない、2 範囲外（無限大になる）。rdx は変えない。
 * 形は [+-] 数字 [. 数字]。数字を 38 桁まで 128 ビットの整数 M に集め、M × 10^E を dec_to_double で正確に丸める。
 * [rbp-8] 読み始め  [rbp-16] rdx  [rbp-24] 負なら 1  [rbp-48] 読み終えた位置  [rbp-56] 捨てた桁に 0 以外があれば 1
 * [rbp-64] M の上位 64 ビット（下位は r9）
 */
static void gen_parse_real(void)
{
    int chkplus;
    int digits;
    int iloop;
    int idone;
    int keep;
    int inext;
    int floop;
    int fdone;
    int fnext;
    int pos;
    int fail1;
    int fail2;
    int out;
    int drop_i;
    int drop_f;

    chkplus = x64_new_label(".L.qy_parse_real.plus");
    digits = x64_new_label(".L.qy_parse_real.digits");
    iloop = x64_new_label(".L.qy_parse_real.iloop");
    idone = x64_new_label(".L.qy_parse_real.idone");
    keep = x64_new_label(".L.qy_parse_real.keep");
    inext = x64_new_label(".L.qy_parse_real.inext");
    floop = x64_new_label(".L.qy_parse_real.floop");
    fdone = x64_new_label(".L.qy_parse_real.fdone");
    fnext = x64_new_label(".L.qy_parse_real.fnext");
    pos = x64_new_label(".L.qy_parse_real.pos");
    fail1 = x64_new_label(".L.qy_parse_real.fail1");
    fail2 = x64_new_label(".L.qy_parse_real.fail2");
    out = x64_new_label(".L.qy_parse_real.out");
    drop_i = x64_new_label(".L.qy_parse_real.drop_i");
    drop_f = x64_new_label(".L.qy_parse_real.drop_f");

    x64_bind(parse_real_label);
    x64_push(RBP);
    x64_mov(RBP, RSP);
    x64_sub_imm(RSP, 64);
    x64_store(RBP, -8, RSI);
    x64_store(RBP, -16, RDX);
    x64_store_imm(RBP, -56, 0);
    x64_store_imm(RBP, -64, 0);
    x64_xor(R8, R8);
    x64_cmp(RSI, RDX);
    x64_jcc(CC_AE, fail1);
    x64_load8(RAX, RSI, 0);
    cmp_rax_imm('-');
    x64_jcc(CC_NZ, chkplus);
    x64_mov_imm(R8, 1);
    x64_add_imm(RSI, 1);
    x64_jmp(digits);
    x64_bind(chkplus);
    cmp_rax_imm('+');
    x64_jcc(CC_NZ, digits);
    x64_add_imm(RSI, 1);

    x64_bind(digits);
    x64_xor(R9, R9);                    /* M */
    x64_xor(R10, R10);                  /* E */
    x64_xor(R11, R11);                  /* 整数部の数字の数 */
    x64_bind(iloop);
    x64_cmp(RSI, RDX);
    x64_jcc(CC_AE, idone);
    x64_load8(RAX, RSI, 0);
    x64_sub_imm(RAX, '0');
    cmp_rax_imm(10);
    x64_jcc(CC_AE, idone);
    gen_m128_room(keep);               /* M が 10^37 未満なら加える（M は 38 桁まで） */
    x64_add_imm(R10, 1);                /* 捨てた桁の分だけ E を増やす */
    x64_test(RAX, RAX);
    x64_jcc(CC_Z, inext);
    x64_bind(drop_i);
    x64_store_imm(RBP, -56, 1);
    x64_jmp(inext);
    x64_bind(keep);
    gen_m128_mul10_add();
    x64_bind(inext);
    x64_add_imm(R11, 1);
    x64_add_imm(RSI, 1);
    x64_jmp(iloop);
    x64_bind(idone);
    x64_test(R11, R11);
    x64_jcc(CC_Z, fail1);

    /* 小数点の後に数字があれば小数部を読む */
    x64_mov(RCX, RSI);
    x64_add_imm(RCX, 1);
    x64_cmp(RCX, RDX);
    x64_jcc(CC_AE, fdone);
    x64_load8(RAX, RSI, 0);
    cmp_rax_imm('.');
    x64_jcc(CC_NZ, fdone);
    x64_load8(RAX, RSI, 1);
    x64_sub_imm(RAX, '0');
    cmp_rax_imm(10);
    x64_jcc(CC_AE, fdone);
    x64_add_imm(RSI, 1);
    x64_bind(floop);
    x64_cmp(RSI, RDX);
    x64_jcc(CC_AE, fdone);
    x64_load8(RAX, RSI, 0);
    x64_sub_imm(RAX, '0');
    cmp_rax_imm(10);
    x64_jcc(CC_AE, fdone);
    gen_m128_room(drop_f);             /* drop_f は「加える」側（名前は飛び先の都合） */
    x64_test(RAX, RAX);                 /* これ以上の桁は捨てる */
    x64_jcc(CC_Z, fnext);
    x64_store_imm(RBP, -56, 1);
    x64_jmp(fnext);
    x64_bind(drop_f);
    gen_m128_mul10_add();
    x64_sub_imm(R10, 1);
    x64_bind(fnext);
    x64_add_imm(RSI, 1);
    x64_jmp(floop);
    x64_bind(fdone);

    x64_call(word_follows_label);
    x64_test(RAX, RAX);
    x64_jcc(CC_NZ, fail1);
    x64_store(RBP, -24, R8);
    x64_store(RBP, -48, RSI);
    x64_mov(RDI, R9);
    x64_load(R8, RBP, -64);
    x64_mov(RSI, R10);
    x64_load(RDX, RBP, -56);
    x64_call(dec_to_double_label);

    /* 無限大になったら範囲外 */
    x64_mov(RCX, RAX);
    x64_btr(RCX, 63);
    x64_mov_imm(RDX, 0x7FF0000000000000LL);
    x64_cmp(RCX, RDX);
    x64_jcc(CC_AE, fail2);
    x64_load(RCX, RBP, -24);
    x64_test(RCX, RCX);
    x64_jcc(CC_Z, pos);
    x64_btc(RAX, 63);
    x64_bind(pos);
    x64_load(RSI, RBP, -48);
    x64_xor(RCX, RCX);
    x64_jmp(out);
    x64_bind(fail1);
    x64_mov_imm(RCX, 1);
    x64_load(RSI, RBP, -8);
    x64_jmp(out);
    x64_bind(fail2);
    x64_mov_imm(RCX, 2);
    x64_load(RSI, RBP, -8);
    x64_bind(out);
    x64_load(RDX, RBP, -16);
    x64_mov(RSP, RBP);
    x64_pop(RBP);
    x64_ret();
}

/* rsi から word の文字が続くか（rax・rcx・r8 を使う。一致しなければ no へ） */
static void match_word(const char *word, int no)
{
    int i;
    int n;

    n = (int)strlen(word);
    x64_mov(R8, RDX);
    x64_sub(R8, RSI);
    x64_mov_imm(RCX, n);
    x64_cmp(R8, RCX);
    x64_jcc(CC_L, no);
    for (i = 0; i < n; i++) {
        x64_load8(RAX, RSI, i);
        cmp_rax_imm((unsigned char)word[i]);
        x64_jcc(CC_NZ, no);
    }
}

/*
 * 真偽値を読む：rsi から rdx の手前まで → rax 0 か 1、rsi 読み終えた位置、rcx 状態（0 成功、1 読めない）
 * true と false だけを受け付ける。
 */
static void gen_parse_bool(void)
{
    int try_false;
    int fail;

    try_false = x64_new_label(".L.qy_parse_bool.false");
    fail = x64_new_label(".L.qy_parse_bool.fail");

    x64_bind(parse_bool_label);
    x64_push(RSI);                      /* 読み始めの位置 */
    match_word("true", try_false);
    x64_add_imm(RSI, 4);
    x64_call(word_follows_label);
    x64_test(RAX, RAX);
    x64_jcc(CC_NZ, fail);
    x64_pop(RCX);
    x64_mov_imm(RAX, 1);
    x64_xor(RCX, RCX);
    x64_ret();
    x64_bind(try_false);
    x64_pop(RSI);
    x64_push(RSI);
    match_word("false", fail);
    x64_add_imm(RSI, 5);
    x64_call(word_follows_label);
    x64_test(RAX, RAX);
    x64_jcc(CC_NZ, fail);
    x64_pop(RCX);
    x64_xor(RAX, RAX);
    x64_xor(RCX, RCX);
    x64_ret();
    x64_bind(fail);
    x64_pop(RSI);
    x64_xor(RAX, RAX);
    x64_mov_imm(RCX, 1);
    x64_ret();
}

/*
 * 区切りの文字を照合する：rsi から rdx の手前まで、区切り rdi（rcx バイト）、r8 = 空白だけの区切りなら 1
 * → rax 一致なら 1、rsi 区切りの後の位置（一致しなければ rsi は変えない）
 * 区切りの中の空白は、入力の 0 個以上の空白と一致する。空白だけの区切りは 1 個以上の空白が必要。
 */
static void gen_match_sep(void)
{
    int loop;
    int lit;
    int wsloop;
    int wsadv;
    int wsdone;
    int endsep;
    int ok;
    int fail;

    loop = x64_new_label(".L.qy_match_sep.loop");
    lit = x64_new_label(".L.qy_match_sep.lit");
    wsloop = x64_new_label(".L.qy_match_sep.wsloop");
    wsadv = x64_new_label(".L.qy_match_sep.wsadv");
    wsdone = x64_new_label(".L.qy_match_sep.wsdone");
    endsep = x64_new_label(".L.qy_match_sep.end");
    ok = x64_new_label(".L.qy_match_sep.ok");
    fail = x64_new_label(".L.qy_match_sep.fail");

    x64_bind(match_sep_label);
    x64_push(RSI);                      /* 照合を始めた位置 */
    x64_xor(R9, R9);                    /* 一致した空白の数 */
    x64_bind(loop);
    x64_test(RCX, RCX);
    x64_jcc(CC_Z, endsep);
    x64_load8(RAX, RDI, 0);
    x64_mov_imm(R10, ' ');
    x64_cmp(RAX, R10);
    x64_jcc(CC_NZ, lit);
    x64_bind(wsloop);
    x64_cmp(RSI, RDX);
    x64_jcc(CC_AE, wsdone);
    x64_load8(R11, RSI, 0);
    x64_mov_imm(R10, ' ');
    x64_cmp(R11, R10);
    x64_jcc(CC_Z, wsadv);
    x64_mov_imm(R10, '\t');
    x64_cmp(R11, R10);
    x64_jcc(CC_Z, wsadv);
    x64_jmp(wsdone);
    x64_bind(wsadv);
    x64_add_imm(RSI, 1);
    x64_add_imm(R9, 1);
    x64_jmp(wsloop);
    x64_bind(wsdone);
    x64_add_imm(RDI, 1);
    x64_sub_imm(RCX, 1);
    x64_jmp(loop);
    x64_bind(lit);
    x64_cmp(RSI, RDX);
    x64_jcc(CC_AE, fail);
    x64_load8(R11, RSI, 0);
    x64_cmp(R11, RAX);
    x64_jcc(CC_NZ, fail);
    x64_add_imm(RSI, 1);
    x64_add_imm(RDI, 1);
    x64_sub_imm(RCX, 1);
    x64_jmp(loop);
    x64_bind(endsep);
    x64_test(R8, R8);
    x64_jcc(CC_Z, ok);
    x64_test(R9, R9);
    x64_jcc(CC_Z, fail);
    x64_bind(ok);
    x64_pop(RAX);                       /* 始めた位置は捨てる */
    x64_mov_imm(RAX, 1);
    x64_ret();
    x64_bind(fail);
    x64_pop(RSI);
    x64_xor(RAX, RAX);
    x64_ret();
}

/*
 * 区切りの文字を探す：rsi から rdx の手前まで、区切り rdi（rcx バイト）、r8 = 空白だけの区切りなら 1
 * → rax 区切りが始まる位置（見つからなければ 0）、rsi 区切りの後の位置
 * [rbp-8] 探す位置  [rbp-16] rdx  [rbp-24] rdi  [rbp-32] rcx  [rbp-40] r8
 */
static void gen_find_sep(void)
{
    int loop;
    int next;
    int notfound;
    int out;

    loop = x64_new_label(".L.qy_find_sep.loop");
    next = x64_new_label(".L.qy_find_sep.next");
    notfound = x64_new_label(".L.qy_find_sep.notfound");
    out = x64_new_label(".L.qy_find_sep.out");

    x64_bind(find_sep_label);
    x64_push(RBP);
    x64_mov(RBP, RSP);
    x64_sub_imm(RSP, 48);
    x64_store(RBP, -8, RSI);
    x64_store(RBP, -16, RDX);
    x64_store(RBP, -24, RDI);
    x64_store(RBP, -32, RCX);
    x64_store(RBP, -40, R8);
    x64_bind(loop);
    x64_load(RSI, RBP, -8);
    x64_load(RDX, RBP, -16);
    x64_cmp(RSI, RDX);
    x64_jcc(CC_AE, notfound);
    x64_load(RDI, RBP, -24);
    x64_load(RCX, RBP, -32);
    x64_load(R8, RBP, -40);
    x64_call(match_sep_label);
    x64_test(RAX, RAX);
    x64_jcc(CC_Z, next);
    x64_load(RAX, RBP, -8);
    x64_jmp(out);
    x64_bind(next);
    x64_load(RAX, RBP, -8);
    x64_add_imm(RAX, 1);
    x64_store(RBP, -8, RAX);
    x64_jmp(loop);
    x64_bind(notfound);
    x64_xor(RAX, RAX);
    x64_bind(out);
    x64_load(RDX, RBP, -16);
    x64_mov(RSP, RBP);
    x64_pop(RBP);
    x64_ret();
}

/* rsi が指す文字が空白なら zero に（rax・rcx を使う） */
static void jump_if_ws(Reg ptr, int disp, int target_label)
{
    x64_load8(RAX, ptr, disp);
    cmp_rax_imm(' ');
    x64_jcc(CC_Z, target_label);
    cmp_rax_imm('\t');
    x64_jcc(CC_Z, target_label);
}

/*
 * rsi から rdx の手前までを新しい str にして rax に返す（rdi = 1 なら前後の空白を除く）
 * [rbp-8] 始め  [rbp-16] 長さ  [rbp-24] 文字列
 */
static void gen_make_str(void)
{
    int notrim;
    int lloop;
    int ladv;
    int rloop;
    int radv;

    notrim = x64_new_label(".L.qy_make_str.notrim");
    lloop = x64_new_label(".L.qy_make_str.lloop");
    ladv = x64_new_label(".L.qy_make_str.ladv");
    rloop = x64_new_label(".L.qy_make_str.rloop");
    radv = x64_new_label(".L.qy_make_str.radv");

    x64_bind(make_str_label);
    x64_push(RBP);
    x64_mov(RBP, RSP);
    x64_sub_imm(RSP, 32);
    x64_test(RDI, RDI);
    x64_jcc(CC_Z, notrim);
    x64_bind(lloop);
    x64_cmp(RSI, RDX);
    x64_jcc(CC_AE, notrim);
    jump_if_ws(RSI, 0, ladv);
    x64_jmp(rloop);
    x64_bind(ladv);
    x64_add_imm(RSI, 1);
    x64_jmp(lloop);
    x64_bind(rloop);
    x64_cmp(RDX, RSI);
    x64_jcc(CC_BE, notrim);
    jump_if_ws(RDX, -1, radv);
    x64_jmp(notrim);
    x64_bind(radv);
    x64_sub_imm(RDX, 1);
    x64_jmp(rloop);
    x64_bind(notrim);
    x64_store(RBP, -8, RSI);
    x64_mov(RAX, RDX);
    x64_sub(RAX, RSI);
    x64_store(RBP, -16, RAX);
    x64_mov(RDI, RAX);
    x64_add_imm(RDI, 8);
    x64_call(alloc_label);
    x64_load(RCX, RBP, -16);
    x64_store(RAX, 0, RCX);
    x64_store(RBP, -24, RAX);
    x64_lea(RDI, RAX, 8);
    x64_load(RSI, RBP, -8);
    x64_rep_movsb();
    x64_load(RAX, RBP, -24);
    x64_mov(RSP, RBP);
    x64_pop(RBP);
    x64_ret();
}

/*
 * 入力の誤りを表示して、終了コード 65 で終える
 *   rdi・r8：見出し（ファイル名・行・列と、ソースの該当行）  rsi・rdx：理由
 *   rcx：入力の中で誤りのあるバイトの位置  r9：読んだ行（str）
 * 表示の例
 *   QYNO 実行時エラー: main.qy:8:2: 入力を読み取れませんでした
 *   	8 | 	input("id, nameの順で入力> {user_id}, {user_name}")
 *   	入力 | 54321 Mary
 *   	       ^ ここに「,」が必要です
 * 誤りの位置までの表示幅は、ASCII を 1、3 バイト以上の UTF-8 の文字を 2、それ以外を 1 として数える。
 */
static void gen_input_error(void)
{
    size_t lbl_input;
    size_t lbl_caret_prefix;
    size_t lbl_space;
    size_t lbl_caret;
    size_t lbl_newline;
    int wloop;
    int wcont;
    int wtwo;
    int wdone;
    int sloop;
    int sdone;
    static const char input_label_text[] = "\t入力 | ";
    static const char caret_prefix[] = "\n\t       ";
    static const char caret[] = "^ ";

    lbl_input = prog_add_data(prog, input_label_text, sizeof(input_label_text) - 1);
    lbl_caret_prefix = prog_add_data(prog, caret_prefix, sizeof(caret_prefix) - 1);
    lbl_space = prog_add_data(prog, " ", 1);
    lbl_caret = prog_add_data(prog, caret, sizeof(caret) - 1);
    lbl_newline = prog_add_data(prog, "\n", 1);
    wloop = x64_new_label(".L.qy_input_error.wloop");
    wcont = x64_new_label(".L.qy_input_error.wcont");
    wtwo = x64_new_label(".L.qy_input_error.wtwo");
    wdone = x64_new_label(".L.qy_input_error.wdone");
    sloop = x64_new_label(".L.qy_input_error.sloop");
    sdone = x64_new_label(".L.qy_input_error.sdone");

    x64_bind(input_error_label);
    x64_push(RBP);
    x64_mov(RBP, RSP);
    x64_sub_imm(RSP, 64);
    x64_store(RBP, -8, RDI);
    x64_store(RBP, -16, R8);
    x64_store(RBP, -24, RSI);
    x64_store(RBP, -32, RDX);
    x64_store(RBP, -40, RCX);
    x64_store(RBP, -48, R9);

    x64_load(RSI, RBP, -8);
    x64_load(RDX, RBP, -16);
    x64_call(write_err_label);
    x64_lea_data(RSI, lbl_input);
    x64_mov_imm(RDX, (long long)sizeof(input_label_text) - 1);
    x64_call(write_err_label);
    x64_load(RAX, RBP, -48);
    x64_load(RDX, RAX, 0);
    x64_lea(RSI, RAX, 8);
    x64_call(write_err_label);
    x64_lea_data(RSI, lbl_caret_prefix);
    x64_mov_imm(RDX, (long long)sizeof(caret_prefix) - 1);
    x64_call(write_err_label);

    /* 誤りの位置までの表示幅 */
    x64_load(RSI, RBP, -48);
    x64_add_imm(RSI, 8);
    x64_load(RDX, RBP, -40);
    x64_add(RDX, RSI);                  /* 誤りの位置 */
    x64_xor(RAX, RAX);
    x64_bind(wloop);
    x64_cmp(RSI, RDX);
    x64_jcc(CC_AE, wdone);
    x64_load8(R9, RSI, 0);
    x64_mov(R10, R9);
    x64_and_imm(R10, 0xC0);
    x64_mov_imm(R11, 0x80);
    x64_cmp(R10, R11);
    x64_jcc(CC_Z, wcont);               /* 続きのバイトは数えない */
    x64_mov_imm(R11, 0xE0);
    x64_cmp(R9, R11);
    x64_jcc(CC_AE, wtwo);
    x64_add_imm(RAX, 1);
    x64_jmp(wcont);
    x64_bind(wtwo);
    x64_add_imm(RAX, 2);
    x64_bind(wcont);
    x64_add_imm(RSI, 1);
    x64_jmp(wloop);
    x64_bind(wdone);
    x64_store(RBP, -56, RAX);

    x64_bind(sloop);
    x64_load(RAX, RBP, -56);
    x64_test(RAX, RAX);
    x64_jcc(CC_Z, sdone);
    x64_sub_imm(RAX, 1);
    x64_store(RBP, -56, RAX);
    x64_lea_data(RSI, lbl_space);
    x64_mov_imm(RDX, 1);
    x64_call(write_err_label);
    x64_jmp(sloop);
    x64_bind(sdone);

    x64_lea_data(RSI, lbl_caret);
    x64_mov_imm(RDX, (long long)sizeof(caret) - 1);
    x64_call(write_err_label);
    x64_load(RSI, RBP, -24);
    x64_load(RDX, RBP, -32);
    x64_call(write_err_label);
    x64_lea_data(RSI, lbl_newline);
    x64_mov_imm(RDX, 1);
    x64_call(write_err_label);
    x64_mov_imm(RAX, EX_DATAERR);
    x64_jmp(exit_label);
}

/* ---------- 行の編集（00.23.0） ---------- */

/*
 * 標準入力と標準出力がどちらも端末のとき、input の1行は OS の行の編集を止めて読み、
 * QYNO の実行時ルーチンが自分で表示を描き直す。カーソルの移動は「\b」で1文字分左へ、
 * 右へは文字を書き直して行う（エスケープシーケンスを使わないので、古いコンソールでも動く）。
 * 行の編集を使っている間だけ端末の設定を切り替え、1行を読み終えたら必ず元に戻す。
 */

/* キーの番号（0 以上は文字のコードポイント） */
#define KEY_EOF     (-1)    /* 入力の終わり */
#define KEY_ERR     (-2)    /* 読み込みの失敗 */
#define KEY_ENTER   (-10)
#define KEY_BS      (-11)   /* Backspace */
#define KEY_DEL     (-12)   /* Delete */
#define KEY_LEFT    (-13)
#define KEY_RIGHT   (-14)
#define KEY_HOME    (-15)
#define KEY_END     (-16)
#define KEY_INTR    (-17)   /* Ctrl+C */
#define KEY_IGNORE  (-18)
#define KEY_EOFKEY  (-19)   /* Ctrl+D（Linux）・Ctrl+Z（Windows） */

#define EXIT_INTERRUPT 130  /* Ctrl+C で止めたときの終了コード（128 + SIGINT） */

/* Linux の ioctl */
#define SYS_IOCTL   16
#define TCGETS      0x5401
#define TCSETS      0x5402

/* Windows のコンソールのモード */
#define ENABLE_PROCESSED_INPUT 0x1
#define ENABLE_LINE_INPUT      0x2
#define ENABLE_ECHO_INPUT      0x4
#define STD_OUTPUT_HANDLE_IN   (-11)

static int put_rep_label;
static int utf8_decode_label;
static int utf8_encode_label;
static int cp_width_label;
static int str_width_label;
static int key_label;
static int raw_on_label;
static int raw_off_label;
static int edit_line_label;
static int win_get_pos_label;
static int win_set_pos_label;
static int win_redraw_label;

static void lineedit_labels(void)
{
    put_rep_label = x64_new_label(".L.qy_put_rep");
    utf8_decode_label = x64_new_label(".L.qy_utf8_decode");
    utf8_encode_label = x64_new_label(".L.qy_utf8_encode");
    cp_width_label = x64_new_label(".L.qy_cp_width");
    str_width_label = x64_new_label(".L.qy_str_width");
    key_label = x64_new_label(".L.qy_key");
    raw_on_label = x64_new_label(".L.qy_raw_on");
    raw_off_label = x64_new_label(".L.qy_raw_off");
    edit_line_label = x64_new_label(".L.qy_edit_line");
    win_get_pos_label = x64_new_label(".L.qy_win_get_pos");
    win_set_pos_label = x64_new_label(".L.qy_win_set_pos");
    win_redraw_label = x64_new_label(".L.qy_win_redraw");
}

/*
 * Windows：カーソルの位置を「行 × 画面の幅 + 列」の1つの数で読み書きする
 *   win_get_pos → rax（GetConsoleScreenBufferInfo。画面の幅も [r14+ST_WWIDTH] に入れる）
 *   win_set_pos：rdi の位置へカーソルを置く（SetConsoleCursorPosition）
 * CONSOLE_SCREEN_BUFFER_INFO の dwSize.X が +0、dwCursorPosition の X が +4、Y が +6（どれも 16 ビット）。
 */
static void gen_win_cursor(void)
{
    int ok;

    if (target == TARGET_LINUX) {
        return;
    }
    ok = x64_new_label(".L.qy_win_get_pos.ok");
    x64_bind(win_get_pos_label);
    x64_push(RBP);
    x64_mov(RBP, RSP);
    x64_and_imm(RSP, -16);
    x64_sub_imm(RSP, 32);
    x64_load(RCX, R14, ST_HOUT);
    x64_lea(RDX, R14, ST_CSBI);
    x64_call_import(IMP_GET_CONSOLE_SCREEN_BUFFER_INFO);
    x64_load(RAX, R14, ST_CSBI);
    x64_and_imm(RAX, 0xFFFF);
    x64_test(RAX, RAX);
    x64_jcc(CC_NZ, ok);
    x64_mov_imm(RAX, 80);               /* 幅が取れなければ 80 とみなす */
    x64_bind(ok);
    x64_store(R14, ST_WWIDTH, RAX);
    x64_load(RCX, R14, ST_CSBI + 6);
    x64_and_imm(RCX, 0xFFFF);           /* 行 */
    x64_imul(RAX, RCX);
    x64_load(RCX, R14, ST_CSBI + 4);
    x64_and_imm(RCX, 0xFFFF);           /* 列 */
    x64_add(RAX, RCX);
    x64_mov(RSP, RBP);
    x64_pop(RBP);
    x64_ret();

    x64_bind(win_set_pos_label);
    x64_push(RBP);
    x64_mov(RBP, RSP);
    x64_and_imm(RSP, -16);
    x64_sub_imm(RSP, 32);
    x64_mov(RAX, RDI);
    x64_xor(RDX, RDX);
    x64_load(RCX, R14, ST_WWIDTH);
    x64_div(RCX);                       /* rax = 行、rdx = 列 */
    x64_shl_imm(RAX, 16);
    x64_or(RDX, RAX);                   /* COORD = 列 | (行 << 16) */
    x64_load(RCX, R14, ST_HOUT);
    x64_call_import(IMP_SET_CONSOLE_CURSOR_POSITION);
    x64_mov(RSP, RBP);
    x64_pop(RBP);
    x64_ret();

    /*
     * win_redraw：rdi = 文字列のデータ、rsi = カーソル（バイト）、rdx = 長さ
     * 行の編集を始めた位置へ戻り、カーソルより前を書いて位置を問い合わせ、残りを書き、
     * 前回より短くなった分は空白で消して、問い合わせた位置へカーソルを戻す。
     * 文字の表示幅をこちらで数えないので、コンソールが文字を何マスで扱っても位置がずれない。
     * [rbp-8] データ  [rbp-16] カーソル  [rbp-24] 長さ  [rbp-32] カーソルの位置  [rbp-40] 行の終わりの位置
     */
    {
        int p1 = x64_new_label(".L.qy_win_redraw.p1");
        int p2 = x64_new_label(".L.qy_win_redraw.p2");
        int nospace = x64_new_label(".L.qy_win_redraw.nospace");
        x64_bind(win_redraw_label);
        x64_push(RBP);
        x64_mov(RBP, RSP);
        x64_sub_imm(RSP, 64);
        x64_store(RBP, -8, RDI);
        x64_store(RBP, -16, RSI);
        x64_store(RBP, -24, RDX);
        x64_store(RBP, -48, R8);        /* 右の枠 */
        x64_store(RBP, -56, R9);
        x64_load(RDI, R14, ST_WSTARTX);
        x64_call(win_set_pos_label);
        x64_load(RSI, RBP, -8);
        x64_load(RDX, RBP, -16);
        x64_test(RDX, RDX);
        x64_jcc(CC_Z, p1);
        x64_call(write_label);
        x64_bind(p1);
        x64_call(win_get_pos_label);
        x64_store(RBP, -32, RAX);
        x64_load(RSI, RBP, -8);
        x64_load(RAX, RBP, -16);
        x64_add(RSI, RAX);
        x64_load(RDX, RBP, -24);
        x64_sub(RDX, RAX);
        x64_jcc(CC_Z, p2);
        x64_call(write_label);
        x64_bind(p2);
        {
            int nosuf = x64_new_label(".L.qy_win_redraw.nosuf");
            x64_load(RDX, RBP, -56);
            x64_test(RDX, RDX);
            x64_jcc(CC_Z, nosuf);
            x64_load(RSI, RBP, -48);
            x64_call(write_label);      /* 右の枠 */
            x64_bind(nosuf);
        }
        x64_call(win_get_pos_label);
        x64_store(RBP, -40, RAX);
        x64_load(RSI, R14, ST_WOLDEND);
        x64_sub(RSI, RAX);
        x64_jcc(CC_LE, nospace);
        x64_mov_imm(RDI, ' ');
        x64_call(put_rep_label);        /* 前回より短くなった分を消す */
        x64_bind(nospace);
        x64_load(RAX, RBP, -40);
        x64_store(R14, ST_WOLDEND, RAX);
        x64_load(RDI, RBP, -32);
        x64_call(win_set_pos_label);
        x64_mov(RSP, RBP);
        x64_pop(RBP);
        x64_ret();
    }
}

/* エントリ処理の続き：標準入力と標準出力がどちらも端末なら [r14+ST_EDIT] = 1 */
static void gen_lineedit_init(void)
{
    int no;

    no = x64_new_label(".L.qy_edit_init.no");
    x64_store_imm(R14, ST_EDIT, 0);
    x64_store_imm(R14, ST_HIGH, 0);
    x64_store_imm(R14, ST_SUFFIX_LEN, 0);
    if (target == TARGET_LINUX) {
        /* ioctl(0, TCGETS, 元の設定) と ioctl(1, TCGETS, 作業用) がどちらも成功すれば端末 */
        x64_xor(RDI, RDI);
        x64_mov_imm(RSI, TCGETS);
        x64_lea(RDX, R14, ST_TERMIOS);
        x64_mov_imm(RAX, SYS_IOCTL);
        x64_syscall();
        x64_test(RAX, RAX);
        x64_jcc(CC_NZ, no);
        x64_mov_imm(RDI, 1);
        x64_mov_imm(RSI, TCGETS);
        x64_lea(RDX, R14, ST_RAW);
        x64_mov_imm(RAX, SYS_IOCTL);
        x64_syscall();
        x64_test(RAX, RAX);
        x64_jcc(CC_NZ, no);
        x64_store_imm(R14, ST_EDIT, 1);
        x64_bind(no);
        return;
    }
    x64_load(RAX, R14, ST_CONSOLE);
    x64_test(RAX, RAX);
    x64_jcc(CC_Z, no);
    x64_mov_imm(RCX, STD_OUTPUT_HANDLE_IN);
    x64_call_import(IMP_GET_STD_HANDLE);
    x64_store(R14, ST_HOUT, RAX);
    x64_mov(RCX, RAX);
    x64_lea(RDX, R14, ST_TMP);
    x64_call_import(IMP_GET_CONSOLE_MODE);
    x64_test(RAX, RAX);
    x64_jcc(CC_Z, no);
    x64_store_imm(R14, ST_EDIT, 1);
    x64_bind(no);
}

/* rdi のバイトを rsi 回、標準出力に書く */
static void gen_put_rep(void)
{
    int loop;
    int done;

    loop = x64_new_label(".L.qy_put_rep.loop");
    done = x64_new_label(".L.qy_put_rep.done");
    x64_bind(put_rep_label);
    x64_push(RBP);
    x64_mov(RBP, RSP);
    x64_sub_imm(RSP, 16);
    x64_store(R14, ST_OUTB, RDI);
    x64_store(RBP, -8, RSI);
    x64_bind(loop);
    x64_load(RAX, RBP, -8);
    x64_test(RAX, RAX);
    x64_jcc(CC_LE, done);
    x64_sub_imm(RAX, 1);
    x64_store(RBP, -8, RAX);
    x64_lea(RSI, R14, ST_OUTB);
    x64_mov_imm(RDX, 1);
    x64_call(write_label);
    x64_jmp(loop);
    x64_bind(done);
    x64_mov(RSP, RBP);
    x64_pop(RBP);
    x64_ret();
}

/* rsi が指す UTF-8 の1文字 → rax コードポイント、rdx バイト数（rcx を使う） */
static void gen_utf8_decode(void)
{
    int two;
    int three;
    int four;

    two = x64_new_label(".L.qy_utf8_decode.two");
    three = x64_new_label(".L.qy_utf8_decode.three");
    four = x64_new_label(".L.qy_utf8_decode.four");
    x64_bind(utf8_decode_label);
    x64_load8(RAX, RSI, 0);
    x64_mov_imm(RDX, 1);
    cmp_rax_imm(0x80);
    x64_jcc(CC_AE, two);
    x64_ret();
    x64_bind(two);
    cmp_rax_imm(0xE0);
    x64_jcc(CC_AE, three);
    x64_and_imm(RAX, 0x1F);
    x64_shl_imm(RAX, 6);
    x64_load8(RCX, RSI, 1);
    x64_and_imm(RCX, 0x3F);
    x64_or(RAX, RCX);
    x64_mov_imm(RDX, 2);
    x64_ret();
    x64_bind(three);
    cmp_rax_imm(0xF0);
    x64_jcc(CC_AE, four);
    x64_and_imm(RAX, 0x0F);
    x64_shl_imm(RAX, 12);
    x64_load8(RCX, RSI, 1);
    x64_and_imm(RCX, 0x3F);
    x64_shl_imm(RCX, 6);
    x64_or(RAX, RCX);
    x64_load8(RCX, RSI, 2);
    x64_and_imm(RCX, 0x3F);
    x64_or(RAX, RCX);
    x64_mov_imm(RDX, 3);
    x64_ret();
    x64_bind(four);
    x64_and_imm(RAX, 0x07);
    x64_shl_imm(RAX, 18);
    x64_load8(RCX, RSI, 1);
    x64_and_imm(RCX, 0x3F);
    x64_shl_imm(RCX, 12);
    x64_or(RAX, RCX);
    x64_load8(RCX, RSI, 2);
    x64_and_imm(RCX, 0x3F);
    x64_shl_imm(RCX, 6);
    x64_or(RAX, RCX);
    x64_load8(RCX, RSI, 3);
    x64_and_imm(RCX, 0x3F);
    x64_or(RAX, RCX);
    x64_mov_imm(RDX, 4);
    x64_ret();
}

/* rdi のコードポイント → rax UTF-8 のバイト（1バイト目が下位）、rdx バイト数（rcx・r8 を使う） */
static void gen_utf8_encode(void)
{
    int two;
    int three;
    int four;

    two = x64_new_label(".L.qy_utf8_encode.two");
    three = x64_new_label(".L.qy_utf8_encode.three");
    four = x64_new_label(".L.qy_utf8_encode.four");
    x64_bind(utf8_encode_label);
    x64_mov(RAX, RDI);
    x64_mov_imm(RDX, 1);
    cmp_rax_imm(0x80);
    x64_jcc(CC_AE, two);
    x64_ret();

    /* 続きのバイト 0x80 | (cp >> shift and 0x3F) を r8 に作り、rax に shift_out だけずらして加える */
#define CONT(shift, at) \
    x64_mov(R8, RDI); \
    if ((shift) > 0) { x64_shr_imm(R8, (shift)); } \
    x64_and_imm(R8, 0x3F); \
    x64_mov_imm(RCX, 0x80); \
    x64_or(R8, RCX); \
    if ((at) > 0) { x64_shl_imm(R8, (at)); } \
    x64_or(RAX, R8)

    x64_bind(two);
    cmp_rax_imm(0x800);
    x64_jcc(CC_AE, three);
    x64_shr_imm(RAX, 6);
    x64_mov_imm(RCX, 0xC0);
    x64_or(RAX, RCX);
    CONT(0, 8);
    x64_mov_imm(RDX, 2);
    x64_ret();
    x64_bind(three);
    cmp_rax_imm(0x10000);
    x64_jcc(CC_AE, four);
    x64_shr_imm(RAX, 12);
    x64_mov_imm(RCX, 0xE0);
    x64_or(RAX, RCX);
    CONT(6, 8);
    CONT(0, 16);
    x64_mov_imm(RDX, 3);
    x64_ret();
    x64_bind(four);
    x64_shr_imm(RAX, 18);
    x64_mov_imm(RCX, 0xF0);
    x64_or(RAX, RCX);
    CONT(12, 8);
    CONT(6, 16);
    CONT(0, 24);
    x64_mov_imm(RDX, 4);
    x64_ret();
#undef CONT
}

/*
 * rdi のコードポイントの表示幅 → rax（1 か 2）。東アジアの全角の文字と絵文字の主な範囲を 2 とする。
 * 範囲の表は [始め, 終わり] の組を rdata に置き、順に調べる。
 */
static void gen_cp_width(void)
{
    static const unsigned int wide[][2] = {
        { 0x1100, 0x115F }, { 0x2E80, 0x303E }, { 0x3041, 0x33FF }, { 0x3400, 0x4DBF },
        { 0x4E00, 0x9FFF }, { 0xA000, 0xA4CF }, { 0xAC00, 0xD7A3 }, { 0xF900, 0xFAFF },
        { 0xFE30, 0xFE4F }, { 0xFF00, 0xFF60 }, { 0xFFE0, 0xFFE6 }, { 0x1F300, 0x1F64F },
        { 0x1F900, 0x1F9FF }, { 0x20000, 0x2FFFD }, { 0x30000, 0x3FFFD }
    };
    unsigned char table[sizeof(wide) / sizeof(wide[0]) * 16];
    size_t offset;
    int n;
    int i;
    int k;
    int loop;
    int next;
    int found;
    int none;

    n = (int)(sizeof(wide) / sizeof(wide[0]));
    for (i = 0; i < n; i++) {
        for (k = 0; k < 8; k++) {
            table[i * 16 + k] = (unsigned char)((unsigned long long)wide[i][0] >> (8 * k));
            table[i * 16 + 8 + k] = (unsigned char)((unsigned long long)wide[i][1] >> (8 * k));
        }
    }
    offset = prog_add_data(prog, (const char *)table, sizeof(table));
    loop = x64_new_label(".L.qy_cp_width.loop");
    next = x64_new_label(".L.qy_cp_width.next");
    found = x64_new_label(".L.qy_cp_width.found");
    none = x64_new_label(".L.qy_cp_width.none");

    x64_bind(cp_width_label);
    x64_lea_data(RSI, offset);
    x64_mov_imm(RCX, n);
    x64_bind(loop);
    x64_test(RCX, RCX);
    x64_jcc(CC_Z, none);
    x64_load(RAX, RSI, 0);
    x64_cmp(RDI, RAX);
    x64_jcc(CC_B, next);
    x64_load(RAX, RSI, 8);
    x64_cmp(RDI, RAX);
    x64_jcc(CC_BE, found);
    x64_bind(next);
    x64_add_imm(RSI, 16);
    x64_sub_imm(RCX, 1);
    x64_jmp(loop);
    x64_bind(found);
    x64_mov_imm(RAX, 2);
    x64_ret();
    x64_bind(none);
    x64_mov_imm(RAX, 1);
    x64_ret();
}

/* rsi から rdx の手前までの UTF-8 の文字列の表示幅 → rax */
static void gen_str_width(void)
{
    int loop;
    int done;

    loop = x64_new_label(".L.qy_str_width.loop");
    done = x64_new_label(".L.qy_str_width.done");
    x64_bind(str_width_label);
    x64_push(RBP);
    x64_mov(RBP, RSP);
    x64_sub_imm(RSP, 32);
    x64_store(RBP, -8, RSI);
    x64_store(RBP, -16, RDX);
    x64_store_imm(RBP, -24, 0);
    x64_bind(loop);
    x64_load(RSI, RBP, -8);
    x64_load(RDX, RBP, -16);
    x64_cmp(RSI, RDX);
    x64_jcc(CC_AE, done);
    x64_call(utf8_decode_label);
    x64_load(RSI, RBP, -8);
    x64_add(RSI, RDX);
    x64_store(RBP, -8, RSI);
    x64_mov(RDI, RAX);
    x64_call(cp_width_label);
    x64_load(RCX, RBP, -24);
    x64_add(RCX, RAX);
    x64_store(RBP, -24, RCX);
    x64_jmp(loop);
    x64_bind(done);
    x64_load(RAX, RBP, -24);
    x64_mov(RSP, RBP);
    x64_pop(RBP);
    x64_ret();
}

/* 端末の設定を行の編集用に切り替える・元に戻す */
static void gen_raw_mode(void)
{
    x64_bind(raw_on_label);
    if (target == TARGET_LINUX) {
        /* 元の設定を写し、ICANON・ECHO・ISIG・IEXTEN（c_lflag）と IXON・ICRNL（c_iflag）を外し、VMIN=1・VTIME=0 */
        int i;
        for (i = 0; i < 40; i += 8) {
            x64_load(RAX, R14, ST_TERMIOS + i);
            x64_store(R14, ST_RAW + i, RAX);
        }
        x64_load(RAX, R14, ST_RAW + 0);         /* 下位 32 ビットが c_iflag */
        x64_and_imm(RAX, ~0x500);
        x64_store(R14, ST_RAW + 0, RAX);
        x64_load(RAX, R14, ST_RAW + 8);         /* 上位 32 ビットが c_lflag */
        x64_mov_imm(RCX, (long long)~(0x800BULL << 32));
        x64_and(RAX, RCX);
        x64_store(R14, ST_RAW + 8, RAX);
        x64_store8_imm(R14, ST_RAW + 17 + 5, 0);     /* VTIME */
        x64_store8_imm(R14, ST_RAW + 17 + 6, 1);     /* VMIN */
        x64_xor(RDI, RDI);
        x64_mov_imm(RSI, TCSETS);
        x64_lea(RDX, R14, ST_RAW);
        x64_mov_imm(RAX, SYS_IOCTL);
        x64_syscall();
        x64_ret();

        x64_bind(raw_off_label);
        x64_xor(RDI, RDI);
        x64_mov_imm(RSI, TCSETS);
        x64_lea(RDX, R14, ST_TERMIOS);
        x64_mov_imm(RAX, SYS_IOCTL);
        x64_syscall();
        x64_ret();
        return;
    }
    /* Windows：行の入力・エコー・Ctrl+C の処理を止める（SetConsoleMode） */
    x64_push(RBP);
    x64_mov(RBP, RSP);
    x64_and_imm(RSP, -16);
    x64_sub_imm(RSP, 32);
    x64_load(RCX, R14, ST_STDIN);
    x64_load(RDX, R14, ST_MODE);
    x64_and_imm(RDX, ~(ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));
    x64_call_import(IMP_SET_CONSOLE_MODE);
    x64_mov(RSP, RBP);
    x64_pop(RBP);
    x64_ret();

    x64_bind(raw_off_label);
    x64_push(RBP);
    x64_mov(RBP, RSP);
    x64_and_imm(RSP, -16);
    x64_sub_imm(RSP, 32);
    x64_load(RCX, R14, ST_STDIN);
    x64_load(RDX, R14, ST_MODE);
    x64_call_import(IMP_SET_CONSOLE_MODE);
    x64_mov(RSP, RBP);
    x64_pop(RBP);
    x64_ret();
}

/* rax の値が imm なら key を rax に入れて out へ */
static void map_key(long long imm, long long key, int out)
{
    int skip;

    skip = new_label("key.skip");
    cmp_rax_imm(imm);
    x64_jcc(CC_NZ, skip);
    x64_mov_imm(RAX, key);
    x64_jmp(out);
    x64_bind(skip);
}

/*
 * キーを1つ読む → rax（0 以上は文字のコードポイント、負は KEY_*）
 * Linux：端末から1バイトずつ読み、UTF-8 とエスケープシーケンス（矢印など）を解釈する。
 * Windows：ReadConsoleInputW でキーのイベントを読み、仮想キーコードと文字を解釈する。
 */
static void gen_key(void)
{
    int out;

    out = x64_new_label(".L.qy_key.out");
    x64_bind(key_label);
    x64_push(RBP);
    x64_mov(RBP, RSP);
    x64_sub_imm(RSP, 32);

    if (target == TARGET_LINUX) {
        int esc = x64_new_label(".L.qy_key.esc");
        int csi = x64_new_label(".L.qy_key.csi");
        int ss3 = x64_new_label(".L.qy_key.ss3");
        int tilde = x64_new_label(".L.qy_key.tilde");
        int tloop = x64_new_label(".L.qy_key.tloop");
        int multi = x64_new_label(".L.qy_key.multi");
        int cloop = x64_new_label(".L.qy_key.cloop");
        int cdone = x64_new_label(".L.qy_key.cdone");
        int ignore = x64_new_label(".L.qy_key.ignore");
        int n3 = x64_new_label(".L.qy_key.n3");
        int n4 = x64_new_label(".L.qy_key.n4");
        int ctrl = x64_new_label(".L.qy_key.ctrl");

        x64_call(getc_label);
        map_key(-1, KEY_EOF, out);
        map_key(-2, KEY_ERR, out);
        cmp_rax_imm(0x80);
        x64_jcc(CC_AE, multi);
        cmp_rax_imm(0x20);
        x64_jcc(CC_B, ctrl);
        map_key(0x7F, KEY_BS, out);
        x64_jmp(out);                   /* 印字できる ASCII */

        x64_bind(ctrl);
        map_key(0x03, KEY_INTR, out);
        map_key(0x04, KEY_EOFKEY, out);
        map_key(0x0D, KEY_ENTER, out);
        map_key(0x0A, KEY_ENTER, out);
        map_key(0x08, KEY_BS, out);
        map_key(0x01, KEY_HOME, out);   /* Ctrl+A */
        map_key(0x05, KEY_END, out);    /* Ctrl+E */
        map_key(0x02, KEY_LEFT, out);   /* Ctrl+B */
        map_key(0x06, KEY_RIGHT, out);  /* Ctrl+F */
        cmp_rax_imm(0x1B);
        x64_jcc(CC_Z, esc);
        x64_jmp(ignore);

        /* ESC [ … と ESC O … */
        x64_bind(esc);
        x64_call(getc_label);
        cmp_rax_imm('[');
        x64_jcc(CC_Z, csi);
        cmp_rax_imm('O');
        x64_jcc(CC_Z, ss3);
        x64_jmp(ignore);
        x64_bind(ss3);
        x64_call(getc_label);
        map_key('H', KEY_HOME, out);
        map_key('F', KEY_END, out);
        map_key('D', KEY_LEFT, out);
        map_key('C', KEY_RIGHT, out);
        x64_jmp(ignore);
        x64_bind(csi);
        x64_call(getc_label);
        map_key('D', KEY_LEFT, out);
        map_key('C', KEY_RIGHT, out);
        map_key('H', KEY_HOME, out);
        map_key('F', KEY_END, out);
        x64_mov(RCX, RAX);
        x64_sub_imm(RCX, '0');
        x64_mov_imm(RDX, 10);
        x64_cmp(RCX, RDX);
        x64_jcc(CC_AE, ignore);         /* 数字でなければ（上下の矢印など）無視 */
        x64_store(RBP, -8, RCX);        /* 最初の数字 */
        x64_store_imm(RBP, -16, 1);     /* 数字は1桁だけか */
        x64_bind(tloop);
        x64_call(getc_label);
        cmp_rax_imm('~');
        x64_jcc(CC_Z, tilde);
        cmp_rax_imm(-1);
        x64_jcc(CC_Z, ignore);
        x64_mov(RCX, RAX);
        x64_sub_imm(RCX, 0x40);
        x64_mov_imm(RDX, 0x3F);
        x64_cmp(RCX, RDX);
        x64_jcc(CC_B, ignore);          /* 終わりの文字（@〜~）なら無視して終える */
        x64_store_imm(RBP, -16, 0);
        x64_jmp(tloop);
        x64_bind(tilde);
        x64_load(RAX, RBP, -16);
        x64_test(RAX, RAX);
        x64_jcc(CC_Z, ignore);
        x64_load(RAX, RBP, -8);
        map_key(3, KEY_DEL, out);
        map_key(1, KEY_HOME, out);
        map_key(7, KEY_HOME, out);
        map_key(4, KEY_END, out);
        map_key(8, KEY_END, out);
        x64_jmp(ignore);

        /* UTF-8 の2バイト目以降を読んでコードポイントにする */
        x64_bind(multi);
        x64_store_imm(RBP, -16, 1);     /* 続きのバイトの数 */
        cmp_rax_imm(0xC0);
        x64_jcc(CC_B, ignore);
        cmp_rax_imm(0xE0);
        x64_jcc(CC_AE, n3);
        x64_and_imm(RAX, 0x1F);
        x64_jmp(cloop);
        x64_bind(n3);
        cmp_rax_imm(0xF0);
        x64_jcc(CC_AE, n4);
        x64_and_imm(RAX, 0x0F);
        x64_store_imm(RBP, -16, 2);
        x64_jmp(cloop);
        x64_bind(n4);
        x64_and_imm(RAX, 0x07);
        x64_store_imm(RBP, -16, 3);
        x64_bind(cloop);
        x64_store(RBP, -8, RAX);
        x64_load(RAX, RBP, -16);
        x64_test(RAX, RAX);
        x64_jcc(CC_Z, cdone);
        x64_sub_imm(RAX, 1);
        x64_store(RBP, -16, RAX);
        x64_call(getc_label);
        x64_test(RAX, RAX);
        x64_jcc(CC_S, ignore);
        x64_and_imm(RAX, 0x3F);
        x64_load(RCX, RBP, -8);
        x64_shl_imm(RCX, 6);
        x64_or(RAX, RCX);
        x64_jmp(cloop);
        x64_bind(cdone);
        x64_load(RAX, RBP, -8);
        x64_jmp(out);

        x64_bind(ignore);
        x64_mov_imm(RAX, KEY_IGNORE);
        x64_bind(out);
        x64_mov(RSP, RBP);
        x64_pop(RBP);
        x64_ret();
        return;
    }

    {
        int loop = x64_new_label(".L.qy_key.loop");
        int err = x64_new_label(".L.qy_key.err");
        int bychar = x64_new_label(".L.qy_key.bychar");
        int low = x64_new_label(".L.qy_key.low");
        int nolow = x64_new_label(".L.qy_key.nolow");

        x64_and_imm(RSP, -16);
        x64_sub_imm(RSP, 32);
        x64_bind(loop);
        /* ReadConsoleInputW(ハンドル, &レコード, 1, &読んだ数) */
        x64_load(RCX, R14, ST_STDIN);
        x64_lea(RDX, R14, ST_REC);
        x64_mov_imm(R8, 1);
        x64_lea(R9, R14, ST_COUNT);
        x64_call_import(IMP_READ_CONSOLE_INPUT_W);
        x64_test(RAX, RAX);
        x64_jcc(CC_Z, err);
        x64_load(RAX, R14, ST_COUNT);
        x64_shl_imm(RAX, 32);
        x64_shr_imm(RAX, 32);
        x64_test(RAX, RAX);
        x64_jcc(CC_Z, loop);
        x64_load(RAX, R14, ST_REC);
        x64_and_imm(RAX, 0xFFFF);       /* EventType */
        cmp_rax_imm(1);                 /* KEY_EVENT 以外は読み飛ばす */
        x64_jcc(CC_NZ, loop);
        x64_load(RAX, R14, ST_REC + 4);
        x64_shl_imm(RAX, 32);           /* bKeyDown（下位 32 ビット。and の即値は符号拡張されるのでシフトで取り出す） */
        x64_shr_imm(RAX, 32);
        x64_test(RAX, RAX);
        x64_jcc(CC_Z, loop);
        x64_load(RAX, R14, ST_REC + 10);
        x64_and_imm(RAX, 0xFFFF);       /* wVirtualKeyCode */
        map_key(0x25, KEY_LEFT, out);
        map_key(0x27, KEY_RIGHT, out);
        map_key(0x24, KEY_HOME, out);
        map_key(0x23, KEY_END, out);
        map_key(0x2E, KEY_DEL, out);
        map_key(0x08, KEY_BS, out);
        map_key(0x0D, KEY_ENTER, out);
        x64_bind(bychar);
        x64_load(RAX, R14, ST_REC + 14);
        x64_and_imm(RAX, 0xFFFF);       /* uChar */
        map_key(0x03, KEY_INTR, out);
        map_key(0x1A, KEY_EOFKEY, out);
        cmp_rax_imm(0x20);
        x64_jcc(CC_B, loop);            /* そのほかの制御文字や文字のないキーは読み飛ばす */
        /* サロゲートペア：上位を覚えておき、次の下位と組み合わせる */
        x64_mov(RCX, RAX);
        x64_sub_imm(RCX, 0xD800);
        x64_mov_imm(RDX, 0x400);
        x64_cmp(RCX, RDX);
        x64_jcc(CC_AE, low);
        x64_store(R14, ST_HIGH, RAX);
        x64_jmp(loop);
        x64_bind(low);
        x64_mov(RCX, RAX);
        x64_sub_imm(RCX, 0xDC00);
        x64_mov_imm(RDX, 0x400);
        x64_cmp(RCX, RDX);
        x64_jcc(CC_AE, out);            /* サロゲートでない文字 */
        x64_load(RDX, R14, ST_HIGH);
        x64_test(RDX, RDX);
        x64_jcc(CC_Z, nolow);
        x64_store_imm(R14, ST_HIGH, 0);
        x64_sub_imm(RDX, 0xD800);
        x64_shl_imm(RDX, 10);
        x64_mov(RAX, RCX);
        x64_add(RAX, RDX);
        x64_add_imm(RAX, 0x10000);
        x64_jmp(out);
        x64_bind(nolow);
        x64_mov_imm(RAX, 0xFFFD);
        x64_jmp(out);
        x64_bind(err);
        x64_mov_imm(RAX, KEY_ERR);
        x64_bind(out);
        x64_mov(RSP, RBP);
        x64_pop(RBP);
        x64_ret();
    }
}

/*
 * 行の編集で1行を読む → rax（readline と同じ：str、入力の終わりなら 0、読み込みの失敗なら 1）
 * [rbp-8] 文字列（データは +8 から）  [rbp-16] 長さ  [rbp-24] 容量  [rbp-32] カーソル（バイト）
 * [rbp-40] キー  [rbp-48]・[rbp-56]・[rbp-64] 作業用
 */
#define EB  (-8)
#define EL  (-16)
#define EC  (-24)
#define ECUR (-32)
#define EK  (-40)
#define T1  (-48)
#define T2  (-56)
#define T3  (-64)
#define EZERO (-72)
#define ESUF  (-80)     /* 右の枠 */
#define ESUFL (-88)     /* 右の枠のバイト数 */
#define ESUFW (-96)     /* 右の枠の表示幅（Linux） */

/* 文字列の [rbp+a] バイト目から [rbp+b] バイト目の手前までを書く（rsi・rdx を使う） */
static void edit_write_range(int a_slot, int b_slot)
{
    int skip;

    if (target != TARGET_LINUX) {
        return;                         /* Windows は win_redraw でまとめて描く */
    }
    skip = new_label("edit.wskip");
    x64_load(RDX, RBP, b_slot);
    x64_load(RAX, RBP, a_slot);
    x64_sub(RDX, RAX);
    x64_jcc(CC_LE, skip);
    x64_load(RSI, RBP, EB);
    x64_add_imm(RSI, 8);
    x64_add(RSI, RAX);
    x64_call(write_label);
    x64_bind(skip);
}

/* 文字列の [rbp+a] バイト目から [rbp+b] バイト目の手前までの表示幅 → rax */
static void edit_width_range(int a_slot, int b_slot)
{
    x64_load(RSI, RBP, EB);
    x64_add_imm(RSI, 8);
    x64_mov(RDX, RSI);
    x64_load(RAX, RBP, a_slot);
    x64_add(RSI, RAX);
    x64_load(RAX, RBP, b_slot);
    x64_add(RDX, RAX);
    x64_call(str_width_label);
}

/* rdi のバイトを [rbp+slot] 回書く */
static void edit_put_rep(int byte, int slot)
{
    if (target != TARGET_LINUX) {
        return;
    }
    x64_mov_imm(RDI, byte);
    x64_load(RSI, RBP, slot);
    x64_call(put_rep_label);
}

/*
 * カーソルを左へ戻す。Linux は「\b」を [rbp+count] 回書き、Windows は行の頭から [rbp+pos] バイト目までの
 * 表示幅の位置へ、コンソールの機能で直接置く（全角の文字や折り返しでもずれない）。
 */
static void edit_back(int count_slot, int pos_slot)
{
    (void)pos_slot;
    edit_put_rep('\b', count_slot);
}

/* Windows：行全体を描き直す（Linux では何もしない） */
static void edit_redraw(void)
{
    if (target == TARGET_LINUX) {
        return;
    }
    x64_load(RDI, RBP, EB);
    x64_add_imm(RDI, 8);
    x64_load(RSI, RBP, ECUR);
    x64_load(RDX, RBP, EL);
    x64_load(R8, RBP, ESUF);
    x64_load(R9, RBP, ESUFL);
    x64_call(win_redraw_label);
}

/* Linux：入力欄の後ろに右の枠を書く */
static void edit_write_suffix(void)
{
    int skip;

    if (target != TARGET_LINUX) {
        return;
    }
    skip = new_label("edit.suffix");
    x64_load(RDX, RBP, ESUFL);
    x64_test(RDX, RDX);
    x64_jcc(CC_Z, skip);
    x64_load(RSI, RBP, ESUF);
    x64_call(write_label);
    x64_bind(skip);
}

/* Linux：[rbp+slot] に右の枠の表示幅を足す（戻る量に右の枠の分を加える） */
static void edit_add_suffix_width(int slot)
{
    if (target != TARGET_LINUX) {
        return;
    }
    x64_load(RAX, RBP, slot);
    x64_load(RCX, RBP, ESUFW);
    x64_add(RAX, RCX);
    x64_store(RBP, slot, RAX);
}

/* カーソルの手前の文字の始め → [rbp+slot]（カーソルが 0 でないこと） */
static void edit_boundary(int direction)
{
    x64_load(RSI,RBP,EB);x64_add_imm(RSI,8);x64_load(RDX,RBP,EL);x64_add(RDX,RSI);x64_load(RDI,RBP,ECUR);
    x64_mov_imm(R8,W_CURSOR);x64_mov_imm(R9,64);x64_xor(R10,R10);x64_mov_imm(R11,direction);x64_call(wide_call_label);
}
static void edit_prev_start(int slot)
{edit_boundary(-1);x64_store(RBP,slot,RAX);}

static void gen_edit_line(void)
{
    int loop;
    int insert;
    int room;
    int mloop;
    int mdone;
    int sloop;
    int sdone;
    int on_bs;
    int on_del;
    int on_left;
    int on_right;
    int on_home;
    int on_end;
    int on_enter;
    int on_eofkey;
    int on_eof;
    int on_err;
    int on_intr;
    int finish;
    int ret_eof;
    int ret_err;
    int out;
    size_t nl;
    size_t intr_msg;

    nl = prog_add_data(prog, "\n", 1);
    intr_msg = prog_add_data(prog, "^C\n", 3);
    loop = x64_new_label(".L.qy_edit.loop");
    insert = x64_new_label(".L.qy_edit.insert");
    room = x64_new_label(".L.qy_edit.room");
    mloop = x64_new_label(".L.qy_edit.mloop");
    mdone = x64_new_label(".L.qy_edit.mdone");
    sloop = x64_new_label(".L.qy_edit.sloop");
    sdone = x64_new_label(".L.qy_edit.sdone");
    on_bs = x64_new_label(".L.qy_edit.bs");
    on_del = x64_new_label(".L.qy_edit.del");
    on_left = x64_new_label(".L.qy_edit.left");
    on_right = x64_new_label(".L.qy_edit.right");
    on_home = x64_new_label(".L.qy_edit.home");
    on_end = x64_new_label(".L.qy_edit.end");
    on_enter = x64_new_label(".L.qy_edit.enter");
    on_eofkey = x64_new_label(".L.qy_edit.eofkey");
    on_eof = x64_new_label(".L.qy_edit.eof");
    on_err = x64_new_label(".L.qy_edit.err");
    on_intr = x64_new_label(".L.qy_edit.intr");
    finish = x64_new_label(".L.qy_edit.finish");
    ret_eof = x64_new_label(".L.qy_edit.ret_eof");
    ret_err = x64_new_label(".L.qy_edit.ret_err");
    out = x64_new_label(".L.qy_edit.out");

    x64_bind(edit_line_label);
    x64_push(RBP);
    x64_mov(RBP, RSP);
    x64_sub_imm(RSP, 112);
    /* 右の枠を受け取り、状態の領域からは消しておく */
    x64_load(RAX, R14, ST_SUFFIX);
    x64_store(RBP, ESUF, RAX);
    x64_load(RAX, R14, ST_SUFFIX_LEN);
    x64_store(RBP, ESUFL, RAX);
    x64_store_imm(R14, ST_SUFFIX_LEN, 0);
    x64_store_imm(RBP, ESUFW, 0);
    x64_mov_imm(RDI, 64 + 8);
    x64_call(alloc_label);
    x64_store(RBP, EB, RAX);
    x64_store_imm(RBP, EL, 0);
    x64_store_imm(RBP, EC, 64);
    x64_store_imm(RBP, ECUR, 0);
    x64_store_imm(RBP, EZERO, 0);
    x64_call(raw_on_label);
    if (target != TARGET_LINUX) {
        x64_call(win_get_pos_label);    /* 行の編集を始めた位置 */
        x64_store(R14, ST_WSTARTX, RAX);
        x64_store(R14, ST_WOLDEND, RAX);
        edit_redraw();                  /* 右の枠を表示する */
    } else {
        /* 右の枠を書き、その幅だけカーソルを戻す */
        int nosuf = new_label("edit.nosuf");
        x64_load(RAX, RBP, ESUFL);
        x64_test(RAX, RAX);
        x64_jcc(CC_Z, nosuf);
        x64_load(RSI, RBP, ESUF);
        x64_mov(RDX, RSI);
        x64_add(RDX, RAX);
        x64_call(str_width_label);
        x64_store(RBP, ESUFW, RAX);
        edit_write_suffix();
        edit_put_rep('\b', ESUFW);
        x64_bind(nosuf);
    }

    x64_bind(loop);
    x64_call(key_label);
    x64_store(RBP, EK, RAX);
    x64_test(RAX, RAX);
    x64_jcc(CC_NS, insert);
    map_key(KEY_ENTER, 0, on_enter);
    x64_load(RAX, RBP, EK);
    map_key(KEY_BS, 0, on_bs);
    x64_load(RAX, RBP, EK);
    map_key(KEY_DEL, 0, on_del);
    x64_load(RAX, RBP, EK);
    map_key(KEY_LEFT, 0, on_left);
    x64_load(RAX, RBP, EK);
    map_key(KEY_RIGHT, 0, on_right);
    x64_load(RAX, RBP, EK);
    map_key(KEY_HOME, 0, on_home);
    x64_load(RAX, RBP, EK);
    map_key(KEY_END, 0, on_end);
    x64_load(RAX, RBP, EK);
    map_key(KEY_EOFKEY, 0, on_eofkey);
    x64_load(RAX, RBP, EK);
    map_key(KEY_EOF, 0, on_eof);
    x64_load(RAX, RBP, EK);
    map_key(KEY_ERR, 0, on_err);
    x64_load(RAX, RBP, EK);
    map_key(KEY_INTR, 0, on_intr);
    x64_jmp(loop);                      /* そのほかのキーは何もしない */

    /* 文字の挿入：UTF-8 にし、容量を確かめ、カーソルより後ろをずらして入れる */
    x64_bind(insert);
    x64_mov(RDI, RAX);
    x64_call(utf8_encode_label);
    x64_store(RBP, T1, RAX);            /* バイト */
    x64_store(RBP, T2, RDX);            /* バイト数 */
    x64_load(RCX, RBP, EL);
    x64_add(RCX, RDX);
    x64_load(RAX, RBP, EC);
    x64_cmp(RCX, RAX);
    x64_jcc(CC_BE, room);
    x64_shl_imm(RAX, 1);
    x64_store(RBP, EC, RAX);
    x64_mov(RDI, RAX);
    x64_add_imm(RDI, 8);
    x64_call(alloc_label);
    x64_load(RSI, RBP, EB);
    x64_add_imm(RSI, 8);
    x64_lea(RDI, RAX, 8);
    x64_load(RCX, RBP, EL);
    x64_rep_movsb();
    x64_store(RBP, EB, RAX);
    x64_bind(room);
    /* 後ろから1バイトずつ n バイト右へ：i = 長さ - 1 から カーソル まで */
    x64_load(RCX, RBP, EL);
    x64_bind(mloop);
    x64_load(RAX, RBP, ECUR);
    x64_cmp(RCX, RAX);
    x64_jcc(CC_LE, mdone);
    x64_sub_imm(RCX, 1);
    x64_load(RSI, RBP, EB);
    x64_add_imm(RSI, 8);
    x64_add(RSI, RCX);
    x64_load8(RAX, RSI, 0);
    x64_load(RDX, RBP, T2);
    x64_add(RSI, RDX);
    x64_store8(RSI, 0, RAX);
    x64_jmp(mloop);
    x64_bind(mdone);
    /* 文字のバイトを入れる */
    x64_load(RSI, RBP, EB);
    x64_add_imm(RSI, 8);
    x64_load(RAX, RBP, ECUR);
    x64_add(RSI, RAX);
    x64_load(RAX, RBP, T1);
    x64_load(RCX, RBP, T2);
    x64_bind(sloop);
    x64_test(RCX, RCX);
    x64_jcc(CC_Z, sdone);
    x64_store8(RSI, 0, RAX);
    x64_shr_imm(RAX, 8);
    x64_add_imm(RSI, 1);
    x64_sub_imm(RCX, 1);
    x64_jmp(sloop);
    x64_bind(sdone);
    x64_load(RAX, RBP, EL);
    x64_load(RCX, RBP, T2);
    x64_add(RAX, RCX);
    x64_store(RBP, EL, RAX);
    /* 入れた文字から後ろと右の枠を書き、その幅だけ戻る */
    edit_write_range(ECUR, EL);
    edit_write_suffix();
    x64_load(RAX, RBP, ECUR);
    x64_load(RCX, RBP, T2);
    x64_add(RAX, RCX);
    x64_store(RBP, ECUR, RAX);
    edit_width_range(ECUR, EL);
    x64_store(RBP, T3, RAX);
    edit_add_suffix_width(T3);
    edit_back(T3, ECUR);
    edit_redraw();
    x64_jmp(loop);

    /* Backspace：手前の文字を消す */
    x64_bind(on_bs);
    x64_load(RAX, RBP, ECUR);
    x64_test(RAX, RAX);
    x64_jcc(CC_Z, loop);
    edit_prev_start(T1);
    edit_width_range(T1, ECUR);
    x64_store(RBP, T3, RAX);            /* 消す文字の幅 */
    edit_back(T3, T1);
    /* [カーソル, 長さ) を [手前の始め] へずらす */
    {
        int l = x64_new_label(".L.qy_edit.bsmove");
        int d = x64_new_label(".L.qy_edit.bsdone");
        x64_load(RCX, RBP, ECUR);
        x64_load(RDX, RBP, T1);
        x64_bind(l);
        x64_load(RAX, RBP, EL);
        x64_cmp(RCX, RAX);
        x64_jcc(CC_AE, d);
        x64_load(RSI, RBP, EB);
        x64_add_imm(RSI, 8);
        x64_mov(RDI, RSI);
        x64_add(RSI, RCX);
        x64_add(RDI, RDX);
        x64_load8(RAX, RSI, 0);
        x64_store8(RDI, 0, RAX);
        x64_add_imm(RCX, 1);
        x64_add_imm(RDX, 1);
        x64_jmp(l);
        x64_bind(d);
    }
    x64_load(RAX, RBP, EL);
    x64_load(RCX, RBP, ECUR);
    x64_sub(RAX, RCX);
    x64_load(RCX, RBP, T1);
    x64_add(RAX, RCX);
    x64_store(RBP, EL, RAX);            /* 長さ -= 消したバイト数 */
    x64_load(RAX, RBP, T1);
    x64_store(RBP, ECUR, RAX);
    /* 後ろの部分を書き直し、消した幅だけ空白で消して、元の位置へ戻る */
    edit_write_range(ECUR, EL);
    edit_write_suffix();
    edit_put_rep(' ', T3);
    edit_width_range(ECUR, EL);
    x64_load(RCX, RBP, T3);
    x64_add(RAX, RCX);
    x64_store(RBP, T2, RAX);
    edit_add_suffix_width(T2);
    edit_back(T2, ECUR);
    edit_redraw();
    x64_jmp(loop);

    /* Delete：カーソルの位置の文字を消す */
    x64_bind(on_del);
    x64_load(RAX, RBP, ECUR);
    x64_load(RCX, RBP, EL);
    x64_cmp(RAX, RCX);
    x64_jcc(CC_AE, loop);
    edit_boundary(1);x64_store(RBP,T1,RAX);x64_load(RDX,RBP,ECUR);x64_sub(RAX,RDX);x64_store(RBP,T2,RAX);
    edit_width_range(ECUR,T1);x64_store(RBP,T3,RAX);
    {
        int l = x64_new_label(".L.qy_edit.delmove");
        int d = x64_new_label(".L.qy_edit.deldone");
        x64_load(RDX, RBP, ECUR);
        x64_mov(RCX, RDX);
        x64_load(RAX, RBP, T2);
        x64_add(RCX, RAX);
        x64_bind(l);
        x64_load(RAX, RBP, EL);
        x64_cmp(RCX, RAX);
        x64_jcc(CC_AE, d);
        x64_load(RSI, RBP, EB);
        x64_add_imm(RSI, 8);
        x64_mov(RDI, RSI);
        x64_add(RSI, RCX);
        x64_add(RDI, RDX);
        x64_load8(RAX, RSI, 0);
        x64_store8(RDI, 0, RAX);
        x64_add_imm(RCX, 1);
        x64_add_imm(RDX, 1);
        x64_jmp(l);
        x64_bind(d);
    }
    x64_load(RAX, RBP, EL);
    x64_load(RCX, RBP, T2);
    x64_sub(RAX, RCX);
    x64_store(RBP, EL, RAX);
    edit_write_range(ECUR, EL);
    edit_write_suffix();
    edit_put_rep(' ', T3);
    edit_width_range(ECUR, EL);
    x64_load(RCX, RBP, T3);
    x64_add(RAX, RCX);
    x64_store(RBP, T2, RAX);
    edit_add_suffix_width(T2);
    edit_back(T2, ECUR);
    edit_redraw();
    x64_jmp(loop);

    /* ← */
    x64_bind(on_left);
    x64_load(RAX, RBP, ECUR);
    x64_test(RAX, RAX);
    x64_jcc(CC_Z, loop);
    edit_prev_start(T1);
    edit_width_range(T1, ECUR);
    x64_store(RBP, T3, RAX);
    edit_back(T3, T1);
    x64_load(RAX, RBP, T1);
    x64_store(RBP, ECUR, RAX);
    edit_redraw();
    x64_jmp(loop);

    /* → */
    x64_bind(on_right);
    x64_load(RAX, RBP, ECUR);
    x64_load(RCX, RBP, EL);
    x64_cmp(RAX, RCX);
    x64_jcc(CC_AE, loop);
    edit_boundary(1);x64_store(RBP,T1,RAX);
    edit_write_range(ECUR, T1);
    x64_load(RAX, RBP, T1);
    x64_store(RBP, ECUR, RAX);
    edit_redraw();
    x64_jmp(loop);

    /* Home */
    x64_bind(on_home);
    x64_store_imm(RBP, T1, 0);
    edit_width_range(T1, ECUR);
    x64_store(RBP, T3, RAX);
    edit_back(T3, T1);
    x64_store_imm(RBP, ECUR, 0);
    edit_redraw();
    x64_jmp(loop);

    /* End */
    x64_bind(on_end);
    edit_write_range(ECUR, EL);
    x64_load(RAX, RBP, EL);
    x64_store(RBP, ECUR, RAX);
    edit_redraw();
    x64_jmp(loop);

    /* Ctrl+D・Ctrl+Z：空の行なら入力の終わり。Linux の Ctrl+D は、空でなければ Delete と同じ */
    x64_bind(on_eofkey);
    x64_load(RAX, RBP, EL);
    x64_test(RAX, RAX);
    x64_jcc(CC_Z, on_eof);
    if (target == TARGET_LINUX) {
        x64_jmp(on_del);
    } else {
        x64_jmp(loop);
    }

    /* 入力の終わり：何か打っていればその行を返し、空なら 0 */
    x64_bind(on_eof);
    x64_load(RAX, RBP, EL);
    x64_test(RAX, RAX);
    x64_jcc(CC_Z, ret_eof);
    x64_bind(on_enter);
    edit_write_range(ECUR, EL);         /* カーソルを行の終わり（右の枠の後ろ）へ */
    edit_write_suffix();
    if (target != TARGET_LINUX) {
        x64_load(RDI, R14, ST_WOLDEND);
        x64_call(win_set_pos_label);
    }
    x64_lea_data(RSI, nl);
    x64_mov_imm(RDX, 1);
    x64_call(write_label);
    x64_jmp(finish);

    x64_bind(ret_eof);
    x64_call(raw_off_label);
    x64_lea_data(RSI, nl);
    x64_mov_imm(RDX, 1);
    x64_call(write_label);
    x64_xor(RAX, RAX);
    x64_jmp(out);

    x64_bind(on_err);
    x64_bind(ret_err);
    x64_call(raw_off_label);
    x64_mov_imm(RAX, 1);
    x64_jmp(out);

    /* Ctrl+C：端末の設定を戻し、^C を表示して終了コード 130 で止める */
    x64_bind(on_intr);
    x64_call(raw_off_label);
    x64_lea_data(RSI, intr_msg);
    x64_mov_imm(RDX, 3);
    x64_call(write_label);
    x64_mov_imm(RAX, EXIT_INTERRUPT);
    x64_jmp(exit_label);

    x64_bind(finish);
    x64_call(raw_off_label);
    x64_load(RAX, RBP, EB);
    x64_load(RCX, RBP, EL);
    x64_store(RAX, 0, RCX);
    x64_bind(out);
    x64_mov(RSP, RBP);
    x64_pop(RBP);
    x64_ret();
}

#undef EB
#undef EL
#undef EC
#undef ECUR
#undef EK
#undef T1
#undef T2
#undef T3
#undef EZERO
#undef ESUF
#undef ESUFL
#undef ESUFW

static void gen_lineedit_routines(void)
{
    gen_put_rep();
    gen_utf8_decode();
    gen_utf8_encode();
    gen_cp_width();
    gen_str_width();
    gen_raw_mode();
    gen_win_cursor();
    gen_key();
    gen_edit_line();
}

#include "complex_input.inc"

static void gen_input_routines(void)
{
    gen_getc();
    gen_readline();
    gen_skip_ws();
    gen_word_follows();
    gen_parse_int();
    gen_integer_format_runtime();
    gen_dec_to_double();
    gen_parse_real();
    gen_complex_input_runtime();
    gen_parse_bool();
    gen_match_sep();
    gen_find_sep();
    gen_make_str();
    gen_input_error();
    gen_lineedit_routines();
}

/* ---------- input の呼び出し箇所 ---------- */

/* tok を含むソースの行 */
static void source_line(Token *tok, const char **start, int *len)
{
    const char *p;
    const char *q;

    p = tok->loc;
    while (p > tok->source->input && p[-1] != '\n') {
        p--;
    }
    q = tok->loc;
    while (*q != '\0' && *q != '\n' && *q != '\r') {
        q++;
    }
    *start = p;
    *len = (int)(q - p);
}

/* 実行時エラーのメッセージ（1行）を rdata に置き、rsi・rdx・r8 を設定して実行時エラーへ */
static void gen_fail(Token *tok, const char *text, int code)
{
    char msg[1024];
    int len;
    size_t offset;

    len = snprintf(msg, sizeof(msg), "QYNO 実行時エラー: %s:%d:%d: %s\n", tok ? tok->source->filename : "<runtime>", tok ? tok->line : 0, tok ? tok->col : 0, text);
    if (len < 0 || len >= (int)sizeof(msg)) {
        len = (int)strlen(msg);
    }
    offset = prog_add_data(prog, msg, (size_t)len);
    x64_lea_data(RSI, offset);
    x64_mov_imm(RDX, len);
    x64_mov_imm(R8, code);
    x64_jmp(runtime_error_label);
}

/* 入力の誤りの見出し（ファイル名・行・列と、ソースの該当行）を rdata に置く */
static size_t input_head(Token *tok, int *len_out)
{
    char *msg;
    const char *line;
    int line_len;
    int cap;
    int len;
    size_t offset;

    source_line(tok, &line, &line_len);
    cap = line_len + 1024;
    msg = malloc((size_t)cap);
    if (msg == NULL) {
        fatal("メモリを確保できません");
    }
    len = snprintf(msg, (size_t)cap, "QYNO 実行時エラー: %s:%d:%d: 入力を読み取れませんでした\n\t%d | %.*s\n",
                   tok->source->filename, tok->line, tok->col, tok->line, line_len, line);
    if (len < 0 || len >= cap) {
        len = (int)strlen(msg);
    }
    offset = prog_add_data(prog, msg, (size_t)len);
    free(msg);
    *len_out = len;
    return offset;
}

/*
 * 入力の誤りの表示へ移る（rsi に誤りの位置）
 * 見出しと理由を設定し、誤りの位置を行の先頭からのバイト数にして input_error へ。
 */
static void gen_input_fail(Node *node, size_t head, int head_len, const char *reason)
{
    size_t offset;
    int len;

    len = (int)strlen(reason);
    offset = prog_add_data(prog, reason, (size_t)len);
    x64_mov(RCX, RSI);
    x64_load(R9, RBP, -node->in_line->offset);
    x64_sub(RCX, R9);
    x64_sub_imm(RCX, 8);
    x64_lea_data(RSI, offset);
    x64_mov_imm(RDX, len);
    x64_lea_data(RDI, head);
    x64_mov_imm(R8, head_len);
    x64_jmp(input_error_label);
}

/* 表示用の区切りの文字（前後の空白を除き、空白だけなら「空白」） */
static void sep_display(InputField *f, char *buf, size_t cap)
{
    int s;
    int e;

    s = 0;
    e = f->sep_len;
    while (s < e && f->sep[s] == ' ') {
        s++;
    }
    while (e > s && f->sep[e - 1] == ' ') {
        e--;
    }
    if (s == e) {
        snprintf(buf, cap, "空白");
        return;
    }
    snprintf(buf, cap, "%.*s", e - s, f->sep + s);
}

/* 1行を読んで in_line・in_cur・in_end に置く。入力の終わりは eof へ */
static void gen_read_line_into(Node *node, int eof)
{
    x64_call(readline_label);
    x64_test(RAX, RAX);
    x64_jcc(CC_Z, eof);
    cmp_rax_imm(1);
    {
        int ok = new_label("input.read_ok");
        x64_jcc(CC_NZ, ok);
        gen_fail(node->tok, "入力の読み込みに失敗しました", EX_IOERR);
        x64_bind(ok);
    }
    x64_store(RBP, -node->in_line->offset, RAX);
    x64_lea(RSI, RAX, 8);
    x64_load(RDX, RAX, 0);
    x64_add(RDX, RSI);
    x64_store(RBP, -node->in_cur->offset, RSI);
    x64_store(RBP, -node->in_end->offset, RDX);
}

/* rax の double を float にする。無限大になれば range へ */
static void gen_real_to_float(int range)
{
    x64_movq_to_xmm(0, RAX);
    x64_cvtsd2ss(0, 0);
    x64_movd_from_xmm(RAX, 0);
    x64_mov(RCX, RAX);
    x64_and_imm(RCX, 0x7FFFFFFF);
    x64_mov_imm(R8, 0x7F800000);
    x64_cmp(RCX, R8);
    x64_jcc(CC_AE, range);
}

#include "models_codegen.inc"
#include "places_codegen.inc"
#include "library_codegen.inc"

/*
 * 値の形：input() ・ input(プロンプト)
 * プロンプトを表示して1行を読み、node->ty の値を rax に入れる。str なら行をそのまま返す。
 * 数値・真偽値は前後の空白を除いて読み、読めなければ実行時エラーで止める。
 */
static void gen_input_value(Node *node)
{
    int eof;
    int done;
    int missing;
    int type_err;
    int range_err;
    int extra;
    int head_len;
    size_t head;
    char reason[256];
    Type ty;

    ty = node->ty;
    eof = new_label("input.eof");
    done = new_label("input.done");
    if (node->lhs != NULL) {
        gen_print_value(node->lhs);
    }
    gen_read_line_into(node, eof);
    if (ty == TY_STR) {
        x64_load(RAX, RBP, -node->in_line->offset);
        x64_jmp(done);
        x64_bind(eof);
        gen_fail(node->tok, "入力が終わっていて、読み込めませんでした", EX_DATAERR);
        x64_bind(done);
        return;
    }

    missing = new_label("input.missing");
    type_err = new_label("input.type");
    range_err = new_label("input.range");
    extra = new_label("input.extra");
    x64_call(skip_ws_label);
    x64_cmp(RSI, RDX);
    x64_jcc(CC_AE, missing);
    if (type_is_integer(ty)) x64_mov_imm(RDI, 0);
    if (type_is_wide(ty)||ty==TY_VOID) gen_wide_parse(ty);
    else if (type_is_integer(ty)) gen_parse_integer(ty);
    else if (type_is_complex(ty)) x64_call(complex_parse_labels[ty == TY_C128]);
    else x64_call(ty == TY_BOOL ? parse_bool_label : parse_real_label);
    x64_mov_imm(R8, 1);
    x64_cmp(RCX, R8);
    x64_jcc(CC_Z, type_err);
    x64_mov_imm(R8, 2);
    x64_cmp(RCX, R8);
    x64_jcc(CC_Z, range_err);
    if (ty == TY_FLOAT) {
        x64_store(RBP, -node->in_cur->offset, RSI);
        gen_real_to_float(range_err);
        x64_load(RSI, RBP, -node->in_cur->offset);
        x64_load(RDX, RBP, -node->in_end->offset);
    }
    x64_store(RBP, -node->in_cur->offset, RAX);     /* 値を退避（位置は rsi に残す） */
    x64_call(skip_ws_label);
    x64_cmp(RSI, RDX);
    x64_jcc(CC_NZ, extra);
    x64_load(RAX, RBP, -node->in_cur->offset);
    x64_jmp(done);

    head = input_head(node->tok, &head_len);
    x64_bind(eof);
    gen_fail(node->tok, "入力が終わっていて、読み込めませんでした", EX_DATAERR);
    x64_bind(missing);
    gen_input_fail(node, head, head_len, "値が入力されていません");
    x64_bind(type_err);
    snprintf(reason, sizeof(reason), "%s として読めません", type_name(ty));
    gen_input_fail(node, head, head_len, reason);
    x64_bind(range_err);
    snprintf(reason, sizeof(reason), "%s の範囲を超えています", type_name(ty));
    gen_input_fail(node, head, head_len, reason);
    x64_bind(extra);
    gen_input_fail(node, head, head_len, "余計な文字があります");
    x64_bind(done);
}

/* 区切りが空白だけか */
static int sep_all_space(InputField *f)
{
    int i;

    for (i = 0; i < f->sep_len; i++) {
        if (f->sep[i] != ' ' && f->sep[i] != '\t') {
            return 0;
        }
    }
    return 1;
}

/* 区切りの文字を rdata に置き、rdi・rcx・r8 を設定する */
static void load_sep(InputField *f)
{
    size_t offset;
    int all_space;
    int i;

    offset = prog_add_data(prog, f->sep, (size_t)f->sep_len);
    all_space = 1;
    for (i = 0; i < f->sep_len; i++) {
        if (f->sep[i] != ' ' && f->sep[i] != '\t') {
            all_space = 0;
        }
    }
    x64_lea_data(RDI, offset);
    x64_mov_imm(RCX, f->sep_len);
    x64_mov_imm(R8, all_space);
}

/*
 * 形に当てはめる形：input("表示する文字{a}区切り{b}")
 * 表示してから1行を読み、{} ごとに読み取って一時変数に置く。すべて読めたら変数に移し、rax = 1。
 * 失敗したとき、条件として使っていれば rax = 0（変数は変えない）、文として使っていれば実行時エラーで止める。
 */
static void gen_input_pattern(Node *node)
{
    InputField *f;
    int done;
    int fail;
    int eof;
    int head_len;
    size_t head;
    char name[256];
    char reason[512];
    char sepbuf[256];
    Node *root;
    Type ty;
    PlacePlan *plans;
    int n;
    int i;
    int *missing;
    int *type_err;
    int *range_err;
    int *sep_err;
    int extra;
    int destination_err=new_label("input.destination.changed");

    done = new_label("input.done");
    fail = new_label("input.fail");
    eof = new_label("input.eof");
    extra = new_label("input.extra");
    n = 0;
    for (f = node->fields; f != NULL; f = f->next) {
        n++;
    }
    plans = calloc((size_t)n, sizeof(*plans));
    if (plans == NULL) fatal("入力先を確保できません");
    i = 0;
    for (f = node->fields; f != NULL; f = f->next, i++) {
        plans[i] = place_capture(f->var);
        collection_context(f->var->tok);
        place_read(&plans[i]);
    }
    missing = calloc((size_t)n, sizeof(int));
    type_err = calloc((size_t)n, sizeof(int));
    range_err = calloc((size_t)n, sizeof(int));
    sep_err = calloc((size_t)n, sizeof(int));
    if (missing == NULL || type_err == NULL || range_err == NULL || sep_err == NULL) {
        fatal("メモリを確保できません");
    }
    for (i = 0; i < n; i++) {
        missing[i] = new_label("input.missing");
        type_err[i] = new_label("input.type");
        range_err[i] = new_label("input.range");
        sep_err[i] = new_label("input.sep");
    }

    if (node->prompt_len > 0) {
        gen_write_data(prog_add_data(prog, node->prompt, (size_t)node->prompt_len), (size_t)node->prompt_len);
    }
    if (node->suffix_len > 0) {
        /* 右の枠を行の編集に渡す（端末でなければ readline が捨てる） */
        x64_lea_data(RAX, prog_add_data(prog, node->suffix, (size_t)node->suffix_len));
        x64_store(R14, ST_SUFFIX, RAX);
        x64_store_imm(R14, ST_SUFFIX_LEN, node->suffix_len);
    }
    gen_read_line_into(node, eof);

    i = 0;
    for (f = node->fields; f != NULL; f = f->next, i++) {
        ty = f->temp->ty;
        x64_load(RSI, RBP, -node->in_cur->offset);
        x64_load(RDX, RBP, -node->in_end->offset);
        if (type_is_text(ty)) {
            int nonempty = new_label("input.nonempty");
            if (f->next == NULL) {
                /* 最後の str は行の終わりまで */
                x64_mov_imm(RDI, 1);
                x64_call(make_str_label);
                x64_load(RCX, RAX, 0);
                x64_test(RCX, RCX);
                x64_jcc(CC_NZ, nonempty);
                x64_load(RSI, RBP, -node->in_cur->offset);
                x64_jmp(missing[i]);
                x64_bind(nonempty);
                x64_store(RBP, -f->temp->offset, RAX);
                if (ty == TY_CHAR) {
                    gen_text_measure_preserving();
                    x64_test(R8, R8);
                    x64_jcc(CC_NZ, type_err[i]);
                    x64_mov_imm(RCX, 1);
                    x64_cmp(R9, RCX);
                    x64_jcc(CC_NZ, type_err[i]);
                }
                x64_load(RCX, RBP, -node->in_end->offset);
                x64_store(RBP, -node->in_cur->offset, RCX);
                continue;
            }
            /* 区切りを探し、その手前までを str にする */
            load_sep(f);
            x64_call(find_sep_label);
            x64_test(RAX, RAX);
            {
                int found = new_label("input.found");
                x64_jcc(CC_NZ, found);
                x64_load(RSI, RBP, -node->in_end->offset);
                x64_jmp(sep_err[i]);
                x64_bind(found);
            }
            x64_store(RBP, -f->temp->offset, RSI);      /* 区切りの後の位置を一時的に置く */
            x64_mov(RDX, RAX);
            x64_load(RSI, RBP, -node->in_cur->offset);
            x64_mov_imm(RDI, 1);
            x64_call(make_str_label);
            x64_load(RCX, RAX, 0);
            x64_test(RCX, RCX);
            x64_jcc(CC_NZ, nonempty);
            x64_load(RSI, RBP, -node->in_cur->offset);
            x64_jmp(missing[i]);
            x64_bind(nonempty);
            x64_load(RCX, RBP, -f->temp->offset);
            x64_store(RBP, -node->in_cur->offset, RCX);
            x64_store(RBP, -f->temp->offset, RAX);
            if (ty == TY_CHAR) {
                gen_text_measure_preserving();
                x64_test(R8, R8);
                x64_jcc(CC_NZ, type_err[i]);
                x64_mov_imm(RCX, 1);
                x64_cmp(R9, RCX);
                x64_jcc(CC_NZ, type_err[i]);
            }
            continue;
        }

        x64_call(skip_ws_label);
        x64_cmp(RSI, RDX);
        x64_jcc(CC_AE, missing[i]);
        x64_store(RBP, -f->temp->offset, RSI);          /* 値の先頭（float の範囲外の表示に使う） */
        if (type_is_integer(ty)) x64_mov_imm(RDI, f->base);
        if (type_is_wide(ty)||ty==TY_VOID) gen_wide_parse(ty);
    else if (type_is_integer(ty)) gen_parse_integer(ty);
        else if (type_is_complex(ty)) x64_call(complex_parse_labels[ty == TY_C128]);
        else x64_call(ty == TY_BOOL ? parse_bool_label : parse_real_label);
        x64_mov_imm(R8, 1);
        x64_cmp(RCX, R8);
        x64_jcc(CC_Z, type_err[i]);
        x64_mov_imm(R8, 2);
        x64_cmp(RCX, R8);
        x64_jcc(CC_Z, range_err[i]);
        x64_store(RBP, -node->in_cur->offset, RSI);
        if (ty == TY_FLOAT) {
            int ok = new_label("input.float_ok");
            int bad = new_label("input.float_bad");
            gen_real_to_float(bad);
            x64_jmp(ok);
            x64_bind(bad);
            x64_load(RSI, RBP, -f->temp->offset);       /* 値の先頭を指す */
            x64_jmp(range_err[i]);
            x64_bind(ok);
        }
        x64_store(RBP, -f->temp->offset, RAX);
        x64_load(RSI, RBP, -node->in_cur->offset);
        x64_load(RDX, RBP, -node->in_end->offset);
        if (f->next != NULL) {
            if (!sep_all_space(f)) {
                x64_call(skip_ws_label);    /* 数値の後ろの空白は区切りの前に置いてよい */
            }
            load_sep(f);
            x64_call(match_sep_label);
            x64_test(RAX, RAX);
            {
                int matched = new_label("input.sep_ok");
                x64_jcc(CC_NZ, matched);
                x64_load(RSI, RBP, -node->in_cur->offset);  /* 誤りの位置は値の直後 */
                x64_jmp(sep_err[i]);
                x64_bind(matched);
            }
            x64_store(RBP, -node->in_cur->offset, RSI);
        } else {
            x64_call(skip_ws_label);
            x64_cmp(RSI, RDX);
            x64_jcc(CC_NZ, extra);
        }
    }

    /* 添字式の副作用で先の入力先が変わっていても、一部更新する前に検査する。 */
    for (i = 0; i < n; i++) {
        collection_context(plans[i].target->tok);
        place_read(&plans[i]);
    }

    /* Stage all parent strings before committing any target. Replacements can
     * merge graphemes and invalidate a later captured index in the same string. */
    i=0;
    for(f=node->fields;f;f=f->next,i++){
        InputField *previous=node->fields;int j;
        place_resolve(&plans[i],plans[i].count-plans[i].text_last);
        place_save(&plans[i],plans[i].count+1,RAX);x64_load(RAX,RAX,0);
        for(j=0;j<i;j++,previous=previous->next){
            int other=new_label("input.different.owner");
            place_load(&plans[i],RDX,plans[i].count+1);place_load(&plans[j],RCX,plans[j].count+1);
            x64_cmp(RDX,RCX);x64_jcc(CC_NZ,other);x64_load(RAX,RBP,-previous->temp->offset);x64_bind(other);
        }
        if(plans[i].text_last){
            place_save(&plans[i],plans[i].count+2,RAX);place_load(&plans[i],RDI,plans[i].count);x64_call(string_scan_label);
            x64_test(R8,R8);x64_jcc(CC_NZ,destination_err);x64_test(RDX,RDX);x64_jcc(CC_Z,destination_err);
            place_load(&plans[i],RAX,plans[i].count+2);place_load(&plans[i],RDI,plans[i].count);x64_load(RSI,RBP,-f->temp->offset);
            x64_call(string_replace_label);x64_store(RBP,-f->temp->offset,RAX);
        }
    }
    i=0;for(f=node->fields;f;f=f->next,i++){
        place_load(&plans[i],RAX,plans[i].count+1);x64_load(RDX,RBP,-f->temp->offset);x64_store(RAX,0,RDX);
    }
    x64_mov_imm(RAX, 1);
    x64_jmp(done);

    if (!node->input_stmt) {
        /* 条件として使う：失敗は false（入力の終わりも含む） */
        x64_bind(eof);
        x64_bind(extra);
        x64_bind(destination_err);
        for (i = 0; i < n; i++) {
            x64_bind(missing[i]);
            x64_bind(type_err[i]);
            x64_bind(range_err[i]);
            x64_bind(sep_err[i]);
        }
        x64_bind(fail);
        x64_xor(RAX, RAX);
        x64_bind(done);
        for (i = n - 1; i >= 0; i--) place_release(&plans[i]);
        free(plans);
        free(missing);
        free(type_err);
        free(range_err);
        free(sep_err);
        return;
    }

    head = input_head(node->tok, &head_len);
    x64_bind(eof);
    gen_fail(node->tok, "入力が終わっていて、読み込めませんでした", EX_DATAERR);
    x64_bind(destination_err);
    gen_input_fail(node,head,head_len,"文字の置換後に入力先の書記素境界が変わります");
    x64_bind(extra);
    gen_input_fail(node, head, head_len, "余計な文字があります");
    i = 0;
    for (f = node->fields; f != NULL; f = f->next, i++) {
        ty = f->temp->ty;
        root = f->var;
        while (root->kind == ND_INDEX) root = root->lhs;
        snprintf(name, sizeof(name), "%.*s", root->var->name_len, root->var->name);
        x64_bind(missing[i]);
        snprintf(reason, sizeof(reason), "「%s」に入れる値がありません", name);
        gen_input_fail(node, head, head_len, reason);
        x64_bind(type_err[i]);
        snprintf(reason, sizeof(reason), "「%s」に入れる値を %s として読めません", name, type_name(ty));
        gen_input_fail(node, head, head_len, reason);
        x64_bind(range_err[i]);
        snprintf(reason, sizeof(reason), "「%s」に入れる値が %s の範囲を超えています", name, type_name(ty));
        gen_input_fail(node, head, head_len, reason);
        x64_bind(sep_err[i]);
        if (f->sep != NULL) {
            sep_display(f, sepbuf, sizeof(sepbuf));
            snprintf(reason, sizeof(reason), "ここに「%s」が必要です", sepbuf);
        } else {
            snprintf(reason, sizeof(reason), "余計な文字があります");
        }
        gen_input_fail(node, head, head_len, reason);
    }
    x64_bind(fail);
    x64_bind(done);
    for (i = n - 1; i >= 0; i--) place_release(&plans[i]);
    free(plans);
    free(missing);
    free(type_err);
    free(range_err);
    free(sep_err);
}

static void gen_input(Node *node)
{
    if (node->input_pattern) {
        gen_input_pattern(node);
    } else {
        gen_input_value(node);
    }
}

/* ref の変数 v の置き場から使う場所を r に読み、何も指していなければ実行時エラー（終了コード 70） */
static void gen_ref_place(Var *v, Token *tok, Reg r)
{
    int ok;
    char msg[256];

    ok = new_label("ref.ok");
    x64_load(r, RBP, -v->offset);
    x64_test(r, r);
    x64_jcc(CC_NZ, ok);
    snprintf(msg, sizeof(msg), "ref の変数 %.*s が何も指していません", v->name_len, v->name);
    gen_fail(tok, msg, EX_SOFTWARE);
    x64_bind(ok);
}

/* 変数の値を rax に読む（ref の変数は、使う場所を通して読む） */
static void gen_load_var(Var *v, Token *tok)
{
    if (v->is_ref) {
        gen_ref_place(v, tok, RAX);
        x64_load(RAX, RAX, 0);
        return;
    }
    x64_load(RAX, RBP, -v->offset);
}

/* rax を変数に書く（ref の変数は、使う場所を通して書く。r11 を使う） */
static void gen_store_var(Var *v, Token *tok)
{
    if (type_is_array(v->ty) || type_is_model(v->ty)) {
        push();
        gen_load_var(v, tok);
        collection_context(tok);
        array_require_unborrowed();
        pop(RAX);
    }
    if (v->is_ref) {
        x64_push(RAX);
        gen_ref_place(v, tok, R11);
        x64_pop(RAX);
        x64_store(R11, 0, RAX);
        return;
    }
    x64_store(RBP, -v->offset, RAX);
}

/* 変数の場所を rax に入れる（ref の変数は、使っている場所。check が 1 なら何も指していないとき実行時エラー） */
static void gen_var_addr(Var *v, Token *tok, int check)
{
    if (v->is_ref) {
        if (check) {
            gen_ref_place(v, tok, RAX);
        } else {
            x64_load(RAX, RBP, -v->offset);
        }
        return;
    }
    x64_lea(RAX, RBP, -v->offset);
}


/* 関数呼び出し：引数を左から順に積み、呼び出した後に取り除く */
static void gen_call(Node *node)
{
    Node *arg;
    Node *part;
    int owners = 0;
    int bytes;
    int base;
    int slot;
    int i;
    for (arg = node->lhs; arg != NULL; arg = arg->next)
        if (arg->kind == ND_REFARG)
            for (part = arg->lhs; part->kind == ND_INDEX; part = part->lhs) owners++;
    if (owners == 0) {
        for (arg = node->lhs; arg != NULL; arg = arg->next) {
            gen_expr(arg);
            push();
        }
        if (node->kind == ND_BUILTIN && node->val >= BI_FILE_OPEN) gen_file_context(node);
        x64_call(node->func->label);
        if (node->nargs) {
            x64_add_imm(RSP, 8 * node->nargs);
            depth -= node->nargs;
        }
        return;
    }
    bytes = ((node->nargs + owners) * 8 + 15) / 16 * 16;
    base = emitting_function->stack_size + depth * 8 + bytes;
    if (bytes) { x64_sub_imm(RSP, bytes); depth += bytes / 8; }
    slot = node->nargs;
    i = 0;
    for (arg = node->lhs; arg != NULL; arg = arg->next, i++) {
        if (arg->kind == ND_REFARG) {
            arg->val = base - slot * 8;
            for (part = arg->lhs; part->kind == ND_INDEX; part = part->lhs) slot++;
        }
        gen_expr(arg);
        x64_store(RBP, -base + i * 8, RAX);
    }
    for (i = 0; i < node->nargs; i++) {
        x64_load(RAX, RBP, -base + i * 8);
        push();
    }
    if (node->kind == ND_BUILTIN && node->val >= BI_FILE_OPEN) gen_file_context(node);
    x64_call(node->func->label);
    if (node->nargs) {
        x64_add_imm(RSP, 8 * node->nargs);
        depth -= node->nargs;
    }
    for (i = node->nargs; i < slot; i++) {
        x64_load(RDX, RBP, -base + i * 8);
        x64_load(RCX, RDX, 32);
        x64_sub_imm(RCX, 1);
        x64_store(RDX, 32, RCX);
    }
    if (bytes) { x64_add_imm(RSP, bytes); depth -= bytes / 8; }
}

/* 式を評価し、結果を rax に入れる */
static void gen_expr_inner(Node *node);
static void gen_expr(Node *node)
{
    gen_expr_inner(node);
    if(node->ty==TY_VOID)x64_xor(RAX,RAX);
    if (type_is_integer(node->ty)) gen_integer_normalize(node->ty);
}
static void gen_expr_inner(Node *node)
{
    switch (node->kind) {
    case ND_COMPLEX:
        x64_mov_imm(RAX, double_bits(node->fval)); x64_mov_imm(RDI, double_bits(node->imag));
        x64_call(complex_make_labels[node->ty == TY_C128]); return;
    case ND_CREAL: case ND_CIMAG:
        gen_expr(node->lhs); gen_complex_unpack(RAX, 0, 1);
        x64_movq_from_xmm(RAX, node->kind == ND_CREAL ? 0 : 1); gen_convert(TY_DOUBLE, node->ty); return;
    case ND_CONSTRUCT:
        gen_construction(node); return;
    case ND_MODEL_SEED:
        gen_model_init(node); return;
    case ND_BUILD_CONTEXT:
        gen_creation_context(node->tok); return;
    case ND_MODEL_NEW:
        gen_model_default(type_model(node->ty), node->tok); return;
    case ND_MODEL_INIT:
        gen_model_init(node); return;
    case ND_MODEL_COPY:
        gen_expr(node->lhs); x64_call(type_model(node->ty)->copy_label); return;
    case ND_ARRAY_LITERAL:
        gen_array_literal(node);
        return;
    case ND_ARRAY_NEW:
    case ND_ARRAY_COPY:
        if (node->kind == ND_ARRAY_NEW) x64_xor(RAX, RAX);
        else gen_expr(node->lhs);
        collection_context(node->tok);
        collection_shape(node->ty, node->dims, RDI);
        x64_mov_imm(RSI, node->kind == ND_ARRAY_NEW ? 1 : node->val);
        x64_call(array_copy_label);
        return;
    case ND_METHOD:
        gen_array_method(node);
        return;
    case ND_SAVED:
        x64_load(RAX, RBP, -(int)node->val);
        return;
    case ND_UNIT:x64_xor(RAX,RAX);return;
    case ND_WIDE: gen_wide_literal(node);return;
    case ND_NUM:
    case ND_BOOL:
        x64_mov_imm(RAX, node->val);
        return;
    case ND_FNUM:
        if (node->ty == TY_FLOAT) {
            x64_mov_imm(RAX, float_bits((float)node->fval));
        } else {
            x64_mov_imm(RAX, double_bits(node->fval));
        }
        return;
    case ND_STRLIT:
        x64_lea_data(RAX, prog_add_str(prog, node->str, (size_t)node->str_len));
        return;
    case ND_INDEX:
    case ND_LENGTH:
        if (type_is_array(node->lhs->ty) || type_is_model(node->lhs->ty)) {
            gen_expr(node->lhs);
            if (node->kind == ND_LENGTH) x64_load(RAX, RAX, 0);
            else {
                push(); gen_expr(node->rhs); x64_mov(RDI, RAX); pop(RAX);
                collection_context(node->tok); x64_call(array_get_label);
            }
        } else gen_text_operation(node);
        return;
    case ND_CALL:
        gen_call(node);
        return;
    case ND_BUILTIN:
        if (node->val == BI_COMPLEX) { gen_complex_constructor(node); return; }
        if(node->val>=BI_LIB_SIZE && node->val<BI_FILE_OPEN) gen_library_builtin(node);
        else if (node->val >= BI_FILE_OPEN) gen_file_builtin(node);
        else gen_builtin(node);
        return;
    case ND_ABS:
        gen_expr(node->lhs);
        if(type_is_wide(node->ty)){gen_wide(W_ABS,node->ty,node->ty,0,node->tok);return;}
        if (type_is_complex(node->lhs->ty)) { x64_call(complex_abs_label); gen_convert(TY_DOUBLE, node->ty); return; }
        gen_abs_rax(node->ty);
        return;
    case ND_INPUT:
        gen_input(node);
        return;
    case ND_VAR:
        if (node->var->is_const) {
            gen_expr(node->var->const_val);     /* const はコンパイル時に決めた値 */
        } else {
            gen_load_var(node->var, node->tok);
        }
        return;
    case ND_ADDR:
        gen_var_addr(node->lhs->var, node->tok, 0);
        return;
    case ND_NONE:
        x64_xor(RAX, RAX);
        return;
    case ND_REFARG:
        if (node->rhs) {
            gen_expr(node->rhs);
            x64_store(RBP, -(node->ref_temp ? node->ref_temp : node->lhs->var)->offset, RAX);
        }
        if (node->lhs->kind == ND_INDEX) gen_element_ref(node);
        else gen_var_addr(node->lhs->var, node->tok, 1);
        if (type_is_array(node->lhs->ty)) {
            collection_context(node->tok);
            collection_shape(node->lhs->ty, node->dims, RDI);
            x64_call(array_ref_label);
        }
        return;
    case ND_CHAIN:
        gen_chain(node);
        return;
    case ND_INT_PARSE:
    case ND_INT_FORMAT:
        gen_integer_conversion(node); return;
    case ND_BIT_NOT:
        gen_expr(node->lhs);if(type_is_wide(node->ty)){gen_wide(W_NOT,node->ty,node->ty,0,node->tok);return;} x64_xor_imm(RAX, -1); return;
    case ND_BIT_AND:
    case ND_BIT_OR:
    case ND_BIT_XOR:
    case ND_XOR:
    case ND_SHL:
    case ND_SHR:
        gen_expr(node->lhs); push(); gen_expr(node->rhs); x64_mov(RCX, RAX); pop(RAX);
        if(type_is_wide(node->ty)){x64_mov(RDI,RCX);gen_wide(wide_codegen_op(node->kind),node->ty,node->ty,0,node->tok);return;}
        if (node->kind == ND_BIT_AND) x64_and(RAX, RCX);
        else if (node->kind == ND_BIT_OR) x64_or(RAX, RCX);
        else if (node->kind == ND_BIT_XOR || node->kind == ND_XOR) x64_xor(RAX, RCX);
        else {
            int valid = new_label("shift.valid"), invalid = new_label("shift.invalid");
            x64_test(RCX, RCX); x64_jcc(CC_S, invalid);
            x64_mov_imm(RDX, integer_width(node->ty) - 1); x64_cmp(RCX, RDX); x64_jcc(CC_BE, valid);
            x64_bind(invalid);
            { char message[128]; snprintf(message, sizeof(message), "シフト量は0以上%d以下にしてください", integer_width(node->ty) - 1); gen_fail(node->tok, message, EX_SOFTWARE); }
            x64_bind(valid);
            if (node->kind == ND_SHL) x64_shl_cl(RAX); else if (integer_unsigned(node->ty)) x64_shr_cl(RAX); else x64_sar_cl(RAX);
        }
        return;
    case ND_AND:
    case ND_OR:
        gen_logic(node);
        return;
    case ND_NOT:
        gen_expr(node->lhs);
        x64_xor_imm(RAX, 1);
        return;
    case ND_CAST:
        gen_cast(node);
        return;
    case ND_NEG:
        gen_neg(node);
        return;
    case ND_ADD:
    case ND_SUB:
    case ND_MUL:
    case ND_DIV:
    case ND_MOD:
        gen_binary(node);
        return;
    case ND_POW:
        /* 底、指数の順に評価し、定義域を検査してから計算する。 */
        gen_expr(node->lhs);
        push();
        gen_expr(node->rhs);
        x64_mov(RDI, RAX);
        pop(RAX);
        if(type_is_wide(node->ty)){gen_wide(W_POW,node->ty,node->ty,0,node->tok);return;}
        if (type_is_complex(node->ty)) { collection_context(node->tok); x64_call(complex_pow_labels[node->ty == TY_C128]); return; }
        gen_pow_check(node);
        if (type_is_integer(node->ty)) {
            x64_call(pow_int_label);
        } else if (node->ty == TY_FLOAT) {
            x64_call(pow_float_label);
        } else {
            x64_call(pow_double_label);
        }
        return;
    default:
        fatal("内部エラー: 未対応の式です");
    }
}

/* ---------- 文 ---------- */

/* 式の値を型に合わせて出力する */
static void gen_print_value(Node *expr)
{
    gen_expr(expr);
    if (expr->format_base) {
        x64_mov_imm(RDI, expr->format_base); x64_mov_imm(RSI, 1); gen_format_integer(expr->ty);
        x64_call(print_str_label); return;
    }
    if(type_is_wide(expr->ty)){x64_mov_imm(RDI,10);x64_xor(RSI,RSI);gen_wide(W_FORMAT,expr->ty,expr->ty,0,NULL);x64_call(print_str_label);return;}
    if (type_is_complex(expr->ty)) { x64_call(complex_print_labels[expr->ty == TY_C128]); return; }
    if (type_is_array(expr->ty)) { x64_call(array_print_label); return; }
    if (type_is_model(expr->ty)) { x64_call(type_model(expr->ty)->print_label); return; }
    if (type_is_integer(expr->ty)) { x64_call(integer_unsigned(expr->ty) ? print_uint_label : print_int_label); return; }
    switch (expr->ty) {
    case TY_VOID:gen_write_const("()");return;
    case TY_FLOAT:
        gen_convert(TY_FLOAT, TY_DOUBLE);
        x64_call(print_float_label);
        return;
    case TY_DOUBLE:
        x64_call(print_double_label);
        return;
    case TY_BOOL:
        x64_call(print_bool_label);
        return;
    case TY_STR:
    case TY_CHAR:
        x64_call(print_str_label);
        return;
    case TY_ADDR:
        x64_call(print_addr_label);
        return;
    default:
        fatal("内部エラー: 出力できない型です");
    }
}

/*
 * print の部分の並びを出力する
 *   literal：文字列リテラル直書き（固定部分 ND_STR と {式} の並び）なら 1、任意の型の式1つなら 0
 *   add_newline：最後に改行を出すなら 1（最後の固定部分とまとめて1回で書く）
 */
static void gen_file_context(Node *node)
{
    char prefix[1200];
    int n = snprintf(prefix, sizeof(prefix), "QYNO 実行時エラー: %s:%d:%d: io.file.%s: ",
                     node->tok->source->filename, node->tok->line, node->tok->col,
                     builtin_name((Builtin)node->val));
    if (n < 0) fatal("ファイルの診断を作成できません");
    if ((size_t)n >= sizeof(prefix)) n = (int)sizeof(prefix) - 1;
    x64_lea_data(R11, prog_add_str(prog, prefix, (size_t)n));
    x64_store(R14, FILE_CONTEXT, R11);
    x64_store_imm(R14, FILE_PATH, 0); x64_store_imm(R14, FILE_OS_ERROR, 0);
}

static void gen_file_builtin(Node *node)
{
    if (node->val == BI_FILE_WRITE) {
        Node *value = node->lhs->next;
        Node *part;
        gen_expr(node->lhs); push();
        x64_mov_imm(RDI, 24); x64_call(alloc_label); push();
        for (part = value->kind == ND_FORMAT ? value->lhs : value; part != NULL; part = part->next) {
            if (part->kind == ND_STR) {
                x64_load(RAX, RSP, 0);
                x64_lea_data(RSI, prog_add_data(prog, part->str, (size_t)part->str_len));
                x64_mov_imm(RDX, part->str_len); x64_call(file_append_label);
            } else {
                Node saved = *part;
                gen_expr(part); push();
                saved.kind = ND_SAVED; saved.val = emitting_function->stack_size + depth * 8;
                x64_load(R10, RSP, 8); x64_store(R14, OUTPUT_SINK, R10);
                gen_print_value(&saved);
                x64_store_imm(R14, OUTPUT_SINK, 0); pop(RAX);
            }
        }
        gen_file_context(node);
        x64_load(RAX, RSP, 8); x64_mov_imm(RDI, 2); x64_call(file_check_label);
        x64_load(RAX, RSP, 0); x64_call(file_string_label); x64_mov(RDI, RAX);
        pop(RDX); pop(RAX); x64_call(file_labels[BI_FILE_WRITE - BI_FILE_OPEN]);
    } else {
        Function function = {0};
        Node call = *node;
        function.label = file_labels[node->val - BI_FILE_OPEN]; call.func = &function;
        gen_call(&call);
    }
}

static void gen_print_parts(Node *list, int literal, int add_newline)
{
    Node *part;
    size_t offset;
    int newline_done;

    newline_done = 0;
    for (part = list; part != NULL; part = part->next) {
        if (literal && part->kind == ND_STR) {
            if (add_newline && part->next == NULL) {
                offset = prog_add_data(prog, part->str, (size_t)part->str_len);
                prog_add_data(prog, "\n", 1);
                gen_write_data(offset, (size_t)part->str_len + 1);
                newline_done = 1;
            } else if (part->str_len > 0) {
                offset = prog_add_data(prog, part->str, (size_t)part->str_len);
                gen_write_data(offset, (size_t)part->str_len);
            }
            continue;
        }
        gen_print_value(part);
    }
    if (add_newline && !newline_done) {
        gen_write_data(newline_offset, 1);
    }
}

/* print 文：値を出力し、最後に end=（省略時は改行）を出力する */
static void gen_print(Node *node)
{
    gen_print_parts(node->lhs, node->val, !node->has_end);
    if (node->has_end) {
        gen_print_parts(node->end, node->end_literal, 0);
    }
}

static void gen_stmt(Node *node);

static void gen_stmts(Node *list)
{
    Node *node;

    for (node = list; node != NULL; node = node->next) {
        gen_stmt(node);
        if (depth != 0) {
            fatal("内部エラー: スタックの深さが %d のままです", depth);
        }
    }
}

static void gen_if(Node *node)
{
    int else_label;
    int end_label;

    else_label = new_label("if.else");
    end_label = new_label("if.end");
    gen_expr(node->cond);
    x64_test(RAX, RAX);
    x64_jcc(CC_Z, else_label);
    gen_stmts(node->then);
    if (node->els != NULL) {
        x64_jmp(end_label);
    }
    x64_bind(else_label);
    gen_stmts(node->els);
    x64_bind(end_label);
}

static void push_loop(int break_label, int continue_label)
{
    if (loop_depth >= MAX_LOOP_DEPTH) {
        fatal("ループの入れ子が深すぎます（%d 段まで）", MAX_LOOP_DEPTH);
    }
    break_labels[loop_depth] = break_label;
    continue_labels[loop_depth] = continue_label;
    loop_depth++;
}

/*
 * while：
 *   begin: 条件 → 偽なら end へ
 *          本体
 *          begin へ戻る（continue() もここへ）
 *   end:
 */
static void gen_while(Node *node)
{
    int begin;
    int end;

    begin = new_label("while.begin");
    end = new_label("while.end");
    x64_bind(begin);
    gen_expr(node->cond);
    x64_test(RAX, RAX);
    x64_jcc(CC_Z, end);
    push_loop(end, begin);
    gen_stmts(node->then);
    loop_depth--;
    x64_jmp(begin);
    x64_bind(end);
}

/* rax を保持し、NaN / 無限大のヘッダー値を拒否する。 */
static void gen_for_finite(Type ty, Token *tok, const char *message)
{
    int ok;
    if (type_is_integer(ty)) return;
    if(type_is_wide(ty)){
        ok=new_label("for.wide.finite");x64_mov(RCX,RAX);
        if(ty==TY_QUAD){x64_test(RCX,RCX);x64_jcc(CC_Z,ok);x64_load(RCX,RCX,8);}
        x64_mov_imm(RDX,ty==TY_HALF?31744:INT64_C(0x7fff000000000000));x64_and(RCX,RDX);x64_cmp(RCX,RDX);x64_jcc(CC_NZ,ok);gen_fail(tok,message,EX_SOFTWARE);x64_bind(ok);return;
    }
    ok = new_label("for.finite");
    x64_mov(RCX, RAX);
    x64_mov_imm(RDX, ty == TY_FLOAT ? 0x7f800000LL : 0x7ff0000000000000LL);
    x64_and(RCX, RDX);
    x64_cmp(RCX, RDX);
    x64_jcc(CC_NZ, ok);
    gen_fail(tok, message, EX_SOFTWARE);
    x64_bind(ok);
}

/* 初期候補と通常回で共通の更新。int の範囲外は終了、小数の停滞は診断する。 */
static void gen_for_update(Node *node, int end)
{
    Var *v = node->init->var;
    Token *tok = node->inc->tok;
    int progress;
    if(type_is_integer(v->ty)&&type_is_wide(v->ty)){
        gen_expr(node->for_amount);push();gen_load_var(v,tok);pop(RDI);gen_wide(W_NEXT,v->ty,v->ty,node->inc_sign,NULL);
        x64_test(R8,R8);x64_jcc(CC_NZ,end);gen_store_var(v,tok);return;
    }
    if (type_is_integer(v->ty)) {
        gen_expr(node->for_amount);
        x64_mov(RDI, RAX);
        gen_load_var(v, tok);
        if (node->inc_sign > 0) x64_add(RAX, RDI);
        else x64_sub(RAX, RDI);
        x64_jcc(integer_unsigned(v->ty) ? CC_B : CC_O, end);
        gen_integer_range(v->ty, v->ty, end); /* store や他の命令でフラグを壊す前に検査する */
        gen_store_var(v, tok);
        return;
    }
    gen_load_var(v, tok);
    gen_for_finite(v->ty, tok, "for の更新前の値は有限の数値にしてください");
    push();
    gen_expr(node->step->lhs); /* 既存の型変換・演算・丸め順序を維持する */
    gen_for_finite(v->ty, tok, "for の更新結果が有限ではありません");
    pop(RDI);
    if(type_is_wide(v->ty)){
        push();gen_wide(W_CMP,v->ty,v->ty,node->val>0?CMP_GT:CMP_LT,NULL);x64_test(RAX,RAX);pop(RAX);progress=new_label("for.wide.progress");x64_jcc(CC_NZ,progress);gen_fail(tok,"for の増分では値が前進しません",EX_SOFTWARE);x64_bind(progress);gen_store_var(v,tok);return;
    }
    if (v->ty == TY_FLOAT) {
        x64_movd_to_xmm(0, RDI);
        x64_cvtss2sd(0, 0);
        x64_movd_to_xmm(1, RAX);
        x64_cvtss2sd(1, 1);
    } else {
        x64_movq_to_xmm(0, RDI);
        x64_movq_to_xmm(1, RAX);
    }
    progress = new_label("for.progress");
    x64_ucomisd(node->val > 0 ? 1 : 0, node->val > 0 ? 0 : 1);
    x64_jcc(CC_A, progress);
    gen_fail(tok, "for の増分では値が前進しません（丸め後の値を確認してください）", EX_SOFTWARE);
    x64_bind(progress);
    gen_store_var(v, tok);
}

/* 各辺を一度だけ評価する。比較型への変換で非有限値になる場合も検査する。 */
static void gen_for_condition(Node *node)
{
    Node *cmp = node->cond->lhs;
    Node converted = {0};
    Node comparison = *cmp;
    converted.ty = cmp->cmp_ty == TY_FLOAT ? TY_DOUBLE : cmp->cmp_ty;
    comparison.cmp_ty = converted.ty;
    comparison.lhs = &converted;
    comparison.rhs = &converted;
    gen_expr(cmp->lhs);
    gen_for_finite(cmp->lhs->ty, cmp->lhs->tok, "for のループ変数は有限の数値にしてください");
    gen_convert_for_compare(cmp->lhs->ty, cmp->cmp_ty);
    push();
    gen_expr(cmp->rhs);
    gen_for_finite(cmp->rhs->ty, cmp->rhs->tok, "for の終了境界は有限の数値にしてください");
    gen_convert_for_compare(cmp->rhs->ty, cmp->cmp_ty);
    gen_for_finite(converted.ty, cmp->rhs->tok, "for の終了境界は有限の数値にしてください");
    pop(RDI);
    gen_compare(&comparison);
}

/* 開いた開始側だけ最初に更新する。continue は更新、break は終了へ進む。 */
static void gen_for(Node *node)
{
    int begin;
    int next;
    int end;

    begin = new_label("for.begin");
    next = new_label("for.next");
    end = new_label("for.end");
    gen_stmt(node->init);
    gen_load_var(node->init->var, node->init->tok);
    gen_for_finite(node->init->var->ty, node->init->lhs->tok, "for の開始値は有限の数値にしてください");
    if (node->for_open) gen_for_update(node, end);
    x64_bind(begin);
    gen_for_condition(node);
    x64_test(RAX, RAX);
    x64_jcc(CC_Z, end);
    push_loop(end, next);
    gen_stmts(node->then);
    loop_depth--;
    x64_bind(next);
    gen_for_update(node, end);
    x64_jmp(begin);
    x64_bind(end);
}

static void gen_stmt(Node *node)
{
    switch (node->kind) {
    case ND_BUILD_REJECT: {
        char reason[256];
        gen_creation_context(node->tok); x64_store(R14, COLLECTION_CONTEXT, RAX);
        snprintf(reason, sizeof(reason), "%s の初期化が拒否されました\n", type_name((Type)node->val));
        collection_fail(reason); return;
    }
    case ND_IF:
        gen_if(node);
        return;
    case ND_WHILE:
        gen_while(node);
        return;
    case ND_FOR:
        gen_for(node);
        return;
    case ND_BREAK:
        x64_jmp(break_labels[loop_depth - 1]);
        return;
    case ND_CONTINUE:
        x64_jmp(continue_labels[loop_depth - 1]);
        return;
    case ND_DECL:
        if (node->var->is_const) {
            return;             /* const は値を直接埋め込むので領域を使わない */
        }
        if (node->lhs != NULL) {
            gen_expr(node->lhs);
            if (node->var->is_ref && type_is_array(node->var->ty)) {
                collection_context(node->tok);
                collection_shape(node->var->ty, node->var->dims, RDI);
                x64_call(array_ref_label);
            }
        } else if (node->var->is_ref) {
            x64_xor(RAX, RAX);                      /* 初期値のない ref は何も指さない */
        } else if (node->var->ty == TY_STR) {
            x64_lea_data(RAX, empty_str_offset);    /* 初期値を省いた str は空文字列 */
        } else if (node->var->ty == TY_CHAR) {
            x64_lea_data(RAX, zero_char_offset);    /* char の初期値は U+0000（空文字列ではない） */
        } else {
            x64_mov_imm(RAX, 0);                    /* 数値は 0、bool は false */
        }
        x64_store(RBP, -node->var->offset, RAX);   /* ref の変数なら、使う場所を入れる */
        return;
    case ND_ASSIGN:
        gen_expr(node->lhs);
        if (type_is_model(node->var->ty)) x64_call(type_model(node->var->ty)->copy_label);
        if (type_is_array(node->var->ty)) {
            push(); gen_load_var(node->var, node->tok); x64_load(RDI, RAX, 16); pop(RAX);
            collection_context(node->tok); x64_xor(RSI, RSI); x64_call(array_copy_label);
        }
        gen_store_var(node->var, node->tok);
        return;
    case ND_STORE:
        gen_index_store(node);
        return;
    case ND_ADDR_ASSIGN:
        gen_expr(node->lhs);
        if (type_is_array(node->var->ty)) {
            collection_context(node->tok); collection_shape(node->var->ty, node->var->dims, RDI);
            x64_call(array_ref_label);
        }
        x64_store(RBP, -node->var->offset, RAX);   /* 指す先を変える */
        return;
    case ND_RETURN:
        if (node->lhs != NULL) {
            gen_expr(node->lhs);
        } else {
            x64_mov_imm(RAX, 0);
        }
        x64_jmp(return_label);
        return;
    case ND_EXPR_STMT:
        gen_expr(node->lhs);
        return;
    case ND_USING:
        return;                 /* 名前の省略だけで、実行時の処理はない */
    case ND_PRINT:
        gen_print(node);
        return;
    default:
        fatal("内部エラー: 未対応の文です");
    }
}

static void gen_function(Function *fn)
{
    char label_name[128];
    emitting_function = fn;
    depth = 0;

    snprintf(label_name, sizeof(label_name), ".L.return.%d", fn->label);
    return_label = x64_new_label(label_name);

    x64_bind(fn->label);

    /* プロローグ：ローカル変数の領域を確保する */
    x64_push(RBP);
    x64_mov(RBP, RSP);
    if (fn->stack_size > 0) {
        x64_sub_imm(RSP, fn->stack_size);
    }

    gen_stmts(fn->body);

    /* return がなかったとき（main と void の関数）は 0 を返す */
    x64_mov_imm(RAX, 0);

    /* エピローグ */
    x64_bind(return_label);
    x64_mov(RSP, RBP);
    x64_pop(RBP);
    x64_ret();
}

/* -S 用：データ領域をアセンブリとして出力する */
static void dump_data(void)
{
    size_t i;

    if (text == NULL) {
        return;
    }
    if (target == TARGET_LINUX) {
        x64_directive(".section .rodata");
    } else {
        x64_directive(".section .rdata,\"dr\"");
    }
    fprintf(text, ".L.data:\n");
    for (i = 0; i < prog->data.len; i++) {
        if (i % 16 == 0) {
            fprintf(text, "    .byte ");
        } else {
            fprintf(text, ", ");
        }
        fprintf(text, "%u", (unsigned)prog->data.data[i]);
        if (i % 16 == 15 || i + 1 == prog->data.len) {
            fprintf(text, "\n");
        }
    }
    if (target == TARGET_LINUX) {
        x64_directive(".section .note.GNU-stack,\"\",@progbits");
    }
}

void codegen(Ast *ast, Program *out_prog, FILE *out_text, Target out_target)
{
    char label_name[128];
    Function *fn;
    int function_index = 0;

    prog = out_prog;
    text = out_text;
    target = out_target;
    size_codes=size_codes_tail=NULL;
    depth = 0;
    label_seq = 0;
    loop_depth = 0;

    emitting_models = ast->models;
    x64_begin(prog, text);
    path_call_label=new_label("path.call");
    { Model *m;
      for (m = ast->models; m; m = m->next) {
          snprintf(label_name, sizeof(label_name), ".L.model.%d.copy", m->ty);
          m->copy_label = x64_new_label(label_name);
          snprintf(label_name, sizeof(label_name), ".L.model.%d.equal", m->ty);
          m->equal_label = x64_new_label(label_name);
          snprintf(label_name, sizeof(label_name), ".L.model.%d.print", m->ty);
          m->print_label = x64_new_label(label_name);
      }
    }
    newline_offset = prog_add_data(prog, "\n", 1);
    empty_str_offset = prog_add_str(prog, "", 0);
    zero_char_offset = prog_add_str(prog, "\0", 1);

    /* 関数のラベルは先に作っておく（後ろで定義された関数も呼べるように） */
    main_label = -1;
    for (fn = ast->funcs; fn != NULL; fn = fn->next) {
        if (fn->is_template) continue;
        snprintf(label_name, sizeof(label_name), ".L.module.%d.function.%d.%.*s", fn->tok->source->module->id, function_index++, fn->name_len, fn->name);
        fn->label = x64_new_label(label_name);
        if (!fn->model && fn->name_len == 4 && strncmp(fn->name, "main", 4) == 0) {
            main_label = fn->label;
        }
    }
    { int mode, op;
      for (mode = 0; mode < 2; mode++) {
          complex_make_labels[mode] = new_label("complex.make"); complex_pow_labels[mode] = new_label("complex.pow");
          complex_sqrt_labels[mode] = new_label("complex.sqrt");
          complex_print_labels[mode] = new_label("complex.print"); complex_parse_labels[mode] = new_label("complex.parse");
          for (op = 0; op < 4; op++) complex_op_labels[mode][op] = new_label("complex.binary");
      }
      complex_equal_label = new_label("complex.equal"); complex_abs_label = new_label("complex.abs"); complex_real_label = new_label("complex.real.parse");
    }
    wide_call_label=new_label("wide.call");wide_engine_label=new_label("wide.engine");
    write_label = x64_new_label(".L.qy_write");
    print_uint_label = x64_new_label(".L.qy_print_uint");
    integer_format_label = x64_new_label(".L.qy_integer_format");
    print_int_label = x64_new_label(".L.qy_print_int");
    print_double_label = x64_new_label(".L.qy_print_double");
    print_float_label = x64_new_label(".L.qy_print_float");
    print_bool_label = x64_new_label(".L.qy_print_bool");
    print_str_label = x64_new_label(".L.qy_print_str");
    print_addr_label = x64_new_label(".L.qy_print_addr");
    alloc_label = x64_new_label(".L.qy_alloc");
    raw_alloc_label = x64_new_label(".L.qy_raw_alloc");
    raw_free_label = x64_new_label(".L.qy_raw_free");
    gc_mark_label = x64_new_label(".L.qy_gc_mark");
    gc_collect_label = x64_new_label(".L.qy_gc_collect");
    array_copy_label = x64_new_label(".L.qy_array_copy");
    array_slot_label = x64_new_label(".L.qy_array_slot");
    array_get_label = x64_new_label(".L.qy_array_get");
    array_add_label = x64_new_label(".L.qy_array_add");
    array_remove_label = x64_new_label(".L.qy_array_remove");
    array_clear_label = x64_new_label(".L.qy_array_clear");
    array_equal_label = x64_new_label(".L.qy_array_equal");
    array_print_label = x64_new_label(".L.qy_array_print");
    quoted_string_label = x64_new_label(".L.qy_quoted_string");
    array_ref_label = x64_new_label(".L.qy_array_ref");
    string_get_label = x64_new_label(".L.qy_string_get");
    string_replace_label = x64_new_label(".L.qy_string_replace");
    concat_label = x64_new_label(".L.qy_concat");
    str_eq_label = x64_new_label(".L.qy_str_eq");
    utf8_next_checked_label = x64_new_label(".L.qy_utf8_next_checked");
    string_scan_label = x64_new_label(".L.qy_string_scan");
    pow_int_label = x64_new_label(".L.qy_pow_int");
    pow_double_label = x64_new_label(".L.qy_pow_double");
    pow_float_label = x64_new_label(".L.qy_pow_float");
    write_err_label = x64_new_label(".L.qy_write_err");
    runtime_error_label = x64_new_label(".L.qy_runtime_error");
    exit_label = x64_new_label(".L.qy_exit");
    round_labels[0] = x64_new_label(".L.qy_round");
    round_labels[1] = x64_new_label(".L.qy_roundup");
    round_labels[2] = x64_new_label(".L.qy_rounddown");
    round_integer_labels[0][0] = x64_new_label(".L.qy_round_integer.round");
    round_integer_labels[1][0] = x64_new_label(".L.qy_round_unsigned.round");
    round_integer_labels[0][1] = x64_new_label(".L.qy_round_integer.roundup");
    round_integer_labels[1][1] = x64_new_label(".L.qy_round_unsigned.roundup");
    round_integer_labels[0][2] = x64_new_label(".L.qy_round_integer.rounddown");
    round_integer_labels[1][2] = x64_new_label(".L.qy_round_unsigned.rounddown");
    {
        int b;
        for (b = BI_FILE_OPEN; b < BI_COUNT; b++) {
            snprintf(label_name, sizeof(label_name), ".L.qy_file_%s", builtin_name((Builtin)b));
            file_labels[b - BI_FILE_OPEN] = x64_new_label(label_name);
        }
    }
    file_write_all_label = x64_new_label(".L.qy_file_write_all");
    file_error_label = x64_new_label(".L.qy_file_error");
    file_validate_label = x64_new_label(".L.qy_file_validate");
    file_path_label = x64_new_label(".L.qy_file_path");
    file_raw_close_label = x64_new_label(".L.qy_file_raw_close");
    file_finalize_label = x64_new_label(".L.qy_file_finalize");
    file_cleanup_label = x64_new_label(".L.qy_file_cleanup");
    file_kind_label = x64_new_label(".L.qy_file_kind");
    file_raw_open_label = x64_new_label(".L.qy_file_raw_open");
    file_check_label = x64_new_label(".L.qy_file_check");
    file_append_label = x64_new_label(".L.qy_file_append");
    file_string_label = x64_new_label(".L.qy_file_string");
    file_capture_label = x64_new_label(".L.qy_file_capture");
    file_getc_label = x64_new_label(".L.qy_file_getc");
    file_text_label = x64_new_label(".L.qy_file_text");
    input_labels();

    x64_directive(".intel_syntax noprefix");
    x64_directive(".text");
    x64_directive(".globl _start");

    /* エントリ処理 */
    prog->entry = prog->text.len;
    if (text != NULL) {
        fprintf(text, "_start:\n");
    }
    if (target == TARGET_WINDOWS) {
        gen_entry_windows();
    } else {
        gen_entry_linux();
    }

    /* 実行時ルーチン */
    gen_write(write_label, 0);
    gen_write(write_err_label, 1);
    gen_runtime_error();
    gen_print_int();
    gen_print_uint();
    gen_print_double();
    gen_print_bool();
    gen_print_str();
    gen_print_addr();
    gen_raw_memory();
    gen_gc_mark();
    gen_gc_collect();
    gen_alloc();
    gen_concat();
    gen_str_eq();
    gen_utf8_next_checked();
    gen_string_scan();
    gen_array_access_runtime();
    gen_array_copy_runtime();
    gen_array_add_runtime();
    gen_array_delete_runtime();
    gen_array_equal_runtime();
    gen_array_ref_runtime();
    gen_string_get_runtime();
    gen_string_replace_runtime();
    gen_quoted_string_runtime();
    gen_array_print_runtime();
    gen_pow_int();
    gen_pow_real(0);
    gen_pow_real(1);
    gen_round(BI_ROUND);
    gen_round(BI_ROUNDUP);
    gen_round(BI_ROUNDDOWN);
    gen_round_integer(BI_ROUND, 0);
    gen_round_integer(BI_ROUND, 1);
    gen_round_integer(BI_ROUNDUP, 0);
    gen_round_integer(BI_ROUNDUP, 1);
    gen_round_integer(BI_ROUNDDOWN, 0);
    gen_round_integer(BI_ROUNDDOWN, 1);
    gen_wide_runtime();
    gen_complex_runtime();
    gen_input_routines();
    gen_file_runtime();
    gen_model_runtime();

    /* ユーザーのプログラム */
    for (fn = ast->funcs; fn != NULL; fn = fn->next) {
        if (!fn->is_template) gen_function(fn);
    }

    gen_library_size_runtime();
    gen_library_path_runtime();
    dump_data();
    x64_end();
}
