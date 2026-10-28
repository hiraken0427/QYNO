#include "qyc.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
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

    /* i alone remains an identifier; i followed by digits is reserved. */
    if ((*p == 'i' && isdigit((unsigned char)p[1])) || isdigit((unsigned char)*p)) {
        int prefix = *p == 'i'; const char *number = p + prefix, *end = number;
        while (isdigit((unsigned char)*end)) end++;
        if (end[0] == '.' && isdigit((unsigned char)end[1])) {
            end++; while (isdigit((unsigned char)*end)) end++;
        }
        if (*end == 'e' || *end == 'E') {
            const char *exponent = end + 1;
            if (*exponent == '+' || *exponent == '-') exponent++;
            if (isdigit((unsigned char)*exponent)) { end = exponent + 1; while (isdigit((unsigned char)*end)) end++; }
        }
        if (prefix || *end == 'i') {
            const char *after = end + !prefix;
            if (is_ident_char(*after) || (*after == '.' && isdigit((unsigned char)after[1])))
                error_at(line, col_of(after, line_start), "虚数リテラルの後に数字・英字・_ は書けません");
            if ((size_t)(end - number) >= sizeof(num_buf)) error_at(line, col_of(p, line_start), "虚数の桁が多すぎます");
            memcpy(num_buf, number, (size_t)(end - number)); num_buf[end - number] = 0;
            errno = 0; fval = strtod(num_buf, NULL);
            if (errno == ERANGE) error_at(line, col_of(p, line_start), "虚数が大きすぎるか小さすぎます");
            *cur = (*cur)->next = new_token(TK_IMAG, p, (int)(after - p), line, col_of(p, line_start));
            (*cur)->fval = fval; return after;
        }
    }

    /* Keep the original decimal spelling for contextual binary16/binary128. */
    if (isdigit((unsigned char)*p) && !(p[0]=='0' && integer_prefix((unsigned char)p[1]))) {
        int floating=0; q=p; while(isdigit((unsigned char)*q))q++;
        if(q[0]=='.' && isdigit((unsigned char)q[1])) { floating=1;q++;while(isdigit((unsigned char)*q))q++; }
        if(*q=='e'||*q=='E') { floating=1;q++;if(*q=='+'||*q=='-')q++;if(!isdigit((unsigned char)*q))error_at(line,col_of(q,line_start),"指数の数字が必要です");while(isdigit((unsigned char)*q))q++; }
        if(floating) {
            WideRequest r={0}; Token *t;
            if(is_ident_char(*q)||*q=='.')error_at(line,col_of(q,line_start),"小数の直後に英字・_・.は書けません");
            r.op=W_READ;r.ty=W_QUAD;r.input=p;r.length=(int)(q-p);wide_execute(&r);
            if(r.status)error_at(line,col_of(p,line_start),"小数がbinary128の範囲外か桁数上限4096を超えています");
            *cur=(*cur)->next=t=new_token(TK_FNUM,p,(int)(q-p),line,col_of(p,line_start));
            t->wide=r.out;
            if((size_t)(q-p)<sizeof(num_buf)) { memcpy(num_buf,p,(size_t)(q-p));num_buf[q-p]=0;errno=0;t->fval=strtod(num_buf,NULL);if(errno==ERANGE)t->wide_ty=TY_QUAD; }
            else { r.op=W_CONVERT;r.from=W_QUAD;r.ty=W_DOUBLE;r.a=r.out;wide_execute(&r);memcpy(&t->fval,&r.out.lo,8); }
            if(!isfinite(t->fval)||(t->fval==0&&(t->wide.lo||t->wide.hi)))t->wide_ty=TY_QUAD;
            return q;
        }
    }
    if(isdigit((unsigned char)*p)) {
        WideRequest r={0};Token *t;int base=10;
        start=p;if(p[0]=='0'&&integer_prefix((unsigned char)p[1])){base=integer_prefix(p[1]);p+=2;}
        q=p;while(is_ident_char(*p))p++;
        if(p==q)error_at(line,col_of(start,line_start),"整数の接頭辞の後に数字が必要です");
        r.op=W_READ;r.ty=128;r.input=start;r.length=(int)(p-start);wide_execute(&r);
        if(r.status||r.used!=r.length)error_at(line,col_of(start,line_start),"整数が128bitの範囲外か基数に合わない数字です");
        if(*p=='.'&&isdigit((unsigned char)p[1]))error_at(line,col_of(p,line_start),"基数指定は整数にだけ使えます");
        *cur=(*cur)->next=t=new_token(TK_NUM,start,(int)(p-start),line,col_of(start,line_start));
        t->wide=r.out;t->val=integer_bits(r.out.lo);
        if(r.out.hi)t->wide_ty=base==10&&(r.out.hi>>63)?TY_U128:TY_I128;
        t->decimal_min=base==10&&!r.out.hi&&r.out.lo==(UINT64_C(1)<<63);
        t->unsigned_literal=base==10&&!r.out.hi&&r.out.lo>INT64_MAX;
        return p;
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
