#include "qyc.h"
#include "optimizer.h"

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
    FILE *out=status?stderr:stdout;
    if(diagnostic_japanese){
        fprintf(out,"使い方: qyc <入力ファイル.qy> [オプション]\n\n");
        fprintf(out,"  -o <ファイル>           出力ファイル名を指定する\n");
        fprintf(out,"  -S                      アセンブリだけを出力する\n");
        fprintf(out,"  -I <ディレクトリ>       import の探索ルートを追加する（複数指定可）\n");
        fprintf(out,"  --target windows|linux  出力先OSを指定する（既定は実行中のOS）\n");
        fprintf(out,"  --jp, --japanese        日本語の表示を使う（大文字も使用可）\n");
        fprintf(out,"  --color=auto|always|never  コンパイラ診断の色を指定する\n");
        fprintf(out,"  --diagnostics=plain     ツール向けの位置情報形式を使う\n");
        fprintf(out,"  --quiet                 完了メッセージを表示しない\n");
        fprintf(out,"  -h, --help              使い方を表示する\n");
        fprintf(out,"  --version               バージョンを表示する\n\n");
        fprintf(out,"出力名を省くと、Windowsは入力名.exe、Linuxは拡張子なしとなる。\n");
    }else{
        fprintf(out,"Usage: qyc <input.qy> [options]\n\n");
        fprintf(out,"  -o <file>               Set the output filename\n");
        fprintf(out,"  -S                      Emit assembly instead of an executable\n");
        fprintf(out,"  -I <directory>          Add an import search root (repeatable)\n");
        fprintf(out,"  --target windows|linux  Set the target OS (default: host OS)\n");
        fprintf(out,"  --jp, --japanese        Use Japanese messages (case-insensitive)\n");
        fprintf(out,"  --color=auto|always|never  Control compiler diagnostic colors\n");
        fprintf(out,"  --diagnostics=plain     Use source locations suitable for tools\n");
        fprintf(out,"  --quiet                 Suppress the completion message\n");
        fprintf(out,"  -h, --help              Show this help\n");
        fprintf(out,"  --version               Show the version\n\n");
        fprintf(out,"Default output: input.exe for Windows, input without .qy for Linux.\n");
    }
    exit(status);
}
static int japanese_option(const char *s)
{
    char lower[16];size_t i,n=strlen(s);if(n>=sizeof(lower))return 0;
    for(i=0;i<=n;i++)lower[i]=(s[i]>='A'&&s[i]<='Z')?(char)(s[i]+'a'-'A'):s[i];
    return !strcmp(lower,"--jp")||!strcmp(lower,"--japanese");
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
        fatal(QMSG_0437);
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
        fatal(QMSG_0437);
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
    fatal(QMSG_0069, name);
    return TARGET_WINDOWS;
}

static void write_assembly(Ast *ast, const char *path, Target target)
{
    FILE *out;
    Program prog;

    out = fopen(path, "w");
    if (out == NULL) {
        fatal(QMSG_0027, path);
    }
    prog_init(&prog);
    codegen(ast, &prog, out, target);
    prog_free(&prog);
    if (fclose(out) != 0) {
        fatal(QMSG_0038, path);
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

    /* Language selection precedes every diagnostic, including malformed CLI
     * options. Values of path/target options and operands after -- are skipped. */
    {int jp=0,j;for(j=1;j<argc;j++){
        if(!strcmp(argv[j],"--"))break;
        if(japanese_option(argv[j]))jp=1;
        if(!strcmp(argv[j],"-o")||!strcmp(argv[j],"-I")||!strcmp(argv[j],"--target"))j++;
    }messages_select(jp);}
    diagnostic_plain=getenv("QYNO_DIAGNOSTICS")&&!strcmp(getenv("QYNO_DIAGNOSTICS"),"plain");
    diagnostic_quiet=getenv("QYNO_QUIET")!=NULL;
    setup_console();
    roots = calloc((size_t)argc, sizeof(*roots));
    if (roots == NULL) fatal(QMSG_0643);

    input_path = NULL;
    output_path = NULL;
    asm_only = 0;
    target = default_target();

    for (i = 1; i < argc; i++) {
        if(japanese_option(argv[i]))continue;
        if(!strcmp(argv[i],"--quiet")){diagnostic_quiet=1;continue;}
        if(!strcmp(argv[i],"--diagnostics=plain")){diagnostic_plain=1;continue;}
        if(!strcmp(argv[i],"--diagnostics=human")){diagnostic_plain=0;continue;}
        if(!strcmp(argv[i],"--color=auto")){diagnostic_color(0);continue;}
        if(!strcmp(argv[i],"--color=always")){diagnostic_color(1);continue;}
        if(!strcmp(argv[i],"--color=never")){diagnostic_color(2);continue;}
        if(!strcmp(argv[i],"--")){
            if(input_path||i+2!=argc)usage(1);
            input_path=argv[++i];diagnostic_input(input_path);break;
        }
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(0);
        }
        if (strcmp(argv[i], "--version") == 0) {
            if(diagnostic_japanese){
                printf("QYNO バージョン %s（プロトタイプ）\n",QYC_VERSION);
                printf("本製品はプロトタイプです。明示・黙示を問わず、いかなる保証もありません。\n");
                printf("商品性および特定目的への適合性に関する黙示の保証も含みます。\n");return 0;
            }
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
            if (i + 1 >= argc) fatal(QMSG_0070);
            roots[root_count++] = argv[++i];
            continue;
        }
        if (strncmp(argv[i], "-I", 2) == 0 && argv[i][2] != '\0') {
            roots[root_count++] = argv[i] + 2;
            continue;
        }
        if (argv[i][0] == '-') {
            fatal(QMSG_0484, argv[i]);
        }
        if (input_path != NULL) {
            fatal(QMSG_0528);
        }
        input_path = argv[i];diagnostic_input(input_path);
    }

    if (input_path == NULL) {
        usage(1);
    }
    if (!has_qy_extension(input_path)) {
        fatal("%s",diagnostic_japanese?"ソースファイルの拡張子は\".qy\"にしてください":"Source file extension must be \".qy\"");
    }

    /* 1–3. 全モジュールを読み込み、各ソース位置を保持して解析する。 */
    diagnostic_phase(1);
    ast = load_program(input_path, roots, root_count);
    free(roots);

    /* 4. 型検査 */
    diagnostic_phase(2);
    typecheck(ast, target);
    diagnostic_phase(0);
    optimize_program(ast);

    /* -S：アセンブリだけを出力して終わる */
    if (asm_only) {
        if (output_path != NULL) {
            out_path = copy_string(output_path);
        } else {
            out_path = replace_extension(input_path, ".s");
        }
        write_assembly(ast, out_path, target);
        free(out_path);
        diagnostic_success();return 0;
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
    diagnostic_success();return 0;
}
