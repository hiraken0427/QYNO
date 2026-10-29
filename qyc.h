#ifndef QYC_H
#define QYC_H

#include <stddef.h>
#include <stdint.h>
#include "wide_runtime.h"
#include "time_runtime.h"
#include "grapheme_runtime.h"
#include <stdio.h>

#define QYC_VERSION "0.27.4"

typedef struct SourceFile SourceFile;
typedef struct Module Module;
typedef struct ModuleImport ModuleImport;
typedef struct Ast Ast;
typedef struct Model Model;
typedef struct EnumDef EnumDef;
typedef struct EnumMember EnumMember;
typedef struct ModelField ModelField;
typedef struct Interface Interface;
typedef struct Implementation Implementation;

struct SourceFile {
    const char *filename;
    char *canonical_path;
    char *text;
    const char *input;  /* UTF-8 BOM の後 */
    Module *module;
};

/* ---------- 出力先 ---------- */

typedef enum {
    TARGET_WINDOWS, /* Windows x64（PE32+ 形式の .exe） */
    TARGET_LINUX    /* Linux x64（ELF64 形式） */
} Target;

/* ---------- 可変長バイト列 ---------- */

typedef struct {
    unsigned char *data;
    size_t len;
    size_t cap;
} Buffer;

/* ---------- 生成するプログラム ---------- */

/* Windows で呼び出す kernel32.dll の関数（pe.c の名前表と順番を合わせる） */
typedef enum {
    IMP_EXIT_PROCESS,
    IMP_GET_STD_HANDLE,
    IMP_WRITE_FILE,
    IMP_GET_CONSOLE_OUTPUT_CP,
    IMP_SET_CONSOLE_OUTPUT_CP,
    IMP_GET_PROCESS_HEAP,
    IMP_HEAP_ALLOC,
    IMP_READ_FILE,
    IMP_GET_CONSOLE_MODE,
    IMP_READ_CONSOLE_W,
    IMP_READ_CONSOLE_INPUT_W,
    IMP_SET_CONSOLE_MODE,
    IMP_GET_CONSOLE_SCREEN_BUFFER_INFO,
    IMP_SET_CONSOLE_CURSOR_POSITION,
    IMP_HEAP_FREE,
    IMP_CREATE_FILE_W,
    IMP_CLOSE_HANDLE,
    IMP_GET_LAST_ERROR,
    IMP_GET_FILE_TYPE,
    IMP_GET_FILE_INFORMATION_BY_HANDLE,
    IMP_MULTI_BYTE_TO_WIDE_CHAR,
    IMP_SET_FILE_POINTER_EX, IMP_GET_SYSTEM_TIME_PRECISE_AS_FILE_TIME,
    IMP_QUERY_PERFORMANCE_COUNTER, IMP_QUERY_PERFORMANCE_FREQUENCY,
    IMP_GET_CURRENT_PROCESS, IMP_GET_PROCESS_TIMES, IMP_SLEEP,
    IMP_GET_CURRENT_DIRECTORY_W, IMP_SET_CURRENT_DIRECTORY_W, IMP_WIDE_CHAR_TO_MULTI_BYTE,
    IMP_COUNT
} Import;

/* 実行ファイルの書き出し時に埋める rip 相対アドレス */
typedef enum {
    RELOC_DATA,     /* data 内の位置を指す */
    RELOC_IMPORT    /* インポートアドレステーブルの項目を指す */
} RelocKind;

typedef struct {
    RelocKind kind;
    size_t pos;         /* text 内の disp32 の位置 */
    size_t target;      /* RELOC_DATA：data 内のオフセット、RELOC_IMPORT：Import の番号 */
} Reloc;

typedef struct {
    Buffer text;        /* 機械語 */
    Buffer data;        /* 読み取り専用データ（文字列） */
    Reloc *relocs;
    int reloc_count;
    int reloc_cap;
    size_t entry;       /* text 内のエントリポイントの位置 */
} Program;

/* ---------- 字句解析 ---------- */

typedef enum {
    TK_NUM,     /* 整数リテラル */
    TK_IMAG,    /* i4.0 / 4.0i */
    TK_FNUM,    /* 小数リテラル */
    TK_IDENT,   /* 識別子・キーワード */
    TK_PUNCT,   /* 記号 */
    TK_STR,     /* 文字列リテラル（loc・len は引用符を含む） */
    TK_NEWLINE, /* 文の区切り（改行） */
    TK_INDENT,  /* インデントが1段深くなった */
    TK_DEDENT,  /* インデントが1段浅くなった */
    TK_EOF      /* 入力の終わり */
} TokenKind;

typedef struct Token Token;
struct Token {
    TokenKind kind;
    Token *next;
    long long val;      /* TK_NUM のときの値 */
    int unsigned_literal; /* decimal magnitude > INT64_MAX */
    int decimal_min;    /* magnitude 2^63; signed-minimum spelling after unary minus */
    WideBits wide;
    int wide_ty;
    double fval;        /* TK_FNUM のときの値 */
    const char *loc;    /* ソース上の位置 */
    int len;            /* トークンの長さ */
    int line;           /* 行番号（1始まり） */
    int col;            /* 列番号（1始まり、バイト単位） */
    SourceFile *source;
};

/* ---------- 型 ---------- */

typedef enum {
    TY_NONE,    /* 型なし（文など） */
    TY_I64,     /* explicit signed 64bit */
    TY_I8, TY_I16, TY_I32,
    TY_U8, TY_U16, TY_U32, TY_U64,
    TY_I128, TY_U128,
    TY_HALF, TY_QUAD,
    TY_FLOAT,   /* 32ビット浮動小数点数（C の float） */
    TY_DOUBLE,  /* 64ビット浮動小数点数（C の double） */
    TY_C64, TY_C128, /* immutable complex values, float/double components */
    TY_BOOL,    /* 真偽値（0 または 1） */
    TY_STR,     /* 文字列（[長さ 8バイト][本体] の先頭を指すポインタ） */
    TY_CHAR,    /* Unicode スカラー値をちょうど1つ含む文字列。表現は str と同じ */
    TY_FILE,    /* 共有されるファイル管理領域。0 は未オープン */
    TY_VOID,    /* unique value (), logical width zero; scalar ABI slot retained */
    TY_ADDR,    /* 変数の場所（x.addr・none。指す先の型は cmp_ty） */
    TY_ARRAY    /* この値以降は types.c で管理する配列・モデル・interface・未解決の名前付き型のID */
} Type;

#define TY_INT TY_I32  /* int and int_32 are the same nominal type */

#define MAX_ARRAY_RANK 32
#define MAX_PLACE_DEPTH 128
#define MAX_CONST_DEPTH 256
#define MAX_FUNCTION_DEPTH 256
typedef struct ArrayDim ArrayDim;

/* ---------- 構文解析（AST） ---------- */

typedef struct Var Var;
typedef struct Node Node;
typedef struct Function Function;
typedef struct InputField InputField;
typedef struct Using Using;

struct ArrayDim {
    ArrayDim *next;
    Node *bound;           /* NULL は [] */
    long long limit;       /* 型検査後。-1 は上限なし */
    int checked;
    Token *tok;
};

/* 組み込み関数 */
typedef enum {
    BI_SQRT,        /* math.sqrt(x) */
    BI_ABS,         /* math.abs(x)（|x| を推奨） */
    BI_ROUND,       /* math.round(x, 桁数?)：絶対値で四捨五入して符号を付ける */
    BI_ROUNDUP,     /* math.roundup(x, 桁数?)：天井関数 */
    BI_ROUNDDOWN,   /* math.rounddown(x, 桁数?)：床関数 */
    BI_INT,         /* int(x) */
    BI_FLOAT,       /* float(x) */
    BI_DOUBLE,      /* double(x) */
    BI_CHAR,        /* char(x)：str/char を1スカラー値に制限する */
    BI_STR,         /* str(x)：char/str を str にする */
    BI_COMPLEX, BI_HALF, BI_QUAD,
    BI_IO_PRINT,    /* io.print(...)：出力文として解析する */
    BI_IO_INPUT,    /* io.input(...)：入力先と形式を保持する */
#define TIME_API(id,name,min,max,args,result) BI_TIME_##id,
#include "time_api.def"
#undef TIME_API
    BI_LIB_SIZE, BI_LIB_LENGTH,
    BI_PATH_JOIN, BI_PATH_BASENAME, BI_PATH_DIRNAME, BI_PATH_EXTENSION,
    BI_PATH_STEM, BI_PATH_IS_ABSOLUTE, BI_PATH_IS_RELATIVE,
    BI_PATH_NORMALIZE, BI_PATH_WITH_EXTENSION,
    BI_DIR_CURRENT, BI_DIR_CHANGE, BI_PATH_ABSOLUTE,
    BI_FILE_OPEN,
    BI_FILE_CLOSE,
    BI_FILE_IS_OPEN,
    BI_FILE_READ,
    BI_FILE_READLINE,
    BI_FILE_WRITE,
    BI_FILE_EXISTS,
    BI_FILE_READ_BYTES, BI_FILE_WRITE_BYTES, BI_FILE_TRY_OPEN,
    BI_FILE_TELL, BI_FILE_SEEK, BI_FILE_SIZE, BI_FILE_MODIFIED_AT,
    BI_COUNT
} Builtin;

enum { TR_QUAD,TR_VOID,TR_BOOL,TR_INT,TR_DATE,TR_WEEKDAY,TR_TEXT,TR_TICKS };
typedef struct {int min_args,max_args;const char *args;int result;} TimeSpec;
const TimeSpec *time_spec(int op);
int builtin_is_time(int b);
int builtin_is_directory(int b);
int builtin_may_raise(int b);
Type standard_type(Ast *ast,const char *ns,const char *name);

/* using の1項目。参照先と宣言位置の字句スコープを保持する。 */
struct Using {
    Using *next;
    Using *vnext;       /* 関数の中の using の見える範囲 */
    Token *path_tok;    /* 参照先の最初の Token */
    Token *tok;         /* 書いた位置 */
    char *path;
    Token *alias;
    Using *scope;       /* 宣言前のブロック内 using */
    Using *all_next;
    Var *locals;        /* 宣言位置で見える変数 */
    EnumDef *target_enum;
    Model *target_model;
    Interface *target_interface;
    Module *target_module;
    Function *target_function;
    Var *target_constant;
    int target_builtin;
    int state;         /* 0:未解決、1:解決中、2:解決済み */
};
struct Var {
    EnumMember *enum_member;
    Var *next;          /* 関数内の全変数の並び */
    Var *vnext;         /* 見えている変数の並び（スコープ） */
    const char *name;
    int name_len;
    Type ty;
    ArrayDim *dims;
    int is_inferred;    /* auto 等の推論付き宣言。初期値の検査後に ty が確定する */
    int is_implicit;    /* 型名のない name = 初期値 による宣言 */
    int is_used;        /* 読み取り・場所の参照・複合更新がある */
    int offset;         /* rbp からの距離（[rbp - offset] に置く）。const は 0 */
    int is_const;       /* const 定数なら 1 */
    int is_param;       /* 関数の引数なら 1（[rbp + 16 + ...] に置かれる） */
    int is_readonly;    /* read-only borrow; independent of nominal type */
    int is_ref;         /* ref の変数なら 1（置き場には値ではなく、使う場所が入る） */
    int is_loop;        /* for 文の繰り返しの変数なら 1 */
    int depth;          /* 宣言したブロックの深さ（関数の引数は 0） */
    int is_global;      /* ファイル先頭の const なら 1 */
    int is_private;     /* module-private top-level constant */
    Node *default_arg;  /* initializer-only default recipe */
    int parameter_index;
    Node *const_val;    /* const の値（コンパイル時に計算したリテラルのノード） */
    Node *decl;         /* const の宣言（ND_DECL）。値を必要になった時点で計算する */
    int const_state;    /* const の値の計算状況（0：未、1：計算中、2：済み） */
    Token *tok;         /* 宣言した位置 */
    Using *usings;
};

typedef enum {
    ND_ENUM_CONVERT, ND_MATCH, ND_CASE, ND_TRY, ND_THROW,
    ND_UNIT,
    ND_WIDE,
    ND_ADD,     /* + （str 同士なら連結） */
    ND_SUB,     /* - */
    ND_MUL,     /* * */
    ND_DIV,     /* / */
    ND_MOD,     /* %（int 同士の余り） */
    ND_POW,     /* ^（累乗。lhs が底、rhs が int の指数） */
    ND_NEG,     /* 単項 - */
    ND_CMP,     /* 比較1つ（ND_CHAIN の中だけに現れる。val に比較の種類） */
    ND_CHAIN,   /* 比較（連鎖を含む）。lhs から next でつながる ND_CMP の並び */
    ND_AND,     /* and / && */
    ND_OR,      /* or / || */
    ND_NOT,     /* not / ! */
    ND_XOR, ND_BIT_AND, ND_BIT_OR, ND_BIT_XOR, ND_BIT_NOT, ND_SHL, ND_SHR,
    ND_INT_PARSE, ND_INT_FORMAT,
    ND_NUM,     /* 整数リテラル */
    ND_COMPLEX, ND_CREAL, ND_CIMAG,
    ND_FNUM,    /* 小数リテラル */
    ND_BOOL,    /* true / false */
    ND_STRLIT,  /* 文字列リテラル（式の中。str・str_len） */
    ND_INDEX,   /* 添字読み出し（lhs は配列・str・char、rhs は添字） */
    ND_LENGTH,  /* lhs.length（配列の要素数、またはUnicodeスカラー値の数） */
    ND_MODEL_INIT, /* named contextual initializer; lhs items carry field names */
    ND_MODEL_COPY,
    ND_CONSTRUCT, ND_MODEL_SEED, ND_BUILD_CONTEXT, ND_BUILD_REJECT,
    ND_MODEL_NEW, /* default value via the per-model runtime helper */
    ND_MODEL_METHOD,
    ND_FIELD,
    ND_ARRAY_LITERAL, /* lhs が要素の並び。nargs は要素数 */
    ND_ARRAY_NEW,     /* 宣言に基づく初期配列 */
    ND_ARRAY_COPY,    /* lhs を dims の制約でコピー。val=1 は初期化の不足分を埋める */
    ND_METHOD,        /* lhs は更新先、rhs は引数。val=AM_* */
    ND_STORE,         /* lhs は添字の更新先、rhs は値。val=更新演算子 */
    ND_SAVED,         /* 複合添字更新の評価済みの値。val は rbp からの距離 */
    ND_VAR,     /* 変数の参照（var が NULL ならファイル先頭の const を型検査で探す） */
    ND_FORMAT,  /* write の補間文字列（lhs は print と同じ部分列） */
    ND_CALL,    /* 関数呼び出し（func、引数は lhs から next でつながる） */
    ND_BUILTIN, /* 組み込み関数（val に Builtin、引数は lhs から next でつながる） */
    ND_ABS,     /* |x|（lhs） */
    ND_INPUT,   /* input(...)：値の形（lhs がプロンプト）か、形に当てはめる形（fields） */
    ND_ADDR,    /* x.addr：変数の場所（lhs は ND_VAR） */
    ND_NONE,    /* none：何も指していない場所 */
    ND_REFARG,  /* 呼び出しの ref x（lhs は変数または配列要素） */
    ND_ADDR_ASSIGN, /* p.addr = 式：ref の変数の指す先を変える（var、lhs） */
    ND_CAST,    /* 型変換（型検査で挿入。lhs を ty に変換する） */
    ND_STR,     /* print の文字列の固定部分（str・str_len） */
    ND_DECL,    /* 変数宣言（var、lhs は初期値または NULL） */
    ND_ASSIGN,  /* 代入（var、lhs は値） */
    ND_RETURN,  /* return 文 */
    ND_IF,      /* if 文（cond、then、els） */
    ND_WHILE,   /* while 文（cond、then） */
    ND_FOR,     /* for 文（init、cond、step、then） */
    ND_EXPR_STMT, /* 式の文（関数呼び出しのみ。lhs） */
    ND_USING,   /* 関数の中の using（using_ref） */
    ND_BREAK,   /* break() */
    ND_CONTINUE,/* continue() */
    ND_PRINT    /* print 文（lhs から next でつながる部分の並び。end で終わりに出すものを指定） */
} NodeKind;

/* 比較の種類 */
typedef enum {
    CMP_EQ,     /* == */
    CMP_NE,     /* != */
    CMP_LT,     /* < */
    CMP_LE,     /* <= */
    CMP_GT,     /* > */
    CMP_GE      /* >= */
} CmpKind;

typedef enum { AM_ADD, AM_REMOVE_AT, AM_CLEAR } ArrayMethod;
typedef enum { UPDATE_SET, UPDATE_ADD, UPDATE_SUB, UPDATE_INC, UPDATE_DEC } UpdateKind;

struct Node {
    NodeKind kind;
    Node *next;         /* 次の文、または print の次の部分 */
    Node *lhs;          /* 左辺（単項演算・return・宣言・代入では式） */
    Node *rhs;          /* 右辺 */
    Type ty;            /* 式の型（型検査で決まる） */
    ArrayDim *dims;      /* 配列コピー・ref 引数の制約 */
    long long val;      /* ND_NUM・ND_BOOL の値。ND_PRINT では文字列リテラル直書きなら 1 */
    WideBits wide;
    int raw_numeric;
    double imag;       /* ND_COMPLEX imaginary component */
    double fval;        /* ND_FNUM の値 */
    char *str;          /* ND_STR・ND_STRLIT の内容（エスケープ処理済み） */
    int str_len;
    Var *var;           /* ND_VAR・ND_DECL・ND_ASSIGN の変数 */
    Type cmp_ty;        /* ND_CMP で比べるときの型 */
    Node *cond;         /* ND_IF の条件 */
    Node *then;         /* ND_IF の条件が真のときの文の並び */
    Node *els;          /* ND_IF の else の文の並び（else if は ND_IF 1つ） */
    Node *init;         /* ND_FOR のループ変数の宣言（ND_DECL） */
    Node *step;         /* ND_FOR の増分（ND_ASSIGN） */
    Node *inc;          /* ND_FOR の増分の量（i += 2 なら 2、i++ なら 1） */
    int inc_sign;       /* ND_FOR の増分の向き（+= と ++ は 1、-= と -- は -1） */
    int for_infer;      /* omitted counter type: integer start, at least signed 32bit */
    int for_open;       /* ND_FOR：開始側の境界を含まない */
    Node *for_amount;   /* ND_FOR：ループ変数の型へ変換した定数の増分 */
    Function *func;     /* ND_CALL の呼び出し先（型検査で決まる） */
    Using *using_ref;   /* ND_USING の宣言リスト */
    int arg_val;        /* 呼び出しの引数に val を付けたら 1 */
    int input_pattern;  /* ND_INPUT：{変数} を含む形に当てはめる形なら 1（結果は bool） */
    int input_stmt;     /* ND_INPUT：文として使う（失敗したら実行時エラーで止める）なら 1 */
    char *prompt;       /* ND_INPUT の形に当てはめる形：最初の { より前に表示する文字 */
    int prompt_len;
    InputField *fields; /* ND_INPUT の形に当てはめる形：{変数} の並び */
    char *suffix;       /* ND_INPUT の形に当てはめる形：最後の } より後（右の枠。00.23.1） */
    int suffix_len;
    Var *in_line;       /* ND_INPUT：読んだ行（隠れたローカル変数） */
    Var *in_cur;        /* ND_INPUT：行の中の読んでいる位置 */
    Var *in_end;        /* ND_INPUT：行の終わり */
    int format_base;    /* interpolation only: 0 = ordinary rendering */
    Token *arg_name;    /* conversion-only keyword argument */
    int option_index;   /* int/str conversion argument slot, evaluated in source order */
    int nargs;          /* ND_CALL の引数の数 */
    Node *end;          /* ND_PRINT の end= の部分の並び（has_end が 0 なら改行） */
    int has_end;        /* ND_PRINT で end= を指定したら 1 */
    int end_literal;    /* ND_PRINT の end= が文字列リテラル直書きなら 1 */
    Token *tok;         /* エラー表示用 */
    char *name;         /* 未解決の名前（修飾名を含む） */
    int explicit_value; /* user-written .value, not plain variable access */
    int init_store;    /* first whole-field assignment may target an empty slot */
    int constant_value; /* canonical evaluated aggregate, not a user initializer */
    Var *ref_temp;      /* const root materialized for a readonly ref call */
    ModelField *field;
    Model *access_model; /* lexical owner of user-written expressions; survives lowering */
    Using *usings;      /* 式の位置のブロック内 using */
};

struct Function {
    Function *next;
    Interface *contract; /* signature-only interface method */
    int is_template;
    Function *template_origin;
    int is_generic;     /* 型未確定の引数を持つ未検査テンプレート */
    Function *generic_origin;
    int generic_instances;
    Model *model;       /* implicit ref this receiver for model methods */
    int is_private;
    int is_readonly;
    int is_initializer;
    int is_internal;
    Var *creation_context;
    const char *name;
    int name_len;
    Token *tok;         /* 関数名の位置 */
    Type ret;           /* 戻り値の型（void は TY_VOID） */
    int infer_return;    /* auto / 戻り値型の省略 */
    int check_state;     /* 関数本体の検査（0:未、1:検査中、2:済み） */
    ArrayDim *return_dims;
    Var *params;        /* 引数（宣言順、next でつながる。locals とは別に並べる） */
    int param_count;
    int is_math_form;   /* double f(double x) = 式 の形で定義したら 1 */
    Node *body;         /* 文の連結リスト */
    Var *locals;        /* ローカル変数（const を含み、引数は含まない） */
    int io_effect;     /* inferred recoverable I/O effect */
    int io_caught;     /* reachable through a recovering handler */
    int stack_size;     /* ローカル変数の領域（16の倍数） */
    int label;          /* コード生成で使うラベル */
};

struct EnumMember {
    EnumMember *next; EnumDef *owner; Token *tok; char *name;
    Node *initial; Var *symbol; long long value; int state, index;
};
struct EnumDef {
    EnumDef *next; Token *tok; char *name; Type ty; int is_private, state, count;
    EnumMember *members; int check_label, name_label, parse_label;
};

struct ModelField {
    ModelField *next;
    Model *model;       /* declaring nominal model */
    int is_private;
    Token *tok;
    char *name;
    Type ty;
    ArrayDim *dims;
    Node *initial;
    int index;
    int pending;
    int initial_state; /* フィールド既定値の遅延検査（0:未、1:検査中、2:済み） */
};
struct Interface {
    Interface *next;
    int is_private;
    Token *tok;
    char *name;
    Type ty;
    Function *methods;
};
struct Implementation {
    Implementation *next;
    Type ty;
    Token *tok;
};
struct Model {
    Model *next;
    int is_private;
    Token *tok;
    char *name;
    Type ty;
    ModelField *fields;
    Implementation *implements;
    int field_count;
    int state;
    Function *initializer;
    Function *builder;
    Function *default_builder;
    int recipe_state;
    int dependency_state;
    int init_state;
    int file_state;    /* memoized contains_file: visiting / no / yes */
    int copy_label;
    int equal_label;
    int print_label;
    Node *default_value;
    Node *constant_default;
};

/* input の形の中の {変数} 1つ分 */
struct InputField {
    InputField *next;
    int base;           /* int input radix, 0 = detect prefix then decimal */
    Node *var;          /* 読み込む先の変数または添字（ND_VAR / ND_INDEX） */
    char *sep;          /* この {} の後ろの区切りの文字（最後の {} なら NULL） */
    int sep_len;
    Var *temp;          /* 読み取った値を一時的に置くローカル変数（すべて読めてから変数に移す） */
};

/* プログラム全体 */
struct Ast {
    EnumDef *enums;
    Model *models;
    Interface *interfaces;
    Function *funcs;    /* すべての関数（main を含む） */
    Node *globals;      /* ファイル先頭の const 宣言（ND_DECL の並び） */
    Var *global_vars;   /* ファイル先頭の const */
    Using *usings;      /* 関数の外の using */
    ModuleImport *imports;
    Using *all_usings;
    Module *modules;
    Module *entry;
};

struct ModuleImport {
    ModuleImport *next;
    char *name;
    Token *tok;
    Module *module;
};

struct Module {
    Module *next;
    char *name;
    SourceFile *source;
    Ast *ast;
    int state;          /* 1:読み込み中、2:完了 */
    int id;
    int builtin;
};

/* builtin.c：組み込み関数の一覧 */
const char *builtin_name(Builtin b);
const char *builtin_namespace(Builtin b);          /* 型変換は NULL */
int builtin_find(Token *ns, Token *name);           /* 見つからなければ -1 */
const char *builtin_namespace_of(const char *name, int len);  /* 標準モジュール名。見つからなければ NULL */

/* main.c */
extern SourceFile *current_file;
void select_source(SourceFile *source);
void error_at(int line, int col, const char *fmt, ...);
void error_tok(Token *tok, const char *fmt, ...);
void warning_tok(Token *tok, const char *fmt, ...);
void fatal(const char *fmt, ...);

/* modules.c：読み込み・モジュール単位の名前解決 */
Ast *load_program(const char *path, const char **roots, int root_count);
void validate_module_names(Ast *program);
void resolve_module_name(Node *node);
Type resolve_module_type(Token *tok, const char *name, Using *scope);

/* tokenize.c */
Token *tokenize(const char *src);
const char *interpolation_close(const char *open, const char *end);
long long integer_bits(uint64_t n);
int integer_digit(int c);
int integer_prefix(int c);
int integer_base_valid(long long base, int automatic);
int type_is_complex(Type ty);
Type complex_component(Type ty);
Type complex_common(Type a, Type b);
int type_is_integer(Type ty);
int type_is_real(Type ty);
int type_is_wide(Type ty);
int type_is_boxed(Type ty);
int wide_tag(Type ty);
Type real_common(Type a, Type b);
int integer_width(Type ty);
int integer_unsigned(Type ty);
Type integer_type(int bits, int unsign);
long long integer_normalize(long long value, Type ty);
int integer_fits(long long value, Type from, Type to);
Type integer_common(Type a, Type b);
int integer_read_typed(const char *s, size_t len, int base, Type ty, long long *value);
char *integer_format_typed(long long value, int base, int prefix, Type ty, int *length);
int integer_read(const char *s, size_t len, int base, long long *value);
char *integer_format(long long value, int base, int prefix, int *length);
Token *tokenize_fragment(const char *start, const char *end, int line, const char *line_start);

/* parse.c */
Ast *parse(Token *tok);

/* type.c：工程4 型検査（式の型を決め、必要な型変換を挿入する） */
const char *type_name(Type ty);
int type_is_text(Type ty);
int type_is_array(Type ty);
Type array_type(Type element);
Type array_element(Type array);
Type array_base(Type array);
int array_rank(Type array);
const char *array_type_name(Type array);
void typecheck(Ast *ast, Target target);
int value_bit_width(Type ty);
int type_is_enum(Type ty);
EnumDef *type_enum(Type ty);
Type enum_type(EnumDef *definition);
EnumMember *enum_find(EnumDef *definition, const char *name);
int module_name_exists(Node *node);
int type_is_model(Type ty);
Model *type_model(Type ty);
Type model_type(Model *model);
int type_is_interface(Type ty);
Interface *type_interface(Type ty);
Type interface_type(Interface *contract);
Type named_type(Token *tok, const char *name, Using *scope);
Type resolve_type(Type ty);
void resolve_model_types(Ast *ast);

/* unicode.c：境界を越えずに UTF-8 の1スカラー値を読む。正常なら1、不正なら0 */
int utf8_next(const unsigned char *s, size_t remaining, uint32_t *scalar, size_t *width);

/* buffer.c */
void buf_init(Buffer *b);
void buf_free(Buffer *b);
void buf_add(Buffer *b, const unsigned char *bytes, size_t n);
void buf_add_u8(Buffer *b, uint8_t v);
void buf_add_u32(Buffer *b, uint32_t v);
void buf_add_u64(Buffer *b, uint64_t v);
void buf_set_u32(Buffer *b, size_t pos, uint32_t v);

/* program.c */
void prog_init(Program *prog);
void prog_free(Program *prog);
size_t prog_add_data(Program *prog, const char *bytes, size_t n);
size_t prog_add_str(Program *prog, const char *bytes, size_t n);
void prog_add_reloc(Program *prog, RelocKind kind, size_t pos, size_t target);

/* x64.c：x64 命令を機械語に変換する（テキストのアセンブリも同時に出力できる） */
typedef enum {
    RAX, RCX, RDX, RBX, RSP, RBP, RSI, RDI,
    R8, R9, R10, R11, R12, R13, R14, R15
} Reg;

typedef enum {
    CC_NO = 0x1,    /* no signed overflow */
    CC_O = 0x0,     /* 符号付き演算のオーバーフロー */
    CC_B = 0x2,     /* 符号なしで小さい */
    CC_AE = 0x3,    /* 符号なしで以上 */
    CC_Z = 0x4,     /* 0 なら（等しい） */
    CC_NZ = 0x5,    /* 0 でなければ */
    CC_BE = 0x6,    /* 符号なしで以下 */
    CC_A = 0x7,     /* 符号なしで大きい */
    CC_S = 0x8,     /* 負なら */
    CC_NS = 0x9,    /* 負でなければ */
    CC_P = 0xA,     /* パリティ（浮動小数点の比較では NaN を含む） */
    CC_NP = 0xB,    /* パリティなし */
    CC_L = 0xC,     /* 符号付きで小さい */
    CC_GE = 0xD,    /* 符号付きで以上 */
    CC_LE = 0xE,    /* 符号付きで以下 */
    CC_G = 0xF      /* 符号付きで大きい */
} Cond;

/* SSE 命令（浮動小数点）の種類 */
typedef enum {
    SSE_ADD = 0x58,
    SSE_MUL = 0x59,
    SSE_SUB = 0x5C,
    SSE_DIV = 0x5E
} SseOp;

void x64_begin(Program *prog, FILE *text);
void x64_end(void);
void x64_directive(const char *fmt, ...);
void x64_comment(const char *fmt, ...);
int x64_new_label(const char *name);
void x64_bind(int label);
void x64_blob(const unsigned char *bytes, size_t length);

void x64_push(Reg r);
void x64_pop(Reg r);
void x64_mov(Reg dst, Reg src);
void x64_mov_imm(Reg dst, long long val);
void x64_add(Reg dst, Reg src);
void x64_sub(Reg dst, Reg src);
void x64_imul(Reg dst, Reg src);
void x64_xor(Reg dst, Reg src);
void x64_test(Reg a, Reg b);
void x64_add_imm(Reg dst, int32_t imm);
void x64_sub_imm(Reg dst, int32_t imm);
void x64_and_imm(Reg dst, int32_t imm);
void x64_shr_imm(Reg dst, int imm);
void x64_shl_imm(Reg dst, int imm);
void x64_or(Reg dst, Reg src);
void x64_and(Reg dst, Reg src);
void x64_load8(Reg dst, Reg base, int32_t disp);
void x64_mul(Reg r);
void x64_bsr(Reg dst, Reg src);
void x64_shl_cl(Reg dst);
void x64_shr_cl(Reg dst);
void x64_sar_cl(Reg dst);
void x64_neg(Reg r);
void x64_dec(Reg r);
void x64_cqo(void);
void x64_idiv(Reg r);
void x64_div(Reg r);
void x64_shr1(Reg r);
void x64_cmp(Reg a, Reg b);
void x64_btc(Reg r, int bit);
void x64_btr(Reg r, int bit);
void x64_rep_movsb(void);
void x64_repe_cmpsb(void);
void x64_xor_imm(Reg dst, int32_t imm);
void x64_setcc(Cond cc, Reg r);
void x64_movzx8(Reg dst, Reg src);
void x64_and8(Reg dst, Reg src);
void x64_or8(Reg dst, Reg src);

/* 浮動小数点（xmm は 0〜7 の番号） */
void x64_movq_to_xmm(int xmm, Reg src);
void x64_movq_from_xmm(Reg dst, int xmm);
void x64_movd_to_xmm(int xmm, Reg src);
void x64_movd_from_xmm(Reg dst, int xmm);
void x64_sse_sd(SseOp op, int dst, int src);
void x64_sse_ss(SseOp op, int dst, int src);
void x64_cvtsi2sd(int dst, Reg src);
void x64_cvtsi2ss(int dst, Reg src);
void x64_cvtss2sd(int dst, int src);
void x64_cvtsd2ss(int dst, int src);
void x64_cvtsd2si(Reg dst, int src);
void x64_cvttsd2si(Reg dst, int src);
void x64_sqrtsd(int dst, int src);
void x64_sqrtss(int dst, int src);
void x64_ucomisd(int a, int b);

void x64_load(Reg dst, Reg base, int32_t disp);
void x64_store(Reg base, int32_t disp, Reg src);
void x64_store_imm(Reg base, int32_t disp, int32_t imm);
void x64_store8(Reg base, int32_t disp, Reg src);
void x64_store8_imm(Reg base, int32_t disp, uint8_t imm);
void x64_lea(Reg dst, Reg base, int32_t disp);
void x64_lea_data(Reg dst, size_t data_offset);

void x64_jmp(int label);
void x64_jcc(Cond cc, int label);
void x64_call(int label);
void x64_call_import(Import imp);
void x64_syscall(void);
void x64_int3(void);
void x64_ret(void);

/* codegen.c：AST から実行ファイルの中身（エントリ処理・実行時ルーチン・main）を作る
 * text が NULL でなければアセンブリも出力する */
void codegen(Ast *ast, Program *prog, FILE *text, Target target);

/* pe.c：Windows 用の実行ファイルを書き出す */
const char *import_name(Import imp);
void write_pe(const char *path, const Program *prog);

/* elf.c：Linux 用の実行ファイルを書き出す */
void write_elf(const char *path, const Program *prog);

#endif
