/*
 * 文法（現時点）
 *
 * program  = (import | const_decl | using | function)* EOF
 * import   = "import" path ("," path)* NEWLINE
 * using    = "using" path ("as" ident)? ("," path ("as" ident)?)* NEWLINE
 * path     = ident ("." ident)*
 * function = rtype ident "(" params? ")" dimensions (";" block | "=" expr NEWLINE)
 * rtype    = type | "void"
 * params   = ("ref" | "val")? type ident dimensions ("," params)?
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
 *          | type ident dimensions ("=" expr)? NEWLINE
 *          | postfix update NEWLINE | postfix（配列操作）NEWLINE
 * update   = "=" expr | "+=" expr | "-=" expr | "++" | "--"
 * const_decl = "const" type ident "=" expr NEWLINE
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
static Using *local_usings; /* 関数の中で今見えている using */

static Node *expr(void);
static Node *string_literal(Token *t);
static Node *stmt(void);

/* 変数名などに使えない予約語 */
static const char *keywords[] = {
    "int", "float", "double", "bool", "str", "char", "void", "file", "const",
    "true", "false", "return", "print", "if", "else", "for", "while",
    "break", "continue", "and", "or", "not", "struct", "enum", "import", "of", "using", "input",
    "ref", "val", "none", "as",
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
    return TY_NONE;
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
        if (*ty == TY_VOID) {
            error_tok(dim->tok, "void の配列は作れません");
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
 * 変数を宣言する。外側のブロックと同じ名前（隠す形の宣言）も禁止する。
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
    if (prev != NULL) {
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
            if (part->kind != ND_VAR && part->kind != ND_INDEX) {
                error_tok(part->tok, "input の {} の中には、読み込む先の変数または配列要素を書いてください");
            }
            if (cur != NULL && cur->sep_len == 0) {
                error_tok(part->tok, "{} と {} の間には、区切りの文字が必要です（例：{a}, {b}）");
            }
            cur = calloc(1, sizeof(InputField));
            if (cur == NULL) {
                fatal("メモリを確保できません");
            }
            cur->var = part;
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
        if (node->lhs->kind == ND_VAR || node->lhs->kind == ND_INDEX) {
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
    node->in_cur = hidden_var(TY_INT, start);
    node->in_end = hidden_var(TY_INT, start);
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
            if (consume("ref")) {
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
        node = expr();
        expect(")");
        return node;
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
    if (tok->kind == TK_NUM) {
        node = new_num(tok->val, tok);
        tok = tok->next;
        return node;
    }
    if (tok->kind == TK_FNUM) {
        node = new_node(ND_FNUM, tok);
        node->fval = tok->fval;
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
    if ((equal(tok, "int") || equal(tok, "float") || equal(tok, "double") ||
         equal(tok, "char") || equal(tok, "str")) && equal(tok->next, "(")) {
        /* 型変換 int(x)・float(x)・double(x) */
        node = new_node(ND_BUILTIN, tok);
        node->val = builtin_find(NULL, tok);
        tok = tok->next->next;
        call_args(node);
        return node;
    }
    if (tok->kind == TK_IDENT && (!is_keyword(tok) || (equal(tok, "file") && equal(tok->next, ".")))) {
        start = tok;
        v = find_var(start);
        if (v != NULL && !equal(start->next, "(")) {
            node = new_node(ND_VAR, start);
            node->var = v;
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
                    error_tok(field, "配列の操作は add・remove_at・clear です");
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
                variable_member = 1;
            } else {
                error_tok(field, "使える要素は length・value・addr です");
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
    if (consume("-")) {
        return new_unary(ND_NEG, unary(), start);
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

    first = add();
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
        right = add();
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

/* or = and ("or" and)* */
static Node *logic_or(void)
{
    Node *node;
    Token *start;

    node = logic_and();
    for (;;) {
        start = tok;
        if (consume("or")) {
            node = new_binary(ND_OR, node, logic_and(), start);
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

    if (memchr(open + 1, ':', (size_t)(close - open - 1)) != NULL) {
        error_at(t->line, col_in(t, open), "書式指定（{x:.1} など）はまだ対応していません");
    }
    line_start = t->loc - (t->col - 1);
    saved = tok;
    tok = tokenize_fragment(open + 1, close, t->line, line_start);
    if (tok->kind == TK_EOF) {
        error_at(t->line, col_in(t, open), "{} の中に式を書いてください（{ そのものは \\{ と書きます）");
    }
    node = expr();
    if (tok->kind != TK_EOF) {
        error_tok(tok, "埋め込みの式が正しくありません");
    }
    tok = saved;
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
            close = p + 1;
            while (close < end && *close != '}') {
                close++;
            }
            if (close >= end) {
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

    ty = type_keyword(tok);
    if (ty == TY_NONE) {
        error_tok(tok, "型（int・float・double・bool・str・char）が必要です");
    }
    tok = tok->next;
    if (tok->kind != TK_IDENT) {
        error_tok(tok, "%sが必要です", is_const ? "定数名" : "変数名");
    }
    name = tok;
    tok = tok->next;

    dims = dimensions(&ty);
    node = new_node(ND_DECL, start);
    if (consume("=")) {
        node->lhs = expr();
    } else if (is_const) {
        error_tok(tok, "const には初期値が必要です（例：const float PI = 3.141592）");
    }
    /* 初期値の式の後で登録する（int a = a のような自己参照を防ぐ） */
    node->var = declare_var(name, ty, is_const);
    node->var->dims = dims;
    node->var->decl = node;
    node->var->is_ref = is_ref;
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
    node = update(name, NULL, NULL);
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
 * ループ変数は for の中で新しく宣言し、型を省略すると int になる。
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

    node = new_node(ND_FOR, start);
    expect("(");
    start_value = add();

    /* 開始側の比較 */
    op1 = tok;
    if (!equal(op1, "<") && !equal(op1, "<=") && !equal(op1, ">") && !equal(op1, ">=")) {
        error_tok(op1, "for の範囲は「開始値 <= 変数 < 終了値」の形で書いてください");
    }
    increasing = equal(op1, "<") || equal(op1, "<=");
    node->for_open = equal(op1, "<") || equal(op1, ">");
    tok = tok->next;

    /* ループ変数（型を省略すると int） */
    ty = type_keyword(tok);
    if (ty != TY_NONE) {
        tok = tok->next;
    } else {
        ty = TY_INT;
    }
    if (tok->kind != TK_IDENT) {
        error_tok(tok, "ループ変数の名前が必要です");
    }
    name = tok;
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
    cmp->rhs = add();
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
    if (equal(tok, "ref") && type_keyword(tok->next) != TY_NONE) {
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
            if (target->kind != ND_CALL && target->kind != ND_METHOD && target->kind != ND_INPUT) {
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
static Function *function_def(void)
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

    fn = calloc(1, sizeof(Function));
    if (fn == NULL) {
        fatal("メモリを確保できません");
    }
    fn->ret = return_type(tok);
    if (fn->ret == TY_NONE) {
        error_tok(tok, "ファイルの先頭に書けるのは const 定数と関数の定義だけです（関数は 戻り値の型 名前(引数); の形で書きます）");
    }
    tok = tok->next;
    if (tok->kind != TK_IDENT || is_keyword(tok)) {
        error_tok(tok, "関数名が必要です");
    }
    name = tok;
    fn->name = name->loc;
    fn->name_len = name->len;
    fn->tok = name;
    tok = tok->next;

    cur_fn = fn;
    visible = NULL;
    loop_depth = 0;
    block_depth = 0;
    local_usings = NULL;

    /* 引数 */
    expect("(");
    memset(&head, 0, sizeof(head));
    cur = &head;
    if (!equal(tok, ")")) {
        for (;;) {
            int param_ref = 0;
            if (consume("ref")) {
                param_ref = 1;
            } else {
                consume("val");         /* val は既定（値渡し）なので、書いても書かなくてもよい */
            }
            ty = type_keyword(tok);
            if (ty == TY_NONE) {
                error_tok(tok, "引数の型（int・float・double・bool・str・char）が必要です");
            }
            tok = tok->next;
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
            cur = cur->next = v;
            fn->param_count++;
            tok = tok->next;
            v->dims = dimensions(&v->ty);
            if (!consume(",")) {
                break;
            }
        }
    }
    expect(")");
    fn->params = head.next;
    fn->return_dims = dimensions(&fn->ret);
    i = 0;
    for (v = fn->params; v != NULL; v = v->next) {
        v->offset = -(16 + 8 * (fn->param_count - 1 - i));
        v->vnext = visible;
        visible = v;
        i++;
    }

    /* 本体：数式形式（= 式）またはブロック */
    start = tok;
    if (consume("=")) {
        if (fn->ret == TY_VOID) {
            error_tok(start, "void の関数は数式の形（= 式）では書けません");
        }
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
    while (tok->kind != TK_EOF) {
        if (tok->kind == TK_INDENT) {
            error_tok(tok, "余分なインデントがあります");
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
        fcur = fcur->next = function_def();
    }
    ast->globals = head.next;
    ast->funcs = fhead.next;
    ast->usings = uhead.next;
    return ast;
}
