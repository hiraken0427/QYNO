/*
 * 文法（現時点）
 *
 * program  = (import | const_decl | using | function)* EOF
 * import   = "import" path ("," path)* NEWLINE
 * using    = "using" path ("as" ident)? ("," path ("as" ident)?)* NEWLINE
 * path     = ident ("." ident)*
 * function = (rtype | "auto")? ident "(" params? ")" dimensions (";" block | "=" expr NEWLINE)
 * rtype    = type | "void"
 * params   = ("ref" "readonly"? | "val")? (type | "auto")? ident dimensions ("," params)?
 * dimensions = ("[" expr? "]")*
 * block    = NEWLINE INDENT stmt+ DEDENT
 * stmt     = "return" expr? NEWLINE
 *          | "print" "(" print_arg? ("," "end" "=" print_arg)? ")" NEWLINE
 *          | ident "(" args? ")" NEWLINE      （関数呼び出しの文）
 *          | input NEWLINE                    （形に当てはめる input は、失敗すると実行時エラー）
 *          | "if" "(" expr ")" ";" block ("else" "if" "(" expr ")" ";" block)* ("else" ";" block)?
 *          | "while" "(" expr ")" ";" block
 *          | "for" "(" add ("<=" | ">=") type? ident cmp_op add "," update ")" ";" block
 *          | "break" NEWLINE
 *          | "continue" NEWLINE
 *          | using
 *          | const_decl
 *          | "auto" ident "=" expr NEWLINE
 *          | ident "=" expr NEWLINE          （未宣言なら推論付きローカル宣言）
 *          | type ident dimensions ("=" expr)? NEWLINE
 *          | postfix update NEWLINE | postfix（配列操作）NEWLINE
 * update   = "=" expr | "+=" expr | "-=" expr | "++" | "--"
 * const_decl = "const" (type | "auto")? ident "=" expr NEWLINE
 * type     = "int" | "float" | "double" | "bool" | "str" | "char"
 * str      = 文字列リテラル（エスケープと {expr} の埋め込みを含む）
 * expr     = or
 * or       = and ("or" and)*
 * and      = not ("and" not)*
 * not      = ("not" | "!") not | compare
 * compare  = add (cmp_op add)*          （連鎖比較。a < b < c は a < b かつ b < c）
 * cmp_op   = "==" | "!=" | "<" | "<=" | ">" | ">="
 * add      = mul ("+" mul | "-" mul)*
 * mul      = unary ("*" unary | "/" unary | "%" unary)*
 * unary    = ("+" | "-") unary | power
 * power    = postfix ("^" unary)?           （右結合。-2^2 は -(2^2)、2^3^2 は 2^(3^2)）
 * postfix  = primary ("[" expr "]" | "." ("length" | "value" | "addr"))*
 * primary  = num | fnum | "true" | "false" | str | "|" expr "|" | "[" (expr ("," expr)* ","?)? "]"
 *          | ident "." ident "(" args? ")"        （名前空間付きの組み込み関数）
 *          | ("int" | "float" | "double" | "char" | "str") "(" args ")"   （型変換）
 *          | input
 *          | ident "(" args? ")" | ident | "(" expr ")"
 * input    = "input" "(" (str（{入力先} を含む） | ("prompt" "=")? expr)? ")"
 * args     = expr ("," expr)*
 * print_arg = str（{式} の埋め込み可） | expr
 * 標準 io の修飾名・別名も print/input の構文で解析し、参照先を型検査で確認する。
 */

#include "qyc.h"

#include <stdlib.h>
#include <string.h>

static Token *tok;
static Ast *ast;
static Function *cur_fn;        /* 解析中の関数（ファイル先頭の const の解析中は NULL） */
static Var *visible;        /* 今見えている変数（内側のブロックから外側へ vnext でつながる） */
static int loop_depth;      /* 今いるループ（for・while）の深さ */
static int block_depth;     /* 今いるブロックの深さ（関数の本体が 1） */
static Model *parsing_model;
static Interface *parsing_interface;
static Using *local_usings; /* 関数の中で今見えている using */

static Node *expr(void);
static Node *string_literal(Token *t);
static Node *stmt(void);

/* 変数名などに使えない予約語 */
static const char *keywords[] = {
    "complex", "complex_64", "complex_128", "int", "int_8", "int_16", "int_32", "int_64", "int_128", "half", "quadruple", "unsigned", "float", "double", "bool", "str", "char", "void", "Void", "file", "const", "auto",
    "true", "false", "return", "print", "if", "else", "for", "while",
    "break", "continue", "and", "or", "xor", "bnot", "not", "struct", "enum", "match", "case", "try", "finally", "throw", "import", "of", "using", "input",
    "ref", "val", "none", "as", "interface", "implements", "model", "public", "private", "readonly",
    NULL
};

static Node *new_node(NodeKind kind, Token *t)
{
    Node *node;

    node = calloc(1, sizeof(Node));
    if (node == NULL) {
        fatal("メモリを確保できません");
    }
    node->kind = kind;
    node->tok = t;
    node->usings = local_usings;
    node->access_model = parsing_model;
    return node;
}

static Node *new_binary(NodeKind kind, Node *lhs, Node *rhs, Token *t)
{
    Node *node;

    node = new_node(kind, t);
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

static Node *new_unary(NodeKind kind, Node *operand, Token *t)
{
    Node *node;

    node = new_node(kind, t);
    node->lhs = operand;
    return node;
}

static Node *new_num(long long val, Token *t)
{
    Node *node;

    node = new_node(ND_NUM, t);
    node->val = val;
    return node;
}

/* 現在のトークンが記号・識別子 s と一致するか */
static int equal(Token *t, const char *s)
{
    if (t->kind != TK_PUNCT && t->kind != TK_IDENT) {
        return 0;
    }
    return (int)strlen(s) == t->len && strncmp(t->loc, s, (size_t)t->len) == 0;
}

static int consume(const char *s)
{
    if (!equal(tok, s)) {
        return 0;
    }
    tok = tok->next;
    return 1;
}

/* 値の直後に閉じる相手のない | があるときのエラー（|| は 00.21 で廃止） */
static void check_stray_bar(void)
{
    if (!equal(tok, "|")) {
        return;
    }
    if (equal(tok->next, "|") && tok->next->loc == tok->loc + 1) {
        error_tok(tok, "|| は廃止されました。論理和には or を使ってください");
    }
    error_tok(tok, "この | に対応する開きの | がありません（絶対値は |x| と書きます）");
}

static void expect(const char *s)
{
    if (!equal(tok, s)) {
        check_stray_bar();
        error_tok(tok, "'%s' が必要です", s);
    }
    tok = tok->next;
}

static void expect_kind(TokenKind kind, const char *msg)
{
    if (tok->kind != kind) {
        check_stray_bar();
        error_tok(tok, "%s", msg);
    }
    tok = tok->next;
}

/* ---------- 変数 ---------- */

static int is_keyword(Token *t)
{
    int i;

    for (i = 0; keywords[i] != NULL; i++) {
        if (equal(t, keywords[i])) {
            return 1;
        }
    }
    return 0;
}

/* 型名なら型を、そうでなければ TY_NONE を返す */
static Type type_keyword(Token *t)
{
    if (equal(t, "complex") || equal(t, "complex_64")) return TY_C64;
    if (equal(t, "complex_128")) return TY_C128;
    if (equal(t, "unsigned")) {
        Type base;
        if (equal(t->next, "unsigned")) error_tok(t, "unsigned は1回だけ指定してください");
        base = type_keyword(t->next);
        if (!type_is_integer(base) || integer_unsigned(base)) error_tok(t, "unsigned の後は整数型にしてください");
        return integer_type(integer_width(base), 1);
    }
    if (equal(t, "int_8")) return TY_I8;
    if (equal(t, "int_16")) return TY_I16;
    if (equal(t, "int_32")) return TY_I32;
    if (equal(t, "int_64")) return TY_I64;
    if (equal(t, "int_128")) return TY_I128;
    if (equal(t, "half")) return TY_HALF;
    if (equal(t, "quadruple")) return TY_QUAD;
    if (equal(t,"void")||equal(t,"Void"))return TY_VOID;
    if (equal(t, "file")) return TY_FILE;
    if (equal(t, "int")) {
        return TY_INT;
    }
    if (equal(t, "float")) {
        return TY_FLOAT;
    }
    if (equal(t, "double")) {
        return TY_DOUBLE;
    }
    if (equal(t, "bool")) {
        return TY_BOOL;
    }
    if (equal(t, "str")) {
        return TY_STR;
    }
    if (equal(t, "char")) {
        return TY_CHAR;
    }
    if (t->kind == TK_IDENT && !is_keyword(t)) {
        Token *end = t;
        char *path;
        size_t length;
        while (equal(end->next, ".") && end->next->next->kind == TK_IDENT) end = end->next->next;
        if (end->next->kind != TK_IDENT) return TY_NONE;
        length = (size_t)(end->loc + end->len - t->loc);
        path = calloc(length + 1, 1);
        if (!path) fatal("型名を確保できません");
        /* Build the path from tokens so spaces around dots are harmless. */
        { Token *part; size_t n = 0;
          for (part = t; ; part = part->next) {
              memcpy(path + n, part->loc, (size_t)part->len); n += (size_t)part->len;
              if (part == end) break;
          }
        }
        { Type ty = named_type(t, path, local_usings); free(path); return ty; }
    }
    return TY_NONE;
}

static void skip_type(void)
{
    if (equal(tok, "unsigned")) tok = tok->next;
    tok = tok->next;
    while (equal(tok, ".") && tok->next->kind == TK_IDENT) tok = tok->next->next;
}

/* 宣言・引数・戻り値の各次元。[] の上限は -1 として保持する。 */
static ArrayDim *dimensions(Type *ty)
{
    ArrayDim head = {0};
    ArrayDim *tail = &head;
    int rank = 0;
    while (equal(tok, "[")) {
        ArrayDim *dim = calloc(1, sizeof(*dim));
        if (dim == NULL) {
            fatal("配列の次元情報を確保できません");
        }
        dim->tok = tok;
        dim->limit = -1;
        tok = tok->next;
        if (!equal(tok, "]")) {
            dim->bound = expr();
        }
        expect("]");
        tail = tail->next = dim;
        if (++rank > MAX_ARRAY_RANK) {
            error_tok(dim->tok, "配列の次元数は %d 以下にしてください", MAX_ARRAY_RANK);
        }
        *ty = array_type(*ty);
    }
    return head.next;
}

static Var *find_var(Token *t)
{
    Var *v;

    for (v = visible; v != NULL; v = v->vnext) {
        if (v->name_len == t->len && strncmp(v->name, t->loc, (size_t)t->len) == 0) {
            return v;
        }
    }
    return NULL;
}

static Var *new_var(Token *t, Type ty)
{
    Var *v;

    if (is_keyword(t)) {
        error_tok(t, "予約語は名前に使えません");
    }
    v = calloc(1, sizeof(Var));
    if (v == NULL) {
        fatal("メモリを確保できません");
    }
    v->name = t->loc;
    v->name_len = t->len;
    v->ty = ty;
    v->tok = t;
    v->usings = local_usings;
    return v;
}

static int same_name(Var *v, Token *t)
{
    return v->name_len == t->len && strncmp(v->name, t->loc, (size_t)t->len) == 0;
}

/*
 * 同じブロックの二重宣言は禁止し、明示的な内側の宣言は外側を隠す。
 * const は値をコンパイル時に決めるので、スタックの領域を使わない。
 * 関数の外（cur_fn が NULL）ではファイル先頭の const として登録する。
 * ファイル先頭の const と同じ名前のローカル変数は、型検査で検出する。
 */
static Var *declare_var(Token *t, Type ty, int is_const)
{
    Var *v;
    Var *prev;
    int count;

    if (cur_fn == NULL) {
        for (prev = ast->global_vars; prev != NULL; prev = prev->next) {
            if (same_name(prev, t)) {
                error_tok(t, "%.*s はすでに %d 行目で宣言されています", t->len, t->loc, prev->tok->line);
            }
        }
        v = new_var(t, ty);
        v->is_const = 1;
        v->is_global = 1;
        v->next = ast->global_vars;
        ast->global_vars = v;
        return v;
    }

    prev = find_var(t);
    if (prev != NULL && (prev->depth == block_depth ||
        (prev->is_param && (block_depth == (cur_fn->model ? 2 : 1) ||
            (cur_fn->model && equal(t, "this")))))) {
        error_tok(t, "%.*s はすでに %d 行目で宣言されています", t->len, t->loc, prev->tok->line);
    }
    v = new_var(t, ty);
    v->is_const = is_const;
    v->depth = block_depth;
    if (!is_const) {
        count = 0;
        for (prev = cur_fn->locals; prev != NULL; prev = prev->next) {
            if (!prev->is_const) {
                count++;
            }
        }
        v->offset = 8 * (count + 1);
    }
    v->next = cur_fn->locals;
    cur_fn->locals = v;
    v->vnext = visible;
    visible = v;
    return v;
}

/* ---------- 式 ---------- */

/* 式の中の文字列リテラル（{式} の埋め込みは print の中だけで使える） */
static Node *string_expr(Token *t)
{
    Node *parts;
    Node *node;

    parts = string_literal(t);
    for (node = parts; node != NULL; node = node->next) {
        if (node->kind != ND_STR) {
            error_tok(t, "{式} の埋め込みは現在 print の中でのみ使えます");
        }
    }
    node = new_node(ND_STRLIT, t);
    if (parts != NULL) {
        node->str = parts->str;
        node->str_len = parts->str_len;
    }
    return node;
}

/* 隠れたローカル変数（名前を持たないので、ほかの名前と重ならない） */
static Var *hidden_var(Type ty, Token *where)
{
    Var *v;
    Var *prev;
    int count;

    if (cur_fn == NULL) {
        error_tok(where, "input は関数の中でだけ使えます");
    }
    v = calloc(1, sizeof(Var));
    if (v == NULL) {
        fatal("メモリを確保できません");
    }
    v->name = "";
    v->name_len = 0;
    v->ty = ty;
    v->tok = where;
    count = 0;
    for (prev = cur_fn->locals; prev != NULL; prev = prev->next) {
        if (!prev->is_const) {
            count++;
        }
    }
    v->offset = 8 * (count + 1);
    v->next = cur_fn->locals;
    cur_fn->locals = v;
    return v;
}

/* 文字列リテラルに、\{ でない { があるか */
static int has_placeholder(Token *t)
{
    const char *p;
    const char *end;

    end = t->loc + t->len - 1;
    for (p = t->loc + 1; p < end; p++) {
        if (*p == '\\') {
            p++;
            continue;
        }
        if (*p == '{') {
            return 1;
        }
    }
    return 0;
}

static void append_text(char **buf, int *len, const char *s, int n)
{
    char *nb;

    nb = realloc(*buf, (size_t)(*len + n + 1));
    if (nb == NULL) {
        fatal("メモリを確保できません");
    }
    memcpy(nb + *len, s, (size_t)n);
    *len += n;
    nb[*len] = '\0';
    *buf = nb;
}

/*
 * input(...)
 *   値の形            input() ・ input(プロンプト)     読んだ1行を、代入先の型で読む（型検査で決まる）
 *   形に当てはめる形  input("表示する文字{a}区切り{b}")  最初の { より前を表示し、後ろの形に当てはめて変数に入れる
 * 入力先は変数または添字。{} と {} の間には区切りが必要で、最後の {} の後ろは右側の表示枠。
 * input(x) も直接入力。値をプロンプトとして渡すには input(prompt=x) を使う。
 */
static Node *input_expr(Token *start)
{
    Node *node;
    Node *part;
    Node *parts;
    InputField head;
    InputField *cur;
    InputField *f;
    Token *lit;
    Type ty;

    node = new_node(ND_INPUT, start);
    node->name = "input";
    expect("(");
    if (equal(tok, "prompt") && equal(tok->next, "=")) {
        tok = tok->next->next;
        node->lhs = expr();
    } else if (tok->kind == TK_STR && equal(tok->next, ")") && has_placeholder(tok)) {
        lit = tok;
        parts = string_literal(lit);
        tok = tok->next;
        node->input_pattern = 1;
        memset(&head, 0, sizeof(head));
        cur = NULL;
        f = &head;
        for (part = parts; part != NULL; part = part->next) {
            if (part->kind == ND_STR) {
                if (part->str_len == 0) {
                    continue;
                }
                if (cur == NULL) {
                    append_text(&node->prompt, &node->prompt_len, part->str, part->str_len);
                } else {
                    append_text(&cur->sep, &cur->sep_len, part->str, part->str_len);
                }
                continue;
            }
            if (part->kind != ND_VAR && part->kind != ND_INDEX && part->kind != ND_FIELD) {
                error_tok(part->tok, "input の {} の中には、読み込む先の変数または配列要素を書いてください");
            }
            if (cur != NULL && cur->sep_len == 0) {
                error_tok(part->tok, "{} と {} の間には、区切りの文字が必要です（例：{a}, {b}）");
            }
            cur = calloc(1, sizeof(InputField));
            if (cur == NULL) {
                fatal("メモリを確保できません");
            }
            cur->var = part; cur->base = part->format_base;
            ty = part->var != NULL ? part->var->ty : TY_INT;
            cur->temp = hidden_var(ty, part->tok);
            f = f->next = cur;
        }
        if (cur != NULL && cur->sep_len > 0) {
            /* 最後の {} より後は右の枠（端末では入力欄の右側に表示し、入力とは照合しない） */
            node->suffix = cur->sep;
            node->suffix_len = cur->sep_len;
            cur->sep = NULL;
            cur->sep_len = 0;
        }
        node->fields = head.next;
        for (f = node->fields; f != NULL; f = f->next) {
            part = f->var;
            part->next = NULL;
        }
    } else if (!equal(tok, ")")) {
        node->lhs = expr();
        if (node->lhs->kind == ND_VAR || node->lhs->kind == ND_INDEX || node->lhs->kind == ND_FIELD) {
            node->input_pattern = 1;
            node->fields = calloc(1, sizeof(InputField));
            if (node->fields == NULL) fatal("入力先を確保できません");
            node->fields->var = node->lhs;
            node->fields->temp = hidden_var(TY_INT, start);
            node->lhs = NULL;
        }
    }
    if (!equal(tok, ")")) {
        error_tok(tok, "input の引数は1つです（複数の値は input(\"表示{a}, {b}\") のように形で書きます）");
    }
    expect(")");
    node->in_line = hidden_var(TY_STR, start);
    node->in_cur = hidden_var(TY_I64, start);
    node->in_end = hidden_var(TY_I64, start);
    return node;
}

/* ドット区切りの名前。元 Token は診断のために保持する。 */
static char *name_path(void)
{
    char *name = NULL;
    size_t size = 0;
    for (;;) {
        size_t length;
        char *grown;
        if (tok->kind != TK_IDENT ||
            (is_keyword(tok) && !(equal(tok, "file") && (size != 0 || equal(tok->next, "."))) && !(size != 0 && (equal(tok, "print") || equal(tok, "input") || equal(tok, "file"))))) {
            error_tok(tok, "モジュール名または識別子が必要です");
        }
        length = (size_t)tok->len;
        grown = realloc(name, size + length + 2);
        if (grown == NULL) fatal("名前の領域を確保できません");
        name = grown;
        if (size != 0) name[size++] = '.';
        memcpy(name + size, tok->loc, length);
        size += length;
        name[size] = '\0';
        tok = tok->next;
        if (!consume(".")) break;
    }
    return name;
}

/* 名前空間付き I/O の構文を選ぶ。参照先そのものは型検査で確認する。 */
static Token *qualified_io_call(Token *start)
{
    Token *last = start;
    int qualified = 0;
    if (last->kind != TK_IDENT || is_keyword(last)) return NULL;
    while (equal(last->next, ".") && last->next->next->kind == TK_IDENT) {
        last = last->next->next;
        qualified = 1;
    }
    if (qualified && equal(last->next, "(") && (equal(last, "print") || equal(last, "input"))) return last;
    return NULL;
}

/* "(" の後の引数を読み、")" まで進める */
static void call_args(Node *node)
{
    Node head;
    Node *cur;

    memset(&head, 0, sizeof(head));
    cur = &head;
    if (!equal(tok, ")")) {
        for (;;) {
            Token *mark = tok;
            if (node->kind == ND_BUILTIN && (node->val == BI_STR || node->val == BI_INT || node->val == BI_COMPLEX) &&
                tok->kind == TK_IDENT && equal(tok->next, "=")) {
                Node *arg; int slot;
                if (node->val == BI_COMPLEX) {
                    if (equal(tok, "real")) slot = 0;
                    else if (equal(tok, "imag")) slot = 1;
                    else { error_tok(tok, "complex の名前付き引数は real / imag です"); slot = 0; }
                }
                else if (equal(tok, "base")) slot = 1;
                else if (equal(tok, "prefix")) slot = 2;
                else { error_tok(tok, "int / str の名前付き引数は base / prefix です"); slot = 0; }
                tok = tok->next->next; arg = expr(); arg->option_index = slot;
                arg->arg_name = mark; cur = cur->next = arg;
            } else if (consume("ref")) {
                /* ref 変数：変数の場所を渡す */
                Node *arg = new_node(ND_REFARG, mark);
                arg->lhs = expr();
                cur = cur->next = arg;
            } else if (consume("val")) {
                cur = cur->next = expr();
                cur->arg_val = 1;
            } else if (tok->kind == TK_STR && has_placeholder(tok)) {
                Node *format = new_node(ND_FORMAT, tok);
                format->lhs = string_literal(tok);
                tok = tok->next;
                cur = cur->next = format;
            } else {
                cur = cur->next = expr();
            }
            node->nargs++;
            if (!consume(",")) {
                break;
            }
        }
    }
    expect(")");
    node->lhs = head.next;
}

/* x.value・x.addr のような変数の要素か（直後が "(" なら名前空間の関数） */
static int is_member(Token *t)
{
    return t->kind == TK_IDENT && !is_keyword(t) && equal(t->next, ".") &&
           (equal(t->next->next, "value") || equal(t->next->next, "addr")) && !equal(t->next->next->next, "(");
}

/* primary = num | fnum | "true" | "false" | str | ident "(" args? ")" | ident | "(" expr ")" */
static Node *primary(void)
{
    Node *node;
    Var *v;
    Token *start;
    char *name;
    const char *member;

    if (consume("(")) {
        if(consume(")")){node=new_node(ND_UNIT,tok);node->ty=TY_VOID;return node;}
        node = expr();
        expect(")");
        return node;
    }
    if (consume("{")) {
        Node head = {0}; Node *tail = &head;
        node = new_node(ND_MODEL_INIT, tok);
        while (!equal(tok, "}")) {
            Token *field = tok; Node *item;
            if (field->kind != TK_IDENT || is_keyword(field)) error_tok(field, "初期化するフィールド名が必要です");
            tok = tok->next; expect("=");
            item = new_unary(ND_FIELD, expr(), field);
            item->name = calloc((size_t)field->len + 1, 1);
            if (!item->name) fatal("フィールド名を確保できません");
            memcpy(item->name, field->loc, (size_t)field->len);
            tail = tail->next = item; node->nargs++;
            if (!consume(",")) break;
        }
        expect("}"); node->lhs = head.next; return node;
    }
    if (consume("[")) {
        Node head = {0};
        Node *tail = &head;
        node = new_node(ND_ARRAY_LITERAL, tok);
        while (!equal(tok, "]")) {
            tail = tail->next = expr();
            node->nargs++;
            if (!consume(",")) {
                break;
            }
        }
        expect("]");
        node->lhs = head.next;
        return node;
    }
    if (tok->kind == TK_IMAG) {
        node = new_node(ND_COMPLEX, tok); node->imag = tok->fval; node->ty = TY_C128;
        tok = tok->next; return node;
    }
    if (tok->kind == TK_NUM) {
        if (tok->wide_ty) { node = new_node(ND_WIDE, tok); node->wide = tok->wide; node->ty = (Type)tok->wide_ty; node->raw_numeric = 1; tok = tok->next; return node; }
        node = new_num(tok->val, tok);
        if (tok->unsigned_literal) node->ty = TY_U64;
        else if (tok->val > INT32_MAX || tok->val < 0) node->ty = TY_I64;
        tok = tok->next;
        return node;
    }
    if (tok->kind == TK_FNUM) {
        node = new_node(ND_FNUM, tok);
        node->fval = tok->fval; node->raw_numeric = 1;
        if (tok->wide_ty) { node->kind = ND_WIDE; node->wide = tok->wide; node->ty = (Type)tok->wide_ty; }
        tok = tok->next;
        return node;
    }
    if (tok->kind == TK_STR) {
        node = string_expr(tok);
        tok = tok->next;
        return node;
    }
    if (equal(tok, "true") || equal(tok, "false")) {
        node = new_node(ND_BOOL, tok);
        node->val = equal(tok, "true");
        tok = tok->next;
        return node;
    }
    if (equal(tok, "input") && equal(tok->next, "(")) {
        start = tok;
        tok = tok->next;
        return input_expr(start);
    }
    if (equal(tok, "none")) {
        node = new_node(ND_NONE, tok);
        tok = tok->next;
        return node;
    }
    if (consume("|")) {
        /* 絶対値：値の位置の | は開き、中の式の後（値の直後）の | は閉じ */
        start = tok;
        node = new_unary(ND_ABS, expr(), start);
        if (!consume("|")) {
            error_tok(tok, "絶対値を閉じる | が必要です");
        }
        return node;
    }
    if (type_is_complex(type_keyword(tok)) && equal(tok->next, "(")) {
        node = new_node(ND_BUILTIN, tok); node->val = BI_COMPLEX; node->cmp_ty = type_keyword(tok);
        tok = tok->next->next; call_args(node); return node;
    }
    if (((type_is_integer(type_keyword(tok)) && equal((equal(tok, "unsigned") ? tok->next : tok)->next, "(")) || equal(tok, "float") || equal(tok, "double") || equal(tok, "half") || equal(tok, "quadruple") ||
         equal(tok, "char") || equal(tok, "str")) && (equal(tok, "unsigned") || equal(tok->next, "("))) {
        /* 型変換 int(x)・float(x)・double(x) */
        node = new_node(ND_BUILTIN, tok);
        node->val = builtin_find(NULL, tok);
        if (type_is_integer(type_keyword(tok))) {
            node->cmp_ty = type_keyword(tok); node->val = BI_INT;
        }
        skip_type(); tok = tok->next;
        call_args(node);
        return node;
    }
    if (tok->kind == TK_IDENT && (!is_keyword(tok) || (equal(tok, "file") && equal(tok->next, ".")))) {
        start = tok;
        v = find_var(start);
        if (v != NULL && !equal(start->next, "(")) {
            node = new_node(ND_VAR, start);
            node->var = v;
            v->is_used = 1;
            tok = tok->next;
            return node;
        }
        name = name_path();
        member = strrchr(name, '.');
        member = member != NULL ? member + 1 : name;
        if (equal(tok, "(") && strcmp(member, "input") == 0) {
            node = input_expr(start);
            node->name = name;
            return node;
        }
        if (equal(tok, "(") && strcmp(member, "print") == 0) {
            error_tok(start, "print は文として使ってください（戻り値はありません）");
        }
        node = new_node(ND_VAR, start);
        node->name = name;
        if (consume("(")) {
            node->kind = ND_CALL;
            call_args(node);
        }
        return node;
    }
    error_tok(tok, "値（数値・文字列・true・false・変数）または '(' が必要です");
    return NULL;
}

static Node *unary(void);

/* 後置操作を共通化。受け手を解析した後、添字・length を記述順に連結する。 */
static Node *postfix(void)
{
    Node *node = primary();
    Token *op;
    Token *field;
    int variable_member = 0;

    for (;;) {
        op = tok;
        if (consume("[")) {
            node = new_binary(ND_INDEX, node, expr(), op);
            expect("]");
            variable_member = 0;
        } else if (consume(".")) {
            field = tok;
            if (field->kind == TK_IDENT && equal(field->next, "(")) {
                Node *method = new_node(ND_METHOD, field);
                Node *receiver = node;
                if (equal(field, "add")) {
                    method->val = AM_ADD;
                } else if (equal(field, "remove_at")) {
                    method->val = AM_REMOVE_AT;
                } else if (equal(field, "clear")) {
                    method->val = AM_CLEAR;
                } else {
                    method->kind = ND_MODEL_METHOD;
                    method->name = calloc((size_t)field->len + 1, 1);
                    if (!method->name) fatal("メソッド名を確保できません");
                    memcpy(method->name, field->loc, (size_t)field->len);
                }
                tok = field->next->next;
                call_args(method);
                method->rhs = method->lhs;
                method->lhs = receiver;
                node = method;
                variable_member = 0;
            } else if (consume("length")) {
                node = new_unary(ND_LENGTH, node, field);
                variable_member = 0;
            } else if (equal(field, "value") || equal(field, "addr")) {
                if (node->kind != ND_VAR || variable_member) {
                    error_tok(field, ".value と .addr は変数名の直後に1回だけ書けます");
                }
                tok = tok->next;
                if (equal(field, "addr")) {
                    node = new_unary(ND_ADDR, node, field);
                }
                if (equal(field, "value")) node->explicit_value = 1;
                variable_member = 1;
            } else {
                Node *access;
                if (field->kind != TK_IDENT || is_keyword(field)) error_tok(field, "フィールド名が必要です");
                access = new_unary(ND_FIELD, node, field);
                access->name = calloc((size_t)field->len + 1, 1);
                if (!access->name) fatal("フィールド名を確保できません");
                memcpy(access->name, field->loc, (size_t)field->len);
                tok = tok->next; node = access; variable_member = 0;
            }
        } else {
            return node;
        }
    }
}

/* power = postfix ("^" unary)? */
static Node *power(void)
{
    Node *node;
    Token *op;

    node = postfix();
    op = tok;
    if (consume("^")) {
        return new_binary(ND_POW, node, unary(), op);
    }
    return node;
}

/* unary = ("+" | "-") unary | power */
static Node *unary(void)
{
    Token *start;

    start = tok;
    if (consume("+")) {
        return unary();
    }
    if (consume("bnot")) return new_unary(ND_BIT_NOT, unary(), start);
    if (consume("-")) {

        if (tok->kind == TK_NUM && !tok->wide_ty && (tok->decimal_min || tok->val == INT64_C(2147483648)) && !equal(tok->next, "^")) {
            Node *minimum = new_num(tok->decimal_min ? INT64_MIN : INT32_MIN, start); tok = tok->next; return minimum;
        }
        { Node *operand = unary();
          if ((operand->kind == ND_WIDE && operand->raw_numeric && type_is_integer(operand->ty)) ||
              (operand->kind == ND_NUM && operand->tok->unsigned_literal && !operand->tok->decimal_min)) {
              WideRequest r = {0}; Node *n;
              r.a = operand->kind == ND_WIDE ? operand->wide : (WideBits){(uint64_t)operand->val, 0};
              if (operand->ty == TY_U128 && (r.a.hi > UINT64_C(0x8000000000000000) || (r.a.hi == UINT64_C(0x8000000000000000) && r.a.lo))) error_tok(start, "負の128bit整数リテラルが範囲外です");
              r.op=W_NEG;r.ty=-128;wide_execute(&r);
              n=new_node(ND_WIDE,start);n->wide=r.out;n->ty=TY_I128;n->raw_numeric=1;return n;
          }
          /* Parentheses and unary + do not change a minimum literal's type. */
          if (operand->kind == ND_NUM && operand->tok->kind == TK_NUM) {
              if (operand->tok->unsigned_literal) {
                  if (!operand->tok->decimal_min) error_tok(operand->tok, "負の整数リテラルが範囲外です");
                  return new_num(INT64_MIN, start);
              }
              if (operand->val == INT64_C(2147483648)) return new_num(INT32_MIN, start);
          }
          return new_unary(ND_NEG, operand, start);
        }
    }
    return power();
}

/* mul = unary ("*" unary | "/" unary | "%" unary)* */
static Node *mul(void)
{
    Node *node;
    Node *rhs;
    Token *start;

    node = unary();
    for (;;) {
        start = tok;
        if (consume("*")) {
            node = new_binary(ND_MUL, node, unary(), start);
            continue;
        }
        if (consume("/")) {
            rhs = unary();
            if (rhs->kind == ND_NUM && rhs->val == 0) {
                error_tok(start, "0で割ることはできません");
            }
            node = new_binary(ND_DIV, node, rhs, start);
            continue;
        }
        if (consume("%")) {
            rhs = unary();
            if (rhs->kind == ND_NUM && rhs->val == 0) {
                error_tok(start, "0で割った余りは求められません");
            }
            node = new_binary(ND_MOD, node, rhs, start);
            continue;
        }
        return node;
    }
}

/* add = mul ("+" mul | "-" mul)* */
static Node *add(void)
{
    Node *node;
    Token *start;

    node = mul();
    for (;;) {
        start = tok;
        if (consume("+")) {
            node = new_binary(ND_ADD, node, mul(), start);
            continue;
        }
        if (consume("-")) {
            node = new_binary(ND_SUB, node, mul(), start);
            continue;
        }
        return node;
    }
}

static Node *shift(void)
{
    Node *node = add();
    while (equal(tok, "<<") || equal(tok, ">>")) {
        Token *op = tok; NodeKind kind = equal(tok, "<<") ? ND_SHL : ND_SHR;
        tok = tok->next; node = new_binary(kind, node, add(), op);
    }
    return node;
}

static int is_cmp_op(Token *t)
{
    return equal(t, "==") || equal(t, "!=") || equal(t, "<") || equal(t, "<=") || equal(t, ">") || equal(t, ">=");
}

static CmpKind cmp_kind(Token *t)
{
    if (equal(t, "==")) {
        return CMP_EQ;
    }
    if (equal(t, "!=")) {
        return CMP_NE;
    }
    if (equal(t, "<")) {
        return CMP_LT;
    }
    if (equal(t, "<=")) {
        return CMP_LE;
    }
    if (equal(t, ">")) {
        return CMP_GT;
    }
    return CMP_GE;
}

/*
 * compare = add (cmp_op add)*
 * 比較は ND_CHAIN にまとめ、比較1つずつを ND_CMP として next でつなぐ。
 * a < b < c では、1つ目の比較の右辺と2つ目の比較の左辺が同じ b を指す（b は1回だけ評価する）。
 */
static Node *compare(void)
{
    Node *first;
    Node *left;
    Node *right;
    Node *chain;
    Node *cmp;
    Node head;
    Node *cur;
    Token *op;

    first = shift();
    if (!is_cmp_op(tok)) {
        return first;
    }

    memset(&head, 0, sizeof(head));
    cur = &head;
    left = first;
    chain = new_node(ND_CHAIN, tok);
    while (is_cmp_op(tok)) {
        op = tok;
        tok = tok->next;
        right = shift();
        cmp = new_node(ND_CMP, op);
        cmp->val = cmp_kind(op);
        cmp->lhs = left;
        cmp->rhs = right;
        cur = cur->next = cmp;
        left = right;
    }
    chain->lhs = head.next;
    return chain;
}

/* not = ("not" | "!") not | compare */
static Node *logic_not(void)
{
    Token *start;

    start = tok;
    if (consume("not") || consume("!")) {
        return new_unary(ND_NOT, logic_not(), start);
    }
    return compare();
}

/* and = not ("and" not)* */
static Node *logic_and(void)
{
    Node *node;
    Token *start;

    node = logic_not();
    for (;;) {
        start = tok;
        if (consume("and")) {
            node = new_binary(ND_AND, node, logic_not(), start);
            continue;
        }
        return node;
    }
}

static Node *logic_xor(void)
{
    Node *node = logic_and();
    while (equal(tok, "xor")) {
        Token *op = tok; tok = tok->next; node = new_binary(ND_XOR, node, logic_and(), op);
    }
    return node;
}

/* or = xor ("or" xor)* */
static Node *logic_or(void)
{
    Node *node;
    Token *start;

    node = logic_xor();
    for (;;) {
        start = tok;
        if (consume("or")) {
            node = new_binary(ND_OR, node, logic_xor(), start);
            continue;
        }
        return node;
    }
}

/* expr = or */
static Node *expr(void)
{
    return logic_or();
}

/* 文字列の中の位置を列番号に直す */
static int col_in(Token *t, const char *p)
{
    return t->col + (int)(p - t->loc);
}

/* ここまでの固定部分を ND_STR にして list の末尾に追加する */
static Node *flush_literal(Node *cur, Buffer *lit, Token *t)
{
    Node *node;

    if (lit->len == 0) {
        return cur;
    }
    node = new_node(ND_STR, t);
    node->str = malloc(lit->len);
    if (node->str == NULL) {
        fatal("メモリを確保できません");
    }
    memcpy(node->str, lit->data, lit->len);
    node->str_len = (int)lit->len;
    lit->len = 0;
    cur->next = node;
    return node;
}

static char escape_char(Token *t, const char *p)
{
    switch (p[1]) {
    case 'n':
        return '\n';
    case 't':
        return '\t';
    case 'r':
        return '\r';
    case '0':
        return '\0';
    case '\\':
    case '"':
    case '\'':
    case '{':
    case '}':
        return p[1];
    default:
        error_at(t->line, col_in(t, p), "使えないエスケープです（\\n \\t \\r \\0 \\\\ \\\" \\' \\{ \\} が使えます）");
        return 0;
    }
}

/* {式} の中身を式として解析する */
static Node *embedded_expr(Token *t, const char *open, const char *close)
{
    Token *saved;
    Node *node;
    const char *line_start;

    const char *p, *colon = NULL; int depth = 0, base = 0; char quote = 0;
    for (p = open + 1; p < close; p++) {
        if (*p == '\\' && p + 1 < close) { p++; continue; }
        if (quote) { if (*p == quote) quote = 0; continue; }
        if (*p == '\'' || *p == '"') { quote = *p; continue; }
        if (*p == '(' || *p == '[' || *p == '{') depth++;
        else if (*p == ')' || *p == ']' || *p == '}') depth--;
        else if (*p == ':' && !depth) { colon = p; break; }
    }
    if (colon) {
        const char *end = close; p = colon + 1;
        while (p < end && (*p == ' ' || *p == '\t')) p++;
        while (end > p && (end[-1] == ' ' || end[-1] == '\t')) end--;
        if (end - p != 2 || p[0] != '0' || !(p[1] == 'b' || p[1] == 'o' || p[1] == 'd' || p[1] == 'x'))
            error_at(t->line, col_in(t, colon), "書式指定は 0b / 0o / 0d / 0x にしてください");
        base = integer_prefix(p[1]);
    }
    line_start = t->loc - (t->col - 1);
    saved = tok;
    tok = tokenize_fragment(open + 1, colon ? colon : close, t->line, line_start);
    if (tok->kind == TK_EOF) {
        error_at(t->line, col_in(t, open), "{} の中に式を書いてください（{ そのものは \\{ と書きます）");
    }
    node = expr();
    if (tok->kind != TK_EOF) {
        error_tok(tok, "埋め込みの式が正しくありません");
    }
    tok = saved;
    node->format_base = base;
    return node;
}

/*
 * 文字列リテラルを、固定部分（ND_STR）と埋め込みの式の並びに分解する。
 * 並びは next でつながり、先頭を返す（空文字列なら NULL）。
 */
static Node *string_literal(Token *t)
{
    Node head;
    Node *cur;
    Buffer lit;
    const char *p;
    const char *end;
    const char *close;

    memset(&head, 0, sizeof(head));
    cur = &head;
    buf_init(&lit);
    p = t->loc + 1;
    end = t->loc + t->len - 1;

    while (p < end) {
        if (*p == '\\') {
            buf_add_u8(&lit, (uint8_t)escape_char(t, p));
            p += 2;
            continue;
        }
        if (*p == '{') {
            close = interpolation_close(p, end);
            if (!close) {
                error_at(t->line, col_in(t, p), "{ に対応する } がありません（{ そのものは \\{ と書きます）");
            }
            cur = flush_literal(cur, &lit, t);
            cur->next = embedded_expr(t, p, close);
            cur = cur->next;
            p = close + 1;
            continue;
        }
        buf_add_u8(&lit, (uint8_t)*p);
        p++;
    }
    flush_literal(cur, &lit, t);
    buf_free(&lit);
    return head.next;
}

static int is_end_arg(Token *t)
{
    return t->kind == TK_IDENT && equal(t, "end") && equal(t->next, "=");
}

/*
 * print の引数1つ分：文字列リテラルだけなら {式} の埋め込みを解析し、*literal を 1 にする。
 * それ以外は任意の型の式。
 */
static Node *print_arg(int *literal)
{
    Node *node;

    if (tok->kind == TK_STR && (equal(tok->next, ")") || equal(tok->next, ","))) {
        node = string_literal(tok);
        *literal = 1;
        tok = tok->next;
        return node;
    }
    *literal = 0;
    return expr();
}

/*
 * print(値?, end=値?)
 * 値は文字列リテラル（{式} の埋め込み可）か任意の型の式。省略すると何も出さない。
 * end= を省略すると改行（end="\n" と同じ）。
 */
static Node *print_stmt(Token *start)
{
    Node *node;
    int literal;

    node = new_node(ND_PRINT, start);
    node->name = "print";
    expect("(");
    if (!equal(tok, ")") && !is_end_arg(tok)) {
        node->lhs = print_arg(&literal);
        node->val = literal;
        if (!equal(tok, ")")) {
            expect(",");
            if (!is_end_arg(tok)) {
                error_tok(tok, "print の2つ目には end=値 を書いてください（例：print(x, end=\"\")）");
            }
        }
    }
    if (is_end_arg(tok)) {
        tok = tok->next->next;
        node->has_end = 1;
        node->end = print_arg(&literal);
        node->end_literal = literal;
    }
    expect(")");
    expect_kind(TK_NEWLINE, "文の終わりには改行が必要です");
    return node;
}

/* type ident ("=" expr)? NEWLINE（is_const なら "const" の後。初期値は必須） */
static Node *declaration_ref(Token *start, int is_const, int is_ref);

static Node *declaration(Token *start, int is_const)
{
    return declaration_ref(start, is_const, 0);
}

/* ref 型 名前 (= 場所)?：別の変数の場所を使う変数（初期値を省くと何も指さない） */
static Node *declaration_ref(Token *start, int is_const, int is_ref)
{
    Node *node;
    Token *name;
    Type ty;
    ArrayDim *dims;
    int short_const = is_const && tok->kind == TK_IDENT && !is_keyword(tok) && equal(tok->next, "=");
    int infer = short_const || equal(tok, "auto");

    ty = infer ? TY_NONE : type_keyword(tok);
    if (infer && is_ref) {
        error_tok(tok, "auto は値変数・const の宣言に使用してください（ref・関数引数・戻り値は未対応です）");
    }
    if (!infer && ty == TY_NONE) {
        error_tok(tok, "型（int・float・double・bool・str・char）が必要です");
    }
    if (!short_const) skip_type();
    if (tok->kind != TK_IDENT) {
        error_tok(tok, "%sが必要です", is_const ? "定数名" : "変数名");
    }
    name = tok;
    tok = tok->next;

    if (infer && equal(tok, "[")) {
        error_tok(tok, "auto の配列は初期値から次元も推論します（例：auto values = [1, 2, 3]）");
    }
    dims = infer ? NULL : dimensions(&ty);
    node = new_node(ND_DECL, start);
    if (consume("=")) {
        node->lhs = expr();
    } else if (infer) {
        error_tok(tok, "auto には初期値が必要です（例：auto value = 3）");
    } else if (is_const) {
        error_tok(tok, "const には初期値が必要です（例：const float PI = 3.141592）");
    }
    /* 初期値の式の後で登録する（int a = a のような自己参照を防ぐ） */
    node->var = declare_var(name, ty, is_const);
    node->var->dims = dims;
    node->var->decl = node;
    node->var->is_ref = is_ref;
    node->var->is_inferred = infer;
    expect_kind(TK_NEWLINE, "文の終わりには改行が必要です");
    return node;
}

static int is_update_op(Token *t)
{
    return equal(t, "=") || equal(t, "+=") || equal(t, "-=") || equal(t, "++") || equal(t, "--");
}

/*
 * 変数の更新（改行は読まない）
 *   = expr / += expr / -= expr / ++ / --
 * += などは「変数 = 変数 + 式」の形の ND_ASSIGN にする。
 * inc・inc_sign には増分の量と向きを返す（= のときは NULL と 0）。
 */
static Node *update(Token *name, Node **inc, int *inc_sign)
{
    Node *node;
    Node *read;
    Node *amount;
    Token *op;
    Var *v;
    int sign;

    v = find_var(name);
    if (v == NULL) {
        error_tok(name, "変数 %.*s は宣言されていません", name->len, name->loc);
    }
    if (v->is_const) {
        error_tok(name, "定数 %.*s には代入できません", name->len, name->loc);
    }
    node = new_node(ND_ASSIGN, name);
    node->var = v;
    read = new_node(ND_VAR, name);
    read->var = v;
    op = tok;
    amount = NULL;
    sign = 0;

    if (consume("=")) {
        node->lhs = expr();
    } else if (consume("++")) {
        amount = new_num(1, op);
        sign = 1;
        node->lhs = new_binary(ND_ADD, read, amount, op);
    } else if (consume("--")) {
        amount = new_num(1, op);
        sign = -1;
        node->lhs = new_binary(ND_SUB, read, amount, op);
    } else if (consume("+=")) {
        amount = expr();
        sign = 1;
        node->lhs = new_binary(ND_ADD, read, amount, op);
    } else if (consume("-=")) {
        amount = expr();
        sign = -1;
        node->lhs = new_binary(ND_SUB, read, amount, op);
    } else {
        error_tok(tok, "=、+=、-=、++、-- のいずれかが必要です");
    }
    if (!equal(op, "=")) v->is_used = 1;
    if (inc != NULL) {
        *inc = amount;
    }
    if (inc_sign != NULL) {
        *inc_sign = sign;
    }
    return node;
}

/* ident update NEWLINE */
static Node *assignment(void)
{
    Node *node;
    Token *name;

    name = tok;
    tok = tok->next;
    if (equal(tok, "=") && find_var(name) == NULL) {
        node = new_node(ND_DECL, name);
        tok = tok->next;
        node->lhs = expr();
        node->var = declare_var(name, TY_NONE, 0);
        node->var->is_inferred = 1;
        node->var->is_implicit = 1;
        node->var->decl = node;
    } else {
        node = update(name, NULL, NULL);
    }
    expect_kind(TK_NEWLINE, "文の終わりには改行が必要です");
    return node;
}

/* using path [as alias]：対象の解決は全モジュールの解析後に行う。 */
static Using *using_list(Token *start)
{
    Using head = {0};
    Using *tail = &head;
    Using *scope = local_usings;
    for (;;) {
        Using *u = calloc(1, sizeof(Using));
        if (u == NULL) fatal("using の領域を確保できません");
        u->path_tok = tok;
        u->tok = start;
        u->path = name_path();
        u->target_builtin = -1;
        u->scope = scope;
        u->locals = visible;
        if (consume("as")) {
            if (tok->kind != TK_IDENT || is_keyword(tok)) {
                error_tok(tok, "as の後に別名が必要です");
            }
            u->alias = tok;
            tok = tok->next;
        }
        if (equal(tok, "(")) {
            error_tok(tok, "using には括弧を付けません");
        }
        u->all_next = ast->all_usings;
        ast->all_usings = u;
        tail = tail->next = u;
        u->vnext = scope;
        scope = u;
        if (!consume(",")) break;
    }
    expect_kind(TK_NEWLINE, "文の終わりには改行が必要です");
    return head.next;
}

static void import_list(Token *start)
{
    ModuleImport **tail = &ast->imports;
    while (*tail != NULL) tail = &(*tail)->next;
    for (;;) {
        ModuleImport *item = calloc(1, sizeof(ModuleImport));
        if (item == NULL) fatal("import の領域を確保できません");
        item->tok = start;
        item->name = name_path();
        *tail = item;
        tail = &item->next;
        if (!consume(",")) break;
    }
    expect_kind(TK_NEWLINE, "文の終わりには改行が必要です");
}

/*
 * block = NEWLINE INDENT stmt+ DEDENT
 * ブロックの中で宣言した変数は、ブロックの外からは見えない。
 */
static Node *block(const char *indent_msg)
{
    Node head;
    Node *cur;
    Var *saved;
    Using *saved_usings;

    expect_kind(TK_NEWLINE, "ブロックを始める ';' の直後は改行してください");
    expect_kind(TK_INDENT, indent_msg);

    saved = visible;
    saved_usings = local_usings;
    block_depth++;
    memset(&head, 0, sizeof(head));
    cur = &head;
    while (tok->kind != TK_DEDENT && tok->kind != TK_EOF) {
        cur = cur->next = stmt();
    }
    expect_kind(TK_DEDENT, "インデントが正しくありません");
    block_depth--;
    visible = saved;
    local_usings = saved_usings;
    return head.next;
}

/* "if" "(" expr ")" ";" block ("else" "if" ...)* ("else" ";" block)? */
static Node *if_stmt(Token *start)
{
    Node *node;
    Token *else_if;

    node = new_node(ND_IF, start);
    expect("(");
    node->cond = expr();
    expect(")");
    expect(";");
    node->then = block("if の本体はタブで1段インデントしてください");

    if (consume("else")) {
        if (equal(tok, "if")) {
            else_if = tok;
            tok = tok->next;
            node->els = if_stmt(else_if);
        } else {
            expect(";");
            node->els = block("else の本体はタブで1段インデントしてください");
        }
    }
    return node;
}

/* "while" "(" expr ")" ";" block */
static Node *while_stmt(Token *start)
{
    Node *node;

    node = new_node(ND_WHILE, start);
    expect("(");
    node->cond = expr();
    expect(")");
    expect(";");
    loop_depth++;
    node->then = block("while の本体はタブで1段インデントしてください");
    loop_depth--;
    return node;
}

/*
 * "for" "(" 開始値 比較 型? 変数 比較 終了値 "," 増分 ")" ";" block
 *
 * 開始側が < / > なら、開始値をヘッダーの更新で1回進めてから判定する。
 * 左右の比較と更新は同じ方向でなければならない。
 * ループ変数は for の中で新しく宣言する。型省略時は整数の開始値から32bit以上の型を選ぶ。
 * 条件「変数 比較 終了値」は毎周評価する（終了値も毎周評価される）。
 */
static Node *for_stmt(Token *start)
{
    Node *node;
    Node *start_value;
    Node *decl;
    Node *read;
    Node *cmp;
    Node *chain;
    Token *op1;
    Token *op2;
    Token *name;
    Var *saved;
    Var *v;
    Type ty;
    int increasing;
    int explicit_type;

    node = new_node(ND_FOR, start);
    expect("(");
    start_value = shift();

    /* 開始側の比較 */
    op1 = tok;
    if (!equal(op1, "<") && !equal(op1, "<=") && !equal(op1, ">") && !equal(op1, ">=")) {
        error_tok(op1, "for の範囲は「開始値 <= 変数 < 終了値」の形で書いてください");
    }
    increasing = equal(op1, "<") || equal(op1, "<=");
    node->for_open = equal(op1, "<") || equal(op1, ">");
    tok = tok->next;

    /* 省略時は一旦 int。型検査で整数の開始値を考慮して確定する。 */
    ty = type_keyword(tok);
    explicit_type = ty != TY_NONE;
    node->for_infer = !explicit_type;
    if (ty != TY_NONE) {
        skip_type();
    } else {
        ty = TY_INT;
    }
    if (tok->kind != TK_IDENT) {
        error_tok(tok, "ループ変数の名前が必要です");
    }
    name = tok;
    if (!explicit_type && find_var(name) != NULL)
        error_tok(name, "外側の変数と同名のループ変数を宣言する場合は、型を明示してください");
    tok = tok->next;

    /* 終了側の比較 */
    op2 = tok;
    if (!equal(op2, "<") && !equal(op2, "<=") && !equal(op2, ">") && !equal(op2, ">=")) {
        error_tok(op2, "for の終了側には <、<=、>、>= のいずれかを書いてください");
    }
    if (increasing != (equal(op2, "<") || equal(op2, "<="))) {
        error_tok(op2, "範囲の向きが合っていません（左右とも <・<=、または左右とも >・>= にしてください）");
    }
    tok = tok->next;

    /* ループ変数はここから for の終わりまで見える */
    saved = visible;
    block_depth++;                      /* 繰り返しの変数は for 文の中だけで見えるので、1段内側とみなす */
    v = declare_var(name, ty, 0);
    block_depth--;
    v->is_loop = 1;
    v->depth = block_depth + 1;
    decl = new_node(ND_DECL, name);
    decl->var = v;
    decl->lhs = start_value;

    read = new_node(ND_VAR, name);
    read->var = v;
    cmp = new_node(ND_CMP, op2);
    cmp->val = cmp_kind(op2);
    cmp->lhs = read;
    cmp->rhs = shift();
    chain = new_node(ND_CHAIN, op2);
    chain->lhs = cmp;

    /* 増分：ループ変数の ++・--・+=・-= のみ */
    expect(",");
    if (tok->kind != TK_IDENT || tok->len != name->len || strncmp(tok->loc, name->loc, (size_t)name->len) != 0 ||
        !(equal(tok->next, "++") || equal(tok->next, "--") || equal(tok->next, "+=") || equal(tok->next, "-="))) {
        error_tok(tok, "for の増分はループ変数 %.*s の ++、--、+= 式、-= 式 のいずれかで書いてください",
                 name->len, name->loc);
    }
    tok = tok->next;
    node->step = update(name, &node->inc, &node->inc_sign);
    expect(")");
    expect(";");

    node->init = decl;
    node->cond = chain;
    node->val = increasing ? 1 : -1;

    loop_depth++;
    node->then = block("for の本体はタブで1段インデントしてください");
    loop_depth--;
    visible = saved;
    return node;
}

/* "break" NEWLINE ・ "continue" NEWLINE */
static Node *loop_jump(NodeKind kind, Token *start)
{
    const char *name;

    name = kind == ND_BREAK ? "break" : "continue";
    if (loop_depth == 0) {
        error_tok(start, "%s はループ（for・while）の中でのみ使えます", name);
    }
    if (equal(tok, "(")) {
        error_tok(tok, "%s には括弧を付けません（00.21 から %s() ではなく %s と書きます）", name, name, name);
    }
    expect_kind(TK_NEWLINE, "文の終わりには改行が必要です");
    return new_node(kind, start);
}

static Node *match_stmt(Token *start)
{
    Node *node=new_node(ND_MATCH,start),**end=&node->then;Var *saved=visible;Using *saved_using=local_usings;
    expect("(");node->cond=expr();expect(")");expect(";");
    expect_kind(TK_NEWLINE,"match には改行が必要です");expect_kind(TK_INDENT,"match の case は1段インデントしてください");
    node->var=hidden_var(TY_NONE,start);
    while(tok->kind!=TK_DEDENT&&tok->kind!=TK_EOF){
        Token *site=tok;
        if(consume("case")){
            Node *arm=new_node(ND_CASE,site),**patterns=&arm->lhs;
            do{*patterns=expr();patterns=&(*patterns)->next;}while(consume(","));
            expect(";");arm->then=block("case の本体は1段インデントしてください");*end=arm;end=&arm->next;
        }else if(consume("else")){
            expect(";");node->els=block("else の本体は1段インデントしてください");
            if(tok->kind!=TK_DEDENT)error_tok(tok,"match の else は最後に1回だけ記述してください");
            break;
        }else error_tok(tok,"match の中には case または else を記述してください");
    }
    expect_kind(TK_DEDENT,"match の終わりが必要です");visible=saved;local_usings=saved_using;
    if(!node->then&&!node->els)error_tok(start,"match は空にできません");
    return node;
}

static Node *try_stmt(Token *start)
{
    Node *n=new_node(ND_TRY,start), **tail=&n->els;
    Var *saved, *binding=NULL;
    if(consume("(")) {
        Node *value;
        if(tok->kind==TK_IDENT && is_update_op(tok->next)) {
            Token *name=tok; tok=tok->next; n->lhs=update(name,NULL,NULL);
        } else {
            value=expr();
            if(consume("=")) { n->lhs=new_node(ND_STORE,value->tok);n->lhs->lhs=value;n->lhs->rhs=expr();n->lhs->val=UPDATE_SET; }
            else n->lhs=new_unary(ND_EXPR_STMT,value,start);
        }
        expect(")");
    }
    if(consume("as")) {
        Token *name=tok; if(is_keyword(tok))error_tok(tok,"エラーの名前に予約語は使えません");expect_kind(TK_IDENT,"エラーの名前が必要です");
        binding=hidden_var(named_type(name,"io.Error",local_usings),name);
        binding->name=name->loc;binding->name_len=name->len;binding->is_readonly=1;
    }
    n->var=binding?binding:hidden_var(named_type(start,"io.Error",local_usings),start);
    n->ref_temp=hidden_var(TY_ADDR,start);
    expect(";");n->then=block("try の本体は1段インデントしてください");
    while(consume("case")) {
        Node *arm=new_node(ND_CASE,start), **values=&arm->lhs;
        arm->tok=tok;expect("(");
        if(!consume(")")) {
            do {*values=expr();values=&(*values)->next;}while(consume(","));
            expect(")");
        }
        expect(";");saved=visible;
        if(binding){binding->vnext=visible;visible=binding;}
        arm->then=block("case の本体は1段インデントしてください");visible=saved;
        *tail=arm;tail=&arm->next;
    }
    if(consume("finally")){expect(";");n->rhs=block("finally の本体は1段インデントしてください");}
    if(!n->els&&!n->rhs)error_tok(start,"try には case または finally が必要です");
    return n;
}

/* stmt = return 文 | print 文 | if 文 | while 文 | for 文 | break() | continue() | const 定数 | 変数宣言 | 更新 */
static Node *stmt(void)
{
    Node *node;
    Token *start;
    Type ty;
    Using *u;
    Token *io_call;

    start = tok;
    if (tok->kind == TK_INDENT) {
        error_tok(tok, "余分なインデントがあります");
    }
    io_call = qualified_io_call(tok);
    if (io_call != NULL && equal(io_call, "print")) {
        char *name = name_path();
        node = print_stmt(start);
        node->name = name;
        return node;
    }
    if (consume("print")) {
        return print_stmt(start);
    }
    if (consume("return")) {
        node = new_node(ND_RETURN, start);
        if (tok->kind != TK_NEWLINE) {
            node->lhs = expr();         /* void の関数では値を書かない */
        }
        expect_kind(TK_NEWLINE, "文の終わりには改行が必要です");
        return node;
    }
    if (consume("try")) return try_stmt(start);
    if (consume("throw")) {
        node=new_node(ND_THROW,start);expect(";");
        expect_kind(TK_NEWLINE,"throw; の後は改行してください");return node;
    }
    if(equal(tok,"case")||equal(tok,"finally"))error_tok(tok,"対応する try がありません");
    if (consume("match")) return match_stmt(start);
    if (consume("if")) {
        return if_stmt(start);
    }
    if (consume("while")) {
        return while_stmt(start);
    }
    if (consume("for")) {
        return for_stmt(start);
    }
    if (consume("break")) {
        return loop_jump(ND_BREAK, start);
    }
    if (consume("continue")) {
        return loop_jump(ND_CONTINUE, start);
    }
    if (equal(tok, "else")) {
        error_tok(tok, "else に対応する if がありません");
    }
    if (consume("const")) {
        return declaration(start, 1);
    }
    if (equal(tok, "ref") && (equal(tok->next, "auto") || type_keyword(tok->next) != TY_NONE)) {
        tok = tok->next;
        return declaration_ref(start, 0, 1);
    }
    if (is_member(tok) && equal(tok->next->next, "addr") && equal(tok->next->next->next, "=")) {
        /* p.addr = 場所：ref の変数の指す先を変える */
        Token *name = tok;
        node = new_node(ND_ADDR_ASSIGN, name);
        node->var = find_var(name);
        if (node->var == NULL) {
            error_tok(name, "変数 %.*s は宣言されていません", name->len, name->loc);
        }
        tok = tok->next->next->next->next;
        node->lhs = expr();
        expect_kind(TK_NEWLINE, "文の終わりには改行が必要です");
        return node;
    }
    if (is_member(tok) && equal(tok->next->next, "value") && is_update_op(tok->next->next->next)) {
        /* p.value = 値・p.value += 値 など：p = 値 と同じ */
        Token *name = tok;
        tok = tok->next->next->next;
        node = update(name, NULL, NULL);
        expect_kind(TK_NEWLINE, "文の終わりには改行が必要です");
        return node;
    }
    if (consume("using")) {
        /* 関数の中の using：書いた位置から、そのブロックの終わりまで有効 */
        node = new_node(ND_USING, start);
        node->using_ref = using_list(start);
        for (u = node->using_ref; u != NULL; u = u->next) {
            u->vnext = local_usings;
            local_usings = u;
        }
        return node;
    }
    if (equal(tok, "auto")) return declaration(start, 0);
    ty = type_keyword(tok);
    if (ty != TY_NONE && !(ty == TY_FILE && equal(tok->next, "."))) {
        return declaration(start, 0);
    }
    if (tok->kind == TK_IDENT && !is_keyword(tok) && is_update_op(tok->next)) {
        return assignment();
    }
    if (equal(tok, "input") && equal(tok->next, "(")) {
        node = new_node(ND_EXPR_STMT, start);
        node->lhs = expr();
        if (node->lhs->kind != ND_INPUT) {
            error_tok(start, "文が必要です（input の結果を使う式は、変数への代入や if の条件に書いてください）");
        }
        node->lhs->input_stmt = 1;
        expect_kind(TK_NEWLINE, "文の終わりには改行が必要です");
        return node;
    }
    if ((tok->kind == TK_IDENT && (!is_keyword(tok) || (equal(tok, "file") && equal(tok->next, ".")))) || equal(tok, "(")) {
        Node *target = expr();
        if (is_update_op(tok)) {
            node = new_node(ND_STORE, tok);
            node->lhs = target;
            if (consume("=")) node->val = UPDATE_SET;
            else if (consume("+=")) node->val = UPDATE_ADD;
            else if (consume("-=")) node->val = UPDATE_SUB;
            else if (consume("++")) node->val = UPDATE_INC;
            else { expect("--"); node->val = UPDATE_DEC; }
            node->rhs = node->val == UPDATE_INC || node->val == UPDATE_DEC ? new_num(1, node->tok) : expr();
        } else {
            if (target->kind != ND_CALL && target->kind != ND_METHOD && target->kind != ND_MODEL_METHOD && target->kind != ND_INPUT) {
                error_tok(target->tok, "この式は代入・print・条件などで使ってください");
            }
            if (target->kind == ND_INPUT) target->input_stmt = 1;
            node = new_unary(ND_EXPR_STMT, target, start);
        }
        expect_kind(TK_NEWLINE, "文の終わりには改行が必要です");
        return node;
    }
    error_tok(tok, "文が必要です（return・print・if・while・for・break・continue・const・using・変数の宣言と更新・関数呼び出しに対応しています）");
    return NULL;
}

/* 戻り値の型（void を含む）。型名でなければ TY_NONE */
static Type return_type(Token *t)
{
    if (equal(t, "void")) {
        return TY_VOID;
    }
    return type_keyword(t);
}

/*
 * function = rtype ident "(" params? ")" (";" block | "=" expr NEWLINE)
 * 「= 式」の形（数式形式）は、本体が return 式 だけのブロックと同じ意味になる。
 * 引数は呼び出し側が左から順に積むので、i 番目（0 始まり）は [rbp + 16 + 8 * (n - 1 - i)] にある。
 */
static Function *function_def(int is_readonly)
{
    Function *fn;
    Var head;
    Var *cur;
    Var *v;
    Var *p;
    Token *name;
    Token *start;
    Type ty;
    int i;
    int count;
    int omitted = tok->kind == TK_IDENT && !is_keyword(tok) && equal(tok->next, "(");

    fn = calloc(1, sizeof(Function));
    if (fn == NULL) {
        fatal("メモリを確保できません");
    }
    fn->is_readonly = is_readonly;
    fn->infer_return = omitted || equal(tok, "auto");
    fn->ret = fn->infer_return ? TY_NONE : return_type(tok);
    if (fn->ret == TY_NONE && !fn->infer_return) {
        error_tok(tok, "ファイルの先頭に書けるのは const 定数と関数の定義だけです（関数は 戻り値の型 名前(引数); の形で書きます）");
    }
    if (!omitted) skip_type();
    if (tok->kind != TK_IDENT || is_keyword(tok)) {
        error_tok(tok, "関数名が必要です");
    }
    name = tok;
    fn->model = parsing_model;
    fn->contract = parsing_interface;
    if (parsing_interface && fn->infer_return)
        error_tok(name, "interface の契約には戻り値の型を明示してください");
    if (parsing_interface && equal(name, "init")) error_tok(name, "interface に init は宣言できません");
    fn->is_initializer = parsing_model && equal(name, "init");
    if (fn->is_initializer && (is_readonly || (fn->ret != TY_VOID && fn->ret != TY_BOOL)))
        error_tok(name, "init は public void / bool のブロック形式で定義してください");
    fn->name = name->loc;
    fn->name_len = name->len;
    fn->tok = name;
    tok = tok->next;

    cur_fn = fn;
    visible = NULL;
    loop_depth = 0;
    block_depth = parsing_model ? 1 : 0;
    local_usings = NULL;

    /* 引数 */
    expect("(");
    memset(&head, 0, sizeof(head));
    cur = &head;
    if (parsing_model || parsing_interface) {
        Token *self = calloc(1, sizeof(*self));
        if (!self) fatal("this を確保できません");
        *self = *name; self->loc = "this"; self->len = 4;
        v = new_var(self, parsing_model ? parsing_model->ty : parsing_interface->ty); v->is_param = 1; v->is_ref = 1; v->is_readonly = is_readonly;
        cur = cur->next = v; fn->param_count++;
    }
    if ((equal(tok,"void")||equal(tok,"Void"))&&equal(tok->next,")")) tok=tok->next;
    if (!equal(tok, ")")) {
        for (;;) {
            int param_ref = 0, param_readonly = 0, param_infer, param_omitted;
            if (consume("ref")) {
                param_ref = 1;
                param_readonly = consume("readonly");
            } else {
                consume("val");         /* val は既定（値渡し）なので、書いても書かなくてもよい */
            }
            param_omitted = tok->kind == TK_IDENT && !is_keyword(tok) &&
                (equal(tok->next, ",") || equal(tok->next, ")") || equal(tok->next, "["));
            param_infer = param_omitted || equal(tok, "auto");
            if (param_infer && (parsing_interface || fn->is_initializer))
                error_tok(tok, "interface の契約と init の引数には型を明示してください");
            ty = param_infer ? TY_NONE : type_keyword(tok);
            if (!param_infer && ty == TY_NONE) {
                error_tok(tok, "引数の型（int・float・double・bool・str・char）が必要です");
            }
            if (!param_omitted) skip_type();
            if (tok->kind != TK_IDENT) {
                error_tok(tok, "引数の名前が必要です");
            }
            for (p = head.next; p != NULL; p = p->next) {
                if (same_name(p, tok)) {
                    error_tok(tok, "引数 %.*s が重複しています", tok->len, tok->loc);
                }
            }
            v = new_var(tok, ty);
            v->is_param = 1;
            v->is_ref = param_ref;
            v->is_readonly = param_readonly;
            v->is_inferred = param_infer;
            if (param_infer) fn->is_generic = fn->is_template = 1;
            cur = cur->next = v;
            fn->param_count++;
            tok = tok->next;
            if (param_infer && equal(tok, "["))
                error_tok(tok, "推論する引数は配列の次元も推論します。名前の後の [] は省略してください");
            v->dims = dimensions(&v->ty);
            if (consume("=")) {
                if (!fn->is_initializer) error_tok(v->tok, "既定引数は init にだけ指定できます");
                v->default_arg = expr();
            }
            if (fn->is_initializer && v->is_ref) error_tok(v->tok, "init の引数は値引数にしてください");
            if (!consume(",")) {
                break;
            }
        }
    }
    expect(")");
    fn->params = head.next;
    if (fn->infer_return && equal(tok, "["))
        error_tok(tok, "戻り値の推論では配列の次元も推論します。関数名の後の [] は省略してください");
    fn->return_dims = dimensions(&fn->ret);
    if (fn->is_initializer && fn->return_dims) error_tok(name, "init の戻り値は void / bool のみです。配列は返せません");
    i = 0;
    for (v = fn->params; v != NULL; v = v->next) {
        v->offset = -(16 + 8 * (fn->param_count - 1 - i));
        v->vnext = visible;
        visible = v;
        i++;
    }

    if (parsing_interface) {
        expect(";");
        expect_kind(TK_NEWLINE, "interface のメソッド宣言の終わりには改行が必要です");
        cur_fn = NULL; visible = NULL; return fn;
    }

    /* 本体：数式形式（= 式）またはブロック */
    start = tok;
    if (consume("=")) {
        if (fn->is_initializer) error_tok(start, "init の本体はブロック形式にしてください");
        fn->is_math_form = 1;
        fn->body = new_node(ND_RETURN, start);
        fn->body->lhs = expr();
        expect_kind(TK_NEWLINE, "数式の形の関数定義は1行で書いてください");
    } else {
        if (!equal(tok, ";")) {
            error_tok(tok, "関数の本体は「;」の後の行に書くか、「= 式」の形で書いてください");
        }
        tok = tok->next;
        fn->body = block("関数の本体はタブで1段インデントしてください");
    }

    /* スタックに置くのは const 以外のローカル変数だけ（引数は呼び出し側が積む） */
    count = 0;
    for (v = fn->locals; v != NULL; v = v->next) {
        if (!v->is_const) {
            count++;
        }
    }
    fn->stack_size = (count * 8 + 15) / 16 * 16;
    cur_fn = NULL;
    visible = NULL;
    return fn;
}

static EnumDef *enum_def(void)
{
    EnumDef *e=calloc(1,sizeof(*e));EnumMember **end;Token *name;
    if(!e)fatal("enum を確保できません");
    expect("enum");name=tok;
    if(tok->kind!=TK_IDENT||is_keyword(tok))error_tok(tok,"enum の型名が必要です");
    e->tok=tok;e->name=calloc((size_t)tok->len+1,1);if(!e->name)fatal("enum 名を確保できません");
    memcpy(e->name,tok->loc,(size_t)tok->len);e->ty=enum_type(e);tok=tok->next;
    expect(";");expect_kind(TK_NEWLINE,"enum 定義には改行が必要です");
    expect_kind(TK_INDENT,"enum の中にはメンバーを1段インデントしてください");end=&e->members;
    while(tok->kind!=TK_DEDENT&&tok->kind!=TK_EOF){
        EnumMember *m=calloc(1,sizeof(*m));EnumMember *old;
        if(!m)fatal("enum メンバーを確保できません");
        if(tok->kind!=TK_IDENT||is_keyword(tok))error_tok(tok,"enum のメンバー名には予約語以外の識別子が必要です");
        m->owner=e;m->tok=tok;m->index=e->count++;m->name=calloc((size_t)tok->len+1,1);
        if(!m->name)fatal("enum メンバー名を確保できません");
        memcpy(m->name,tok->loc,(size_t)tok->len);
        for(old=e->members;old;old=old->next)if(!strcmp(old->name,m->name))error_tok(tok,"enum のメンバー名 %s が重複しています",m->name);
        tok=tok->next;if(consume("="))m->initial=expr();
        m->symbol=calloc(1,sizeof(*m->symbol));if(!m->symbol)fatal("enum の定数を確保できません");
        m->symbol->name=m->name;m->symbol->name_len=(int)strlen(m->name);m->symbol->ty=e->ty;
        m->symbol->is_const=1;m->symbol->is_global=1;m->symbol->tok=m->tok;m->symbol->enum_member=m;
        *end=m;end=&m->next;expect_kind(TK_NEWLINE,"enum の各メンバーには改行が必要です");
    }
    expect_kind(TK_DEDENT,"enum 定義の終わりが必要です");
    if(!e->members)error_tok(name,"enum は空にできません");
    return e;
}

static Model *model_def(Function **tail)
{
    Model *m = calloc(1, sizeof(*m)); ModelField **fields;
    Token *start;
    if (!m) fatal("モデルを確保できません");
    expect("model"); start = tok;
    if (tok->kind != TK_IDENT || is_keyword(tok)) error_tok(tok, "モデル名が必要です");
    m->tok = tok; m->name = calloc((size_t)tok->len + 1, 1);
    if (!m->name) fatal("モデル名を確保できません");
    memcpy(m->name, tok->loc, (size_t)tok->len); m->ty = model_type(m);
    tok = tok->next;
    if (consume("implements")) {
        Implementation **end = &m->implements;
        do {
            Implementation *im = calloc(1, sizeof(*im));
            if (!im) fatal("implements を確保できません");
            im->tok = tok;
            if (tok->kind != TK_IDENT || is_keyword(tok)) error_tok(tok, "implements には interface の型名が必要です");
            { char *path = name_path(); im->ty = named_type(im->tok, path, NULL); free(path); }
            *end = im; end = &im->next;
        } while (consume(","));
    }
    expect(";");
    expect_kind(TK_NEWLINE, "モデル定義には改行が必要です");
    expect_kind(TK_INDENT, "モデルの中はタブで1段インデントしてください");
    fields = &m->fields;
    parsing_model = m;
    while (tok->kind != TK_DEDENT && tok->kind != TK_EOF) {
        Type ty; Token *name; int is_private = 0, is_readonly, inferred;
        if (equal(tok, "public") || equal(tok, "private")) {
            is_private = equal(tok, "private"); tok = tok->next;
            if (equal(tok, "public") || equal(tok, "private"))
                error_tok(tok, "公開指定 public / private は1つだけ書いてください");
        }
        is_readonly = consume("readonly");
        if (equal(tok, "readonly") || equal(tok, "public") || equal(tok, "private"))
            error_tok(tok, "公開指定の後に readonly を1つだけ書いてください");
        ty = return_type(tok); name = tok;
        inferred = equal(tok, "auto") || (tok->kind == TK_IDENT && !is_keyword(tok) && equal(tok->next, "("));
        if (!inferred) {
            if (ty == TY_NONE) error_tok(tok, "モデルにはフィールドまたはメソッドを定義してください");
            skip_type();
            if (tok->kind != TK_IDENT || is_keyword(tok)) error_tok(tok, "メンバー名が必要です");
        } else if (equal(tok, "auto") && !(tok->next->kind == TK_IDENT && equal(tok->next->next, "("))) {
            error_tok(tok, "モデルの auto はメソッドの戻り値に使用してください。フィールドの型は明示してください");
        }
        if (inferred || equal(tok->next, "(")) {
            tok = name;
            (*tail)->next = function_def(is_readonly); *tail = (*tail)->next;
            (*tail)->is_private = is_private;
            if ((*tail)->is_initializer) {
                if (is_private) error_tok((*tail)->tok, "init は public で定義してください");
                if (m->initializer) error_tok((*tail)->tok, "init の定義が重複しています");
                m->initializer = *tail;
            }
        } else {
            ModelField *f = calloc(1, sizeof(*f));
            if (is_readonly) error_tok(name, "readonly はメソッドと ref 引数に指定してください");
            if (!f) fatal("フィールドを確保できません");
            if (equal(tok, "init")) error_tok(tok, "init はモデル生成専用の名前です");
            if (equal(tok, "this") || equal(tok, "addr") || equal(tok, "value") || equal(tok, "length"))
                error_tok(tok, "this・addr・value・length はモデルのフィールド名に使えません");
            f->model = m; f->is_private = is_private;
            f->tok = tok; f->name = calloc((size_t)tok->len + 1, 1);
            if (!f->name) fatal("フィールド名を確保できません");
            memcpy(f->name, tok->loc, (size_t)tok->len); f->ty = ty; f->index = m->field_count++;
            tok = tok->next; f->dims = dimensions(&f->ty);
            cur_fn = NULL; visible = NULL; local_usings = NULL;
            if (consume("=")) f->initial = expr();
            expect_kind(TK_NEWLINE, "フィールド宣言の終わりには改行が必要です");
            *fields = f; fields = &f->next;
        }
    }
    expect_kind(TK_DEDENT, "モデル定義を閉じてください");
    parsing_model = NULL; (void)start; return m;
}

static Interface *interface_def(void)
{
    Interface *in = calloc(1, sizeof(*in)); Function **end;
    if (!in) fatal("interface を確保できません");
    expect("interface");
    if (tok->kind != TK_IDENT || is_keyword(tok)) error_tok(tok, "interface 名が必要です");
    in->tok = tok; in->name = calloc((size_t)tok->len + 1, 1);
    if (!in->name) fatal("interface 名を確保できません");
    memcpy(in->name, tok->loc, (size_t)tok->len); in->ty = interface_type(in);
    tok = tok->next; expect(";");
    expect_kind(TK_NEWLINE, "interface 定義には改行が必要です");
    expect_kind(TK_INDENT, "interface には1つ以上のメソッド宣言が必要です");
    parsing_interface = in; end = &in->methods;
    while (tok->kind != TK_DEDENT && tok->kind != TK_EOF) {
        int ro;
        if (equal(tok, "private")) error_tok(tok, "interface のメソッドは public にしてください");
        consume("public"); ro = consume("readonly");
        *end = function_def(ro); end = &(*end)->next;
    }
    expect_kind(TK_DEDENT, "interface 定義を閉じてください");
    if (!in->methods) error_tok(in->tok, "空の interface は定義できません");
    parsing_interface = NULL; return in;
}

/* program = (const_decl | function)* EOF */
Ast *parse(Token *t)
{
    Node head;
    Node *cur;
    Function fhead;
    Function *fcur;
    Using uhead;
    Using *ucur;
    Token *start;

    ast = calloc(1, sizeof(Ast));
    if (ast == NULL) {
        fatal("メモリを確保できません");
    }
    tok = t;
    cur_fn = NULL;
    visible = NULL;
    loop_depth = 0;

    memset(&head, 0, sizeof(head));
    cur = &head;
    memset(&fhead, 0, sizeof(fhead));
    fcur = &fhead;
    memset(&uhead, 0, sizeof(uhead));
    ucur = &uhead;
    local_usings = NULL;
    parsing_model = NULL; parsing_interface = NULL;
    while (tok->kind != TK_EOF) {
        int is_private=0;Token *visibility=NULL;
        if(equal(tok,"public")||equal(tok,"private")){
            visibility=tok;is_private=equal(tok,"private");tok=tok->next;
            if(equal(tok,"public")||equal(tok,"private"))error_tok(tok,"公開指定 public / private は1つだけ書いてください");
            if(equal(tok,"import")||equal(tok,"using"))error_tok(visibility,"import / using に公開指定は付けられません");
        }
        if (tok->kind == TK_INDENT) {
            error_tok(tok, "余分なインデントがあります");
        }
        if (equal(tok, "enum")) {
            EnumDef *e=enum_def(),**end=&ast->enums;e->is_private=is_private;
            while(*end)end=&(*end)->next;
            *end=e;continue;
        }
        if (equal(tok, "interface")) {
            Interface *in = interface_def(), **end = &ast->interfaces;
            in->is_private=is_private;
            while (*end) end = &(*end)->next;
            *end = in; continue;
        }
        if (equal(tok, "model")) {
            Model *m = model_def(&fcur); Model **end = &ast->models;
            m->is_private=is_private;
            while (*end) end = &(*end)->next;
            *end = m; continue;
        }
        if (equal(tok, "import")) {
            start = tok;
            tok = tok->next;
            import_list(start);
            continue;
        }
        if (equal(tok, "const")) {
            start = tok;
            tok = tok->next;
            cur = cur->next = declaration(start, 1);
            cur->var->is_private=is_private;
            continue;
        }
        if (equal(tok, "using")) {
            start = tok;
            tok = tok->next;
            ucur->next = using_list(start);
            while (ucur->next != NULL) {
                ucur = ucur->next;
            }
            continue;
        }
        fcur = fcur->next = function_def(0);
        fcur->is_private=is_private;
    }
    ast->globals = head.next;
    ast->funcs = fhead.next;
    ast->usings = uhead.next;
    return ast;
}
