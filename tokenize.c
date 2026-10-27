#include "qyc.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static Token *new_token(TokenKind kind, const char *start, int len, int line, int col)
{
    Token *tok;

    tok = calloc(1, sizeof(Token));
    if (tok == NULL) {
        fatal("メモリを確保できません");
    }
    tok->kind = kind;
    tok->loc = start;
    tok->len = len;
    tok->line = line;
    tok->col = col;
    tok->source = current_file;
    return tok;
}

static int col_of(const char *p, const char *line_start)
{
    return (int)(p - line_start) + 1;
}

static int is_ident_start(char c)
{
    return isalpha((unsigned char)c) || c == '_';
}

static int is_ident_char(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

/* 空行、またはコメントだけの行なら 1 */
static int is_blank_line(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\r') {
        p++;
    }
    if (*p == '\n' || *p == '\0') {
        return 1;
    }
    return p[0] == '/' && p[1] == '/';
}

/* 行末（改行の手前）まで進める */
static const char *skip_to_eol(const char *p)
{
    while (*p != '\n' && *p != '\0') {
        p++;
    }
    return p;
}

/* 2文字の記号（1文字の記号より先に調べる） */
static const char *punct2[] = { "==", "!=", "<=", ">=", "++", "--", "+=", "-=", "<<", ">>", NULL };

/*
 * 整数・識別子・記号を1つ読み、トークンを *cur の後ろに追加する。
 * 読めた場合は次の位置を、どれにも当てはまらない場合は NULL を返す。
 */
static const char *lex_basic(const char *p, int line, const char *line_start, Token **cur)
{
    const char *start;
    const char *q;
    char num_buf[400];
    double fval;
    int i;

    /* 小数リテラル（数字 . 数字） */
    if (isdigit((unsigned char)*p) && !(p[0] == '0' && integer_prefix((unsigned char)p[1]))) {
        start = p;
        q = p;
        while (isdigit((unsigned char)*q)) {
            q++;
        }
        if (q[0] == '.' && isdigit((unsigned char)q[1])) {
            q++;
            while (isdigit((unsigned char)*q)) {
                q++;
            }
            if (is_ident_char(*q) || *q == '.') {
                error_at(line, col_of(q, line_start), "小数の直後に英字や _ や . は書けません");
            }
            if ((size_t)(q - start) >= sizeof(num_buf)) {
                error_at(line, col_of(start, line_start), "小数の桁が多すぎます");
            }
            memcpy(num_buf, start, (size_t)(q - start));
            num_buf[q - start] = '\0';
            errno = 0;
            fval = strtod(num_buf, NULL);
            if (errno == ERANGE) {
                error_at(line, col_of(start, line_start), "小数が大きすぎるか小さすぎます");
            }
            *cur = (*cur)->next = new_token(TK_FNUM, start, (int)(q - start), line, col_of(start, line_start));
            (*cur)->fval = fval;
            return q;
        }
    }

    /* 整数リテラル */
    if (isdigit((unsigned char)*p)) {
        start = p;
        { uint64_t magnitude = 0, limit; int base = 10, digit, prefixed = 0, decimal_min;
          if (p[0] == '0' && (base = integer_prefix((unsigned char)p[1]))) { p += 2; prefixed = 1; }
          else base = 10;
          limit = base == 10 ? (UINT64_C(1) << 63) : UINT64_MAX;
          q = p;
          while (is_ident_char(*p)) {
              digit = integer_digit((unsigned char)*p);
              if (digit < 0 || digit >= base) error_at(line, col_of(p, line_start), "整数の基数に合わない数字・文字です");
              if (magnitude > (limit - (unsigned)digit) / (unsigned)base) error_at(line, col_of(start, line_start), "整数が大きすぎます");
              magnitude = magnitude * (unsigned)base + (unsigned)digit; p++;
          }
          if (p == q) error_at(line, col_of(start, line_start), "整数の接頭辞の後に数字が必要です");
          if (prefixed && *p == '.' && isdigit((unsigned char)p[1])) error_at(line, col_of(p, line_start), "基数指定は整数にだけ使えます");
          decimal_min = base == 10 && magnitude == (UINT64_C(1) << 63);
          *cur = (*cur)->next = new_token(TK_NUM, start, (int)(p - start), line, col_of(start, line_start));
          (*cur)->val = integer_bits(magnitude); (*cur)->decimal_min = decimal_min;
          return p;
        }
    }

    /* 識別子・キーワード */
    if (is_ident_start(*p)) {
        start = p;
        while (is_ident_char(*p)) {
            p++;
        }
        *cur = (*cur)->next = new_token(TK_IDENT, start, (int)(p - start), line, col_of(start, line_start));
        return p;
    }

    /* && は 00.21 で廃止（|| は縦棒2本として読む） */
    if (p[0] == '&') {
        error_at(line, col_of(p, line_start), "&& は廃止されました。論理積には and を使ってください");
    }

    /* 2文字の記号 */
    for (i = 0; punct2[i] != NULL; i++) {
        if (strncmp(p, punct2[i], 2) == 0) {
            *cur = (*cur)->next = new_token(TK_PUNCT, p, 2, line, col_of(p, line_start));
            return p + 2;
        }
    }

    /* 1文字の記号 */
    if (*p != '\0' && strchr("+-*/%^();=<>!,|.[]{}", *p) != NULL) {
        *cur = (*cur)->next = new_token(TK_PUNCT, p, 1, line, col_of(p, line_start));
        return p + 1;
    }

    return NULL;
}

const char *interpolation_close(const char *open, const char *end)
{
    const char *p; int depth = 1; char quote = 0;
    for (p = open + 1; p < end; p++) {
        if (*p == '\\' && p + 1 < end) { p++; continue; }
        if (quote) { if (*p == quote) quote = 0; continue; }
        if (*p == '\'' || *p == '"') { quote = *p; continue; }
        if (*p == '{') depth++;
        if (*p == '}' && --depth == 0) return p;
    }
    return NULL;
}

/* 文字列リテラルの終わり（閉じ引用符の次）まで進める。文字列は1行で閉じる */
static const char *skip_string(const char *p, int line, const char *line_start)
{
    const char *start;
    char quote;

    start = p;
    quote = *p;
    p++;
    while (*p != quote) {
        if (*p == '\0' || *p == '\n' || *p == '\r') {
            error_at(line, col_of(start, line_start), "文字列が閉じられていません（文字列は1行で書いてください）");
        }
        if (*p == '{') {
            const char *line_end = p, *close;
            while (*line_end && *line_end != '\n' && *line_end != '\r') line_end++;
            close = interpolation_close(p, line_end);
            if (close) { p = close + 1; continue; }
        }
        if (*p == '\\' && p[1] != '\0' && p[1] != '\n' && p[1] != '\r') {
            p++;
        }
        p++;
    }
    return p + 1;
}

/*
 * 文字列の {式} の中身（start〜end の手前）を字句解析する。
 * line・line_start は元のソースの位置で、エラー表示に使う。
 */
Token *tokenize_fragment(const char *start, const char *end, int line, const char *line_start)
{
    Token head;
    Token *cur;
    const char *p;
    const char *next;

    memset(&head, 0, sizeof(head));
    cur = &head;
    p = start;
    while (p < end) {
        if (*p == ' ' || *p == '\t') {
            p++;
            continue;
        }
        if (*p == '"' || *p == '\'') {
            next = skip_string(p, line, line_start);
            if (next > end) error_at(line, col_of(p, line_start), "埋め込みの文字列が閉じられていません");
            cur = cur->next = new_token(TK_STR, p, (int)(next - p), line, col_of(p, line_start));
            p = next; continue;
        }
        next = lex_basic(p, line, line_start, &cur);
        if (next == NULL || next > end) {
            error_at(line, col_of(p, line_start), "埋め込みの式に使えない文字です");
        }
        p = next;
    }
    cur = cur->next = new_token(TK_EOF, end, 0, line, col_of(end, line_start));
    return head.next;
}

Token *tokenize(const char *src)
{
    Token head;
    Token *cur;
    const char *p;
    const char *line_start;
    const char *start;
    const char *next;
    int line;
    int indent;
    int depth;
    int at_line_start;
    int brackets = 0;

    memset(&head, 0, sizeof(head));
    cur = &head;
    p = src;
    line_start = src;
    line = 1;
    indent = 0;
    at_line_start = 1;

    while (*p != '\0') {
        /* 行頭：空行を読み飛ばし、タブの数でインデントの深さを判定する */
        if (at_line_start) {
            if (is_blank_line(p)) {
                p = skip_to_eol(p);
                if (*p == '\n') {
                    p++;
                    line++;
                    line_start = p;
                }
                continue;
            }

            depth = 0;
            while (*p == '\t') {
                depth++;
                p++;
            }
            if (*p == ' ') {
                error_at(line, col_of(p, line_start), "インデントにスペースは使えません。タブを使ってください");
            }
            if (depth > indent + 1) {
                error_at(line, col_of(p, line_start), "インデントが深すぎます。1段ずつ深くしてください");
            }
            if (depth == indent + 1) {
                cur = cur->next = new_token(TK_INDENT, p, 0, line, col_of(p, line_start));
                indent++;
            }
            while (depth < indent) {
                cur = cur->next = new_token(TK_DEDENT, p, 0, line, col_of(p, line_start));
                indent--;
            }
            at_line_start = 0;
            continue;
        }

        /* 改行は文の区切り */
        if (*p == '\n') {
            if (brackets == 0) {
                cur = cur->next = new_token(TK_NEWLINE, p, 0, line, col_of(p, line_start));
            }
            p++;
            line++;
            line_start = p;
            at_line_start = brackets == 0;
            continue;
        }

        /* 行の途中の空白と CR は読み飛ばす */
        if (*p == ' ' || *p == '\t' || *p == '\r') {
            p++;
            continue;
        }

        /* コメント */
        if (p[0] == '/' && p[1] == '/') {
            p = skip_to_eol(p);
            continue;
        }

        /* 文字列リテラル（" または ' で囲む。エスケープの解釈は構文解析で行う） */
        if (*p == '"' || *p == '\'') {
            start = p;
            p = skip_string(p, line, line_start);
            cur = cur->next = new_token(TK_STR, start, (int)(p - start), line, col_of(start, line_start));
            continue;
        }

        /* 整数・識別子・記号 */
        next = lex_basic(p, line, line_start, &cur);
        if (next != NULL) {
            if (*p == '[' || *p == '{') brackets++;
            if ((*p == ']' || *p == '}') && brackets > 0) brackets--;
            p = next;
            continue;
        }

        error_at(line, col_of(p, line_start), "使えない文字です");
    }

    /* 最終行に改行がなくても文を閉じ、残りのインデントをすべて戻す */
    if (cur != &head && cur->kind != TK_NEWLINE) {
        cur = cur->next = new_token(TK_NEWLINE, p, 0, line, col_of(p, line_start));
    }
    while (indent > 0) {
        cur = cur->next = new_token(TK_DEDENT, p, 0, line, col_of(p, line_start));
        indent--;
    }
    cur = cur->next = new_token(TK_EOF, p, 0, line, col_of(p, line_start));
    return head.next;
}
