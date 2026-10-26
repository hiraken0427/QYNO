#include "qyc.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

SourceFile *current_file;

void select_source(SourceFile *source)
{
    current_file = source;
}

/* 位置情報なしのエラー */
void fatal(const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "qyc: エラー: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

/* ソースの該当行と ^ で位置を示してエラー終了する */
static void source_error(SourceFile *source, int line, int col, const char *fmt, va_list ap)
{
    const char *line_start;
    const char *line_end;
    int cur_line;
    int i;

    line_start = source->input;
    cur_line = 1;
    while (cur_line < line && *line_start != '\0') {
        if (*line_start == '\n') {
            cur_line++;
        }
        line_start++;
    }
    line_end = line_start;
    while (*line_end != '\0' && *line_end != '\n' && *line_end != '\r') {
        line_end++;
    }

    fprintf(stderr, "%s:%d:%d: エラー: ", source->filename, line, col);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");

    fprintf(stderr, "%.*s\n", (int)(line_end - line_start), line_start);
    for (i = 0; i < col - 1 && line_start + i < line_end; i++) {
        fputc(line_start[i] == '\t' ? '\t' : ' ', stderr);
    }
    fprintf(stderr, "^\n");
    exit(1);
}

void error_at(int line, int col, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    source_error(current_file, line, col, fmt, ap);
    va_end(ap);
}

void error_tok(Token *tok, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    source_error(tok->source, tok->line, tok->col, fmt, ap);
    va_end(ap);
}

#ifdef _WIN32
static UINT original_output_cp;

/* 終了時にコンソールの文字コードを元に戻す */
static void restore_console_cp(void)
{
    SetConsoleOutputCP(original_output_cp);
}

/* メッセージは UTF-8 で出力するため、コンソールを UTF-8 に切り替える */
static void setup_console(void)
{
    original_output_cp = GetConsoleOutputCP();
    if (original_output_cp != 0 && original_output_cp != CP_UTF8) {
        SetConsoleOutputCP(CP_UTF8);
        atexit(restore_console_cp);
    }
}
#else
static void setup_console(void)
{
}
#endif

static void usage(int status)
{
    fprintf(stderr, "使い方: qyc <入力ファイル.qy> [-o <出力ファイル>] [-S] [-I <探索ルート>] [--target windows|linux]\n");
    fprintf(stderr, "  -o <ファイル>        出力ファイル名を指定する\n");
    fprintf(stderr, "                       （既定は Windows 向けなら 入力名.exe、Linux 向けなら拡張子なし）\n");
    fprintf(stderr, "  -S                   実行ファイルを作らず、アセンブリ（.s）だけを出力する\n");
    fprintf(stderr, "  --target <OS>        出力先の OS（windows または linux）。既定は実行中の OS\n");
    fprintf(stderr, "  -I <ディレクトリ>    import の探索ルートを追加する（複数指定可）\n");
    fprintf(stderr, "  --version            バージョンを表示する\n");
    exit(status);
}

static int has_qy_extension(const char *path)
{
    size_t len;

    len = strlen(path);
    return len > 3 && strcmp(path + len - 3, ".qy") == 0;
}

/* 末尾の ".qy" を ext に置き換えた文字列を作る */
static char *replace_extension(const char *path, const char *ext)
{
    size_t base_len;
    size_t ext_len;
    char *buf;

    base_len = strlen(path) - 3;
    ext_len = strlen(ext);
    buf = malloc(base_len + ext_len + 1);
    if (buf == NULL) {
        fatal("メモリを確保できません");
    }
    memcpy(buf, path, base_len);
    memcpy(buf + base_len, ext, ext_len + 1);
    return buf;
}

static char *copy_string(const char *str)
{
    size_t len;
    char *buf;

    len = strlen(str);
    buf = malloc(len + 1);
    if (buf == NULL) {
        fatal("メモリを確保できません");
    }
    memcpy(buf, str, len + 1);
    return buf;
}

static Target default_target(void)
{
#ifdef _WIN32
    return TARGET_WINDOWS;
#else
    return TARGET_LINUX;
#endif
}

static Target parse_target(const char *name)
{
    if (strcmp(name, "windows") == 0) {
        return TARGET_WINDOWS;
    }
    if (strcmp(name, "linux") == 0) {
        return TARGET_LINUX;
    }
    fatal("--target には windows か linux を指定してください: %s", name);
    return TARGET_WINDOWS;
}

static void write_assembly(Ast *ast, const char *path, Target target)
{
    FILE *out;
    Program prog;

    out = fopen(path, "w");
    if (out == NULL) {
        fatal("%s に書き込めません", path);
    }
    prog_init(&prog);
    codegen(ast, &prog, out, target);
    prog_free(&prog);
    if (fclose(out) != 0) {
        fatal("%s の書き込みに失敗しました", path);
    }
}

int main(int argc, char **argv)
{
    const char *input_path;
    const char *output_path;
    char *out_path;
    Ast *ast;
    Program prog;
    Target target;
    int asm_only;
    int i;
    const char **roots;
    int root_count = 0;

    setup_console();
    roots = calloc((size_t)argc, sizeof(*roots));
    if (roots == NULL) fatal("探索ルートの領域を確保できません");

    input_path = NULL;
    output_path = NULL;
    asm_only = 0;
    target = default_target();

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(0);
        }
        if (strcmp(argv[i], "--version") == 0) {
            printf("QYNO version %s (PROTOTYPE)\n", QYC_VERSION);
            printf("Please note that this product is a prototype. No warranties of any kind exist,\n");
            printf("including implied warranties of merchantability or fitness for a particular purpose.\n");
            return 0;
        }
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                usage(1);
            }
            i++;
            output_path = argv[i];
            continue;
        }
        if (strcmp(argv[i], "-S") == 0) {
            asm_only = 1;
            continue;
        }
        if (strcmp(argv[i], "--target") == 0) {
            if (i + 1 >= argc) {
                usage(1);
            }
            i++;
            target = parse_target(argv[i]);
            continue;
        }
        if (strcmp(argv[i], "-I") == 0) {
            if (i + 1 >= argc) fatal("-I の後に探索ルートが必要です");
            roots[root_count++] = argv[++i];
            continue;
        }
        if (strncmp(argv[i], "-I", 2) == 0 && argv[i][2] != '\0') {
            roots[root_count++] = argv[i] + 2;
            continue;
        }
        if (argv[i][0] == '-') {
            fatal("不明なオプションです: %s", argv[i]);
        }
        if (input_path != NULL) {
            fatal("入力ファイルは1つだけ指定してください");
        }
        input_path = argv[i];
    }

    if (input_path == NULL) {
        usage(1);
    }
    if (!has_qy_extension(input_path)) {
        fatal("ソースファイルの拡張子は .qy にしてください: %s", input_path);
    }

    /* 1–3. 全モジュールを読み込み、各ソース位置を保持して解析する。 */
    ast = load_program(input_path, roots, root_count);
    free(roots);

    /* 4. 型検査 */
    typecheck(ast);

    /* -S：アセンブリだけを出力して終わる */
    if (asm_only) {
        if (output_path != NULL) {
            out_path = copy_string(output_path);
        } else {
            out_path = replace_extension(input_path, ".s");
        }
        write_assembly(ast, out_path, target);
        free(out_path);
        return 0;
    }

    /* 5. 機械語を生成する */
    prog_init(&prog);
    codegen(ast, &prog, NULL, target);

    /* 6. 実行ファイルを書き出す */
    if (output_path != NULL) {
        out_path = copy_string(output_path);
    } else if (target == TARGET_WINDOWS) {
        out_path = replace_extension(input_path, ".exe");
    } else {
        out_path = replace_extension(input_path, "");
    }
    if (target == TARGET_WINDOWS) {
        write_pe(out_path, &prog);
    } else {
        write_elf(out_path, &prog);
    }

    free(out_path);
    prog_free(&prog);
    return 0;
}
