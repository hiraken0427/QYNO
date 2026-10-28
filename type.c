/*
 * 工程4：型検査
 *
 * 式の型を決めてノードに付け、必要な型変換（ND_CAST）を挿入する。
 * 推論付きのローカル変数・const は初期値から型を決め、それ以外は明示された型を使う。
 *
 * 暗黙の型変換の規則
 *   int    → float・double      許可（自動昇格）
 *   float  → double             許可
 *   double → float              リテラルだけでできた式（例：3.14、-0.5、100 * 3.14）のときだけ許可
 *   char   → str               許可（同じ文字列の表現）
 *   str    → char              1スカラー値に限り許可。定数なら静的検査、それ以外は実行時検査
 *   それ以外（float → int、int → bool など）はコンパイルエラー
 *
 * 小数リテラルは double として扱う。
 * 演算の結果の型は、int < float < double の広いほうに合わせる。
 * ただし float と「小数リテラル」の演算は float にする。
 *
 * %：int 同士のみ。結果は int（余りの符号は割られる数と同じ）。
 * ^：底は int・float・double、指数は int。結果は底と同じ型（型をそろえる変換はしない）。
 *
 * 比較：数値同士は上と同じ規則で型をそろえて比べる。bool 同士・str 同士は == と != のみ。
 * and・or・not・if の条件は bool のみ。
 * const の値はここでコンパイル時に計算し、リテラルのノードとして変数に持たせる。
 */

#include "qyc.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

const char *type_name(Type ty)
{
    if (type_is_array(ty)) return array_type_name(ty);
    if (type_is_model(ty)) return type_model(ty)->name;
    if (type_is_interface(ty)) return type_interface(ty)->name;
    switch (ty) {
    case TY_I128: return "int_128";
    case TY_U128: return "unsigned int_128";
    case TY_HALF: return "half";
    case TY_QUAD: return "quadruple";
    case TY_I64: return "int_64";
    case TY_I8: return "int_8";
    case TY_I16: return "int_16";
    case TY_I32: return "int";
    case TY_U8: return "unsigned int_8";
    case TY_U16: return "unsigned int_16";
    case TY_U32: return "unsigned int";
    case TY_U64: return "unsigned int_64";
    case TY_C64: return "complex";
    case TY_C128: return "complex_128";
    case TY_FLOAT:
        return "float";
    case TY_DOUBLE:
        return "double";
    case TY_BOOL:
        return "bool";
    case TY_STR:
        return "str";
    case TY_CHAR:
        return "char";
    case TY_FILE:
        return "file";
    case TY_VOID:
        return "void";
    case TY_ADDR:
        return "場所";
    default:
        return "（型なし）";
    }
}

int type_is_text(Type ty)
{
    return ty == TY_STR || ty == TY_CHAR;
}

static int is_numeric(Type ty)
{
    return type_is_integer(ty) || type_is_real(ty);
}

static int is_number(Type ty) { return is_numeric(ty) || type_is_complex(ty); }

static Ast *cur_ast;
static Function *cur_func;
static int allow_void;          /* 次に調べる呼び出しが文として使われるなら 1 */
static int const_check_depth;
static int function_check_depth;

/* const の計算に使う値 */
typedef struct {
    Type ty;
    WideBits wide;
    long long i;        /* all integer types and bool; unsigned uses the same bits */
    double imag;
    double d;           /* float・double（float は float に丸めた値） */
    char *s;            /* str */
    int len;
    Node *items;
} Value;

static Value eval_const(Node *node);
static int is_const_expr(Node *node);
static void check_expr(Node *node);
static void const_error(Node *node, const char *msg);
static Node *value_node(Value v, Token *tok);
static void check_expected(Node *node, Type expected);
static void check_dims(ArrayDim *dims);
static Node *copy_array(Node *source, ArrayDim *dims, int pad);
static void check_model_init(Node *node, Type expected);
static Node *copy_model(Node *source);
static Node *model_default(Type ty, ArrayDim *dims, Token *tok);
static void validate_const_array(Node *where, Value source, ArrayDim *dims);
static int contains_initializer(Type ty);
static int defaultable(Type ty, ArrayDim *dims);
static void require_default(Type ty, ArrayDim *dims, Token *tok);
static void check_padding(Node *source, Type ty, ArrayDim *dims);
static void check_array_capacity(Node *source, Type ty, ArrayDim *dims);
static void check_construction(Node *node, Model *m);
static void check_initializer_defaults(Model *m);
static void ensure_function(Function *fn, Token *site);
static Function *specialize_generic_call(Node *call);

/*
 * リテラルだけでできた式か（3.14、-0.5、100 * 3.14、2.0^2 など）。
 * double の値でもこの形なら float に自動変換できる。変数や const は含まない。
 */
static int is_float_literal(Node *node)
{
    Node *arg;

    switch (node->kind) {
    case ND_WIDE:
    case ND_COMPLEX:
    case ND_FNUM:
    case ND_NUM:
        return 1;
    case ND_NEG:
    case ND_CAST:
        return is_float_literal(node->lhs);
    case ND_ADD:
    case ND_SUB:
    case ND_MUL:
    case ND_DIV:
    case ND_MOD:
    case ND_POW:
        return is_float_literal(node->lhs) && is_float_literal(node->rhs);
    case ND_CREAL: case ND_CIMAG:
    case ND_ABS:
        return is_float_literal(node->lhs);
    case ND_INT_PARSE:
    case ND_INT_FORMAT:
    case ND_BUILTIN:
        if (node->kind == ND_BUILTIN && node->val >= BI_FILE_OPEN) return 0;
        for (arg = node->lhs; arg != NULL; arg = arg->next) {
            if (!is_float_literal(arg)) {
                return 0;
            }
        }
        return 1;
    default:
        return 0;
    }
}

static void ensure_const(Var *v);
static void check_stmt(Node *node);
static void check_builtin(Node *node);
static void check_input(Node *node);
static void check_ref_arg(Node *arg, Var *param, int index);
static void check_print_parts(Node *list);

/* 名前が同じか */
static int same_name(const char *a, int alen, const char *b, int blen)
{
    return alen == blen && strncmp(a, b, (size_t)alen) == 0;
}

static Var *find_global(Module *module, const char *name, int len)
{
    Var *v;

    for (v = cur_ast->global_vars; v != NULL; v = v->next) {
        if (v->tok->source->module == module && same_name(v->name, v->name_len, name, len)) {
            return v;
        }
    }
    return NULL;
}

static Node *new_cast(Node *expr, Type ty)
{
    Node *node;

    node = calloc(1, sizeof(Node));
    if (node == NULL) {
        fatal("メモリを確保できません");
    }
    node->kind = ND_CAST;
    node->lhs = expr;
    node->ty = ty;
    node->tok = expr->tok;
    return node;
}

#include "wide_type.inc"

static int integer_literal(Node *n)
{
    if (!type_is_integer(n->ty)) return 0;
    if (n->kind == ND_WIDE && n->raw_numeric) return 1;
    if (n->kind == ND_NUM || (n->kind == ND_VAR && n->var->is_const && n->ty == TY_INT)) return 1;
    if (n->kind == ND_NEG) return integer_literal(n->lhs);
    if (n->kind == ND_ADD || n->kind == ND_SUB || n->kind == ND_MUL || n->kind == ND_DIV || n->kind == ND_MOD || n->kind == ND_POW)
        return integer_literal(n->lhs) && integer_literal(n->rhs);
    return 0;
}
static int integer_constant_fits(Node *expr, Type to)
{
    return type_is_integer(expr->ty) && is_const_expr(expr) && ((type_is_wide(expr->ty) || type_is_wide(to)) ? wide_fits(eval_const(expr), to) : integer_fits(eval_const(expr).i, expr->ty, to));
}
static int integer_widens(Type from, Type to)
{
    return integer_common(from, to) == to;
}
static int real_fits_integer(double d, Type to)
{
    int bits = integer_width(to);
    double maximum = ldexp(1.0, bits - !integer_unsigned(to));
    return isfinite(d) && d >= (integer_unsigned(to) ? 0.0 : -maximum) && d < maximum;
}
/* expr を to 型に変換する。変換できなければ NULL */
static Node *convert(Node *expr, Type to)
{
    Type from;

    from = expr->ty;
    if (from == to) {
        return expr;
    }
    if (type_is_complex(to) && is_number(from)) {
        if (from == TY_QUAD) return NULL;
        if ((from != TY_C128 && from != TY_DOUBLE) || to == TY_C128 || is_float_literal(expr)) return new_cast(expr, to);
        return NULL;
    }
    if (type_is_text(from) && type_is_text(to)) {
        Node *cast = new_cast(expr, to);
        if (to == TY_CHAR && is_const_expr(expr)) {
            (void)eval_const(cast);
        }
        return cast;
    }
    /* 値の形の input は、代入先の型で読む */
    if (expr->kind == ND_INPUT && !expr->input_pattern && from == TY_STR &&
        (type_is_integer(to) || type_is_real(to) || to == TY_VOID || to == TY_BOOL || type_is_complex(to))) {
        expr->ty = to;
        return expr;
    }
    if (type_is_integer(from) && type_is_integer(to)) {
        if (integer_widens(from, to) || integer_constant_fits(expr, to)) return new_cast(expr, to);
        return NULL;
    }
    if (type_is_integer(from) && type_is_real(to)) {
        return new_cast(expr, to);
    }
    if (type_is_real(from) && type_is_real(to) && (real_common(from,to)==to || is_float_literal(expr))) return new_cast(expr,to);
    if (from == TY_FLOAT && to == TY_DOUBLE) {
        return new_cast(expr, to);
    }
    if (from == TY_DOUBLE && to == TY_FLOAT && is_float_literal(expr)) {
        return new_cast(expr, to);
    }
    return NULL;
}

/* 変換できなければエラーにする */
static Node *coerce(Node *expr, Type to, Token *where, const char *what)
{
    Node *result;

    result = convert(expr, to);
    if (result == NULL) {
        if (expr->ty == TY_DOUBLE && to == TY_FLOAT) {
            error_tok(where, "%s：double の値は float に自動変換できません（リテラルだけでできた式なら変換できます）", what);
        }
        error_tok(where, "%s：%s の値は %s に変換できません", what, type_name(expr->ty), type_name(to));
    }
    return result;
}

/* 二項演算の結果の型 */
static Type binary_type(Node *lhs, Node *rhs)
{
    Type l;
    Type r;

    check_expected(lhs, rhs->ty); check_expected(rhs, lhs->ty);
    l = lhs->ty;
    r = rhs->ty;
    if (l == r) {
        return l;
    }
    if (type_is_complex(l) || type_is_complex(r)) {
        if(l==TY_QUAD||r==TY_QUAD)error_tok(lhs->tok,"quadruple と複素数を混ぜる場合は成分の型を明示して変換してください");
        if (l == TY_C64 && is_float_literal(rhs)) return TY_C64;
        if (r == TY_C64 && is_float_literal(lhs)) return TY_C64;
        return complex_common(l, r);
    }
    if (type_is_integer(l) && type_is_integer(r)) {
        Type common;
        if (integer_literal(rhs) && integer_constant_fits(rhs, l)) return l;
        if (integer_literal(lhs) && integer_constant_fits(lhs, r)) return r;
        common = integer_common(l, r);
        if (common == TY_NONE) error_tok(lhs->tok, "整数の型 %s と %s は安全に統一できません。明示的に変換してください", type_name(l), type_name(r));
        return common;
    }
    if (type_is_integer(l)) return r;
    if (type_is_integer(r)) return l;
    if (l==TY_QUAD || r==TY_QUAD) return TY_QUAD;
    if (l==TY_HALF && is_float_literal(rhs)) return TY_HALF;
    if (r==TY_HALF && is_float_literal(lhs)) return TY_HALF;
    /* float と double の組み合わせ */
    if (l == TY_FLOAT && is_float_literal(rhs)) {
        return TY_FLOAT;
    }
    if (r == TY_FLOAT && is_float_literal(lhs)) {
        return TY_FLOAT;
    }
    return real_common(l,r);
}

static const char *cmp_name(CmpKind kind)
{
    switch (kind) {
    case CMP_EQ:
        return "==";
    case CMP_NE:
        return "!=";
    case CMP_LT:
        return "<";
    case CMP_LE:
        return "<=";
    case CMP_GT:
        return ">";
    default:
        return ">=";
    }
}

static void check_expr(Node *node);

/* 比較1つの型を決める（左右は変換せず、比べるときの型 cmp_ty だけを記録する） */
static void check_cmp(Node *cmp)
{
    Type l;
    Type r;
    CmpKind kind;

    if ((cmp->lhs->kind == ND_ARRAY_LITERAL || cmp->lhs->kind == ND_MODEL_INIT) && cmp->rhs->kind != ND_ARRAY_LITERAL && cmp->rhs->kind != ND_MODEL_INIT) {
        check_expr(cmp->rhs);
        check_expected(cmp->lhs, cmp->rhs->ty);
    } else {
        check_expr(cmp->lhs);
        check_expected(cmp->rhs, cmp->lhs->ty);
    }
    l = cmp->lhs->ty;
    r = cmp->rhs->ty;
    kind = (CmpKind)cmp->val;
    cmp->ty = TY_BOOL;
    if (type_is_model(l) || type_is_model(r)) {
        if (l != r || (kind != CMP_EQ && kind != CMP_NE)) error_tok(cmp->tok, "モデルは同じ型で == または != を使って比較してください");
        cmp->cmp_ty = l; return;
    }
    if (type_is_array(l) || type_is_array(r)) {
        if (l != r || (kind != CMP_EQ && kind != CMP_NE)) {
            error_tok(cmp->tok, "配列は同じ要素型・次元数で == または != を使って比較してください");
        }
        cmp->cmp_ty = l;
        return;
    }

    if ((type_is_complex(l) || type_is_complex(r)) && is_number(l) && is_number(r)) {
        if (kind != CMP_EQ && kind != CMP_NE) error_tok(cmp->tok, "複素数は == / != で比較してください");
        cmp->cmp_ty = binary_type(cmp->lhs, cmp->rhs); return;
    }
    if (is_numeric(l) && is_numeric(r)) {
        cmp->cmp_ty = binary_type(cmp->lhs, cmp->rhs);
        return;
    }
    if (l == TY_ADDR && r == TY_ADDR) {
        if (kind != CMP_EQ && kind != CMP_NE) {
            error_tok(cmp->tok, "場所は == と != でしか比べられません（%s は使えません）", cmp_name(kind));
        }
        if (cmp->lhs->cmp_ty != TY_NONE && cmp->rhs->cmp_ty != TY_NONE && cmp->lhs->cmp_ty != cmp->rhs->cmp_ty) {
            error_tok(cmp->tok, "%s の変数の場所と %s の変数の場所は比べられません",
                     type_name(cmp->lhs->cmp_ty), type_name(cmp->rhs->cmp_ty));
        }
        cmp->cmp_ty = TY_ADDR;
        return;
    }
    if ((l == r && (l == TY_BOOL || l == TY_FILE || l == TY_VOID)) || (type_is_text(l) && type_is_text(r))) {
        if (kind != CMP_EQ && kind != CMP_NE) {
            error_tok(cmp->tok, "%s は == と != でしか比べられません（%s は使えません）",
                     type_name(l), cmp_name(kind));
        }
        cmp->cmp_ty = type_is_text(l) ? TY_STR : l;
        return;
    }
    error_tok(cmp->tok, "%s と %s は比べられません", type_name(l), type_name(r));
}

static void require_bool(Node *node, const char *what)
{
    if (node->ty != TY_BOOL) {
        error_tok(node->tok, "%sは bool にしてください（%s が指定されています）",
                 what, type_name(node->ty));
    }
}

#include "interfaces_type.inc"
#include "readonly_type.inc"
#include "arrays_type.inc"
#include "models_type.inc"
#include "initialization_type.inc"
#include "integer_bases_type.inc"
#include "complex_type.inc"

static long long round_integer(Node *node, long long x, long long digits, Builtin mode);
static void check_pow_domain(Node *node);

static void check_file_builtin(Node *node)
{
    Builtin b = (Builtin)node->val;
    Node *first = node->lhs;
    Node *second = first != NULL ? first->next : NULL;
    int expected = b == BI_FILE_READLINE || b == BI_FILE_WRITE ? 2 : 1;
    allow_void = 0;
    if (node->nargs != expected && !(b == BI_FILE_OPEN && node->nargs == 2))
        error_tok(node->tok, "io.file.%s の引数の数が不正です（%d 個）", builtin_name(b), node->nargs);
    check_expr(first);
    if (b == BI_FILE_OPEN || b == BI_FILE_EXISTS) {
        if (!type_is_text(first->ty)) error_tok(first->tok, "ファイルのパスは str・char にしてください");
    } else {
        if (first->ty != TY_FILE) error_tok(first->tok, "io.file.%s には file が必要です", builtin_name(b));
        if (b != BI_FILE_IS_OPEN && readonly_place(first) && place_root(first)->var->is_readonly)
            error_tok(first->tok, "readonly の file に %s は使えません（共有状態が変化します）", builtin_name(b));
    }
    if (b == BI_FILE_OPEN) {
        if (second == NULL) {
            second = calloc(1, sizeof(*second));
            if (!second) fatal("既定モードを確保できません");
            second->kind = ND_STRLIT; second->ty = TY_STR; second->tok = node->tok;
            second->str = "r"; second->str_len = 1; first->next = second; node->nargs = 2;
        }
        check_expr(second);
        if (!type_is_text(second->ty)) error_tok(second->tok, "ファイルのモードは str・char にしてください");
        if (is_const_expr(second)) {
            Value mode = eval_const(second);
            if (mode.len != 1 || (mode.s[0] != 'r' && mode.s[0] != 'w' && mode.s[0] != 'a'))
                error_tok(second->tok, "ファイルのモードは r・w・a のいずれかです");
        }
        node->ty = TY_FILE;
    } else if (b == BI_FILE_READLINE) {
        Var target = {0};
        target.ty = TY_STR;
        if (second->kind != ND_REFARG) error_tok(second->tok, "readline の出力先は ref str を指定してください");
        check_ref_arg(second, &target, 2);
        node->ty = TY_BOOL;
    } else if (b == BI_FILE_WRITE) {
        if (second->kind == ND_FORMAT) {
            Node *part;
            check_print_parts(second->lhs);
            for (part = second->lhs; part; part = part->next)
                if (part->kind != ND_STR && !is_number(part->ty) && part->ty != TY_BOOL && part->ty != TY_VOID && !type_is_text(part->ty))
                    error_tok(part->tok, "write の埋め込みは int・float・double・bool・str・char にしてください");
        }
        else {
            check_expr(second);
            if (!is_number(second->ty) && second->ty != TY_BOOL && second->ty != TY_VOID && !type_is_text(second->ty))
                error_tok(second->tok, "write できるのは int・float・double・bool・str・char です");
        }
        node->ty = TY_I64;
    } else if (b == BI_FILE_CLOSE) {
        node->ty = TY_VOID;
    } else node->ty = b == BI_FILE_READ ? TY_STR : TY_BOOL;
}

/* int(x) に渡せる値か（NaN でなく、int の範囲に収まる） */
/*
 * 組み込み関数の引数の数と型を確かめ、戻り値の型を決める
 *   math.sqrt(x)                 x は数値。int は double に変換する。戻り値は x の型
 *   math.abs(x)                  x は数値。戻り値は x の型
 *   math.round・roundup・rounddown(x, 桁数?)  x は数値、桁数は int。戻り値は x の型
 *   int(x)・float(x)・double(x)  x は数値
 */
static void check_builtin(Node *node)
{
    Builtin b;
    Node *x;
    Node *digits;
    int min_args;
    int max_args;
    Value v;

    b = (Builtin)node->val;
    if (b >= BI_FILE_OPEN) { check_file_builtin(node); return; }
    if (b == BI_COMPLEX) { check_complex_constructor(node); return; }
    if ((b==BI_HALF || b==BI_QUAD) && node->lhs) check_expected(node->lhs,b==BI_HALF?TY_HALF:TY_QUAD);
    if (b == BI_INT && node->lhs && integer_unsigned(node->cmp_ty)) check_expected(node->lhs, node->cmp_ty);
    if ((b == BI_INT || b == BI_STR) && check_integer_conversion(node)) return;
    min_args = 1;
    max_args = (b == BI_ROUND || b == BI_ROUNDUP || b == BI_ROUNDDOWN) ? 2 : 1;
    if (node->nargs < min_args || node->nargs > max_args) {
        if (max_args == 2) {
            error_tok(node->tok, "%s の引数は 1 個か 2 個（値, 桁数）です（%d 個渡しています）",
                     builtin_name(b), node->nargs);
        }
        error_tok(node->tok, "%s の引数は 1 個です（%d 個渡しています）", builtin_name(b), node->nargs);
    }
    x = node->lhs;
    if (x->arg_name) error_tok(x->arg_name, "変換する値を位置引数で指定してください");
    digits = x->next;
    check_expr(x);
    if ((b==BI_HALF || b==BI_QUAD) && type_is_text(x->ty)) { node->ty=b==BI_HALF?TY_HALF:TY_QUAD;if(is_const_expr(x))(void)wide_read_value(node,eval_const(x),node->ty,0);return; }
    if(b==BI_STR&&type_is_wide(x->ty)&&type_is_real(x->ty)){node->kind=ND_INT_FORMAT;node->ty=TY_STR;x->option_index=0;return;}
    if(b==BI_STR&&x->ty==TY_VOID){node->ty=TY_STR;return;}
    if (b == BI_CHAR || b == BI_STR) {
        if (!type_is_text(x->ty)) {
            error_tok(x->tok, "%s() に渡せるのは str・char です", builtin_name(b));
        }
        node->ty = b == BI_CHAR ? TY_CHAR : TY_STR;
        if (b == BI_CHAR && is_const_expr(x)) {
            (void)eval_const(node);
        }
        return;
    }
    if (b == BI_SQRT && type_is_complex(x->ty)) { node->ty = x->ty; return; }
    if (b == BI_ABS && type_is_complex(x->ty)) { node->ty = complex_component(x->ty); return; }
    if (!is_numeric(x->ty)) {
        error_tok(x->tok, "%s に渡せるのは数値（int・float・double）です（%s が渡されています）",
                 builtin_name(b), type_name(x->ty));
    }
    if (digits != NULL) {
        check_expr(digits);
        if (!type_is_integer(digits->ty)) {
            error_tok(digits->tok, "%s の桁数は int にしてください（%s が渡されています）",
                     builtin_name(b), type_name(digits->ty));
        }
    }

    if (digits != NULL) { x->next = digits = new_cast(digits, TY_I64); }
    switch (b) {
    case BI_SQRT:
        if (type_is_integer(x->ty)) {
            node->lhs = new_cast(x, type_is_wide(x->ty)?TY_QUAD:TY_DOUBLE);
        }
        node->ty = node->lhs->ty;
        return;
    case BI_INT:
        node->ty = node->cmp_ty ? node->cmp_ty : TY_INT;
        /* 値がコンパイル時に分かるなら、範囲外や NaN はここでエラーにする */
        if (is_const_expr(x)) {
            v = eval_const(x);
            if ((type_is_wide(x->ty)||type_is_wide(node->ty)) ? !wide_fits(v,node->ty) : type_is_integer(x->ty) ? !integer_fits(v.i, x->ty, node->ty) : !real_fits_integer(v.d, node->ty)) {
                error_tok(node->tok, "int() に int の範囲外の値（または NaN）が渡されています");
            }
        }
        return;
    case BI_HALF: case BI_QUAD:
        node->ty=b==BI_HALF?TY_HALF:TY_QUAD; return;
    case BI_FLOAT:
        node->ty = TY_FLOAT;
        return;
    case BI_DOUBLE:
        node->ty = TY_DOUBLE;
        return;
    case BI_ROUND:
    case BI_ROUNDUP:
    case BI_ROUNDDOWN:
        node->ty = x->ty;
        if (type_is_integer(x->ty) && !type_is_wide(x->ty) && is_const_expr(x) &&
            (digits == NULL || is_const_expr(digits))) {
            v = eval_const(x);
            round_integer(node, v.i, digits != NULL ? eval_const(digits).i : 0, b);
        }
        return;
    default:
        node->ty = x->ty;
        return;
    }
}

/*
 * input の型検査
 *   値の形            プロンプトはスカラー値。結果は str で、代入先があればその型で読む（coerce）
 *   直接・形式指定    入力先は変更可能なスカラー変数または要素。結果は bool
 */
static void check_input(Node *node)
{
    InputField *f;
    if (node->name != NULL) resolve_module_name(node);
    if (!node->input_pattern) {
        if (node->lhs != NULL) {
            Type ty;
            check_expr(node->lhs);
            ty = node->lhs->ty;
            if (!is_number(ty) && ty != TY_BOOL && ty != TY_VOID && !type_is_text(ty))
                error_tok(node->lhs->tok, "input のプロンプトは int・float・double・bool・str・char にしてください");
        }
        node->ty = TY_STR;
        return;
    }
    for (f = node->fields; f != NULL; f = f->next) {
        Type ty;
        check_expr(f->var);
        check_place(f->var);
        ty = f->var->ty;
        if (f->base && !type_is_integer(ty)) error_tok(f->var->tok, "基数指定の入力先は int にしてください");
        if (!is_number(ty) && ty != TY_BOOL && ty != TY_VOID && !type_is_text(ty))
            error_tok(f->var->tok, "%s には直接入力できません。変数または末端の配列要素を指定してください", type_name(ty));
        /* 文字列内の1文字への入力も1スカラー値に制限する。 */
        f->temp->ty = f->var->kind == ND_INDEX && type_is_text(f->var->lhs->ty) ? TY_CHAR : ty;
    }
    node->ty = TY_BOOL;
}

/*
 * ref の変数 v に、場所の式 e を入れてよいか確かめる（初期値・p.addr = e・ref の引数）
 *   e は x.addr か none（ほかの ref の変数 q の q.addr も可）。指す先の型は v の型と同じ。
 *   指す先の変数は、v と同じか外側のブロックで宣言されていること（v より先に消えないため）。
 */
static void check_ref_target(Var *v, Node *e, const char *what)
{
    Var *target;

    check_expr(e);
    if (e->ty != TY_ADDR) {
        error_tok(e->tok, "%s：ref の変数には場所（x.addr か none）を入れてください（%s が指定されています）",
                 what, type_name(e->ty));
    }
    if (e->kind == ND_NONE) {
        return;
    }
    if (e->cmp_ty != v->ty) {
        error_tok(e->tok, "%s：%s の ref の変数には、%s の変数の場所を入れてください（%s の変数の場所が指定されています）",
                 what, type_name(v->ty), type_name(v->ty), type_name(e->cmp_ty));
    }
    if (e->kind == ND_ADDR) {
        target = e->lhs->var;
        check_ref_limits(v, target, e->tok);
        if (target->depth > v->depth) {
            error_tok(e->tok, "%s：内側のブロックの変数 %.*s は指せません（ブロックを抜けると消えるため）",
                     what, target->name_len, target->name);
        }
    }
}

/* ref の引数に渡す変数を確かめる（ND_REFARG の lhs） */
static void check_ref_arg(Node *arg, Var *param, int index)
{
    Var *v;
    Node *root;
    int rank = 0;
    Node *saved_interface_use = allowed_interface_use;
    allowed_interface_use = arg->lhs;
    check_expr(arg->lhs);
    v = param->is_readonly ? check_read_place(arg->lhs) : check_place(arg->lhs);
    root = arg->lhs;
    while (root->kind == ND_INDEX) {
        if (!type_is_array(root->lhs->ty) && !type_is_model(root->lhs->ty))
            error_tok(root->tok, "文字列内の文字は ref で渡せません。char 配列の要素を指定してください");
        rank++;
        root = root->lhs;
    }
    if (v->is_loop) error_tok(arg->tok, "%d 番目の引数：for 文の繰り返しの変数は ref で渡せません", index);
    if (type_is_interface(param->ty)) {
        if (!implements_type(arg->lhs->ty, param->ty))
            error_tok(arg->tok, "%s は interface %s を implements していません。同じ契約の ref を渡してください", type_name(arg->lhs->ty), type_name(param->ty));
    } else if (arg->lhs->ty != param->ty)
        error_tok(arg->tok, "%d 番目の引数：ref の引数の型は %s です（%s は渡せません。ref では型を変換しません）",
                  index, type_name(param->ty), type_name(arg->lhs->ty));
    if (rank == 0) check_ref_limits(param, v, arg->tok);
    if (param->is_readonly && v->is_const) materialize_const_ref(arg, v);
    arg->ty = TY_ADDR;
    arg->cmp_ty = arg->lhs->ty;
    allowed_interface_use = saved_interface_use;
}

/* 関数呼び出し：呼び出し先を探し、引数の数と型を確かめて、引数を引数の型に変換する */
static void check_call(Node *node)
{
    Function *fn;
    Var *param;
    Node head;
    Node *cur;
    Node *arg;
    Node *next;
    int allow;
    char what[64];
    int i;

    allow = allow_void;
    allow_void = 0;

    if (!node->func) resolve_module_name(node);
    if (node->kind == ND_MODEL_METHOD) {
        allow_void = allow; check_model_method(node); allow_void = 0; return;
    }
    if (node->kind == ND_MODEL_INIT) { check_model_init(node, node->cmp_ty); return; }
    if (node->kind == ND_BUILTIN) {
        allow_void = allow;
        check_builtin(node);
        allow_void = 0;
        return;
    }
    fn = node->func;
    if (!fn->model && !fn->contract && same_name(fn->name, fn->name_len, "main", 4)) {
        error_tok(node->tok, "main は呼び出せません");
    }
    if (node->nargs != fn->param_count) {
        error_tok(node->tok, "関数 %.*s の引数は %d 個です（%d 個渡しています）",
                 fn->name_len, fn->name, fn->param_count, node->nargs);
    }
    if (fn->is_generic) fn = node->func = specialize_generic_call(node);
    if (fn->infer_return || (fn->generic_origin && fn->check_state != 1)) ensure_function(fn, node->tok);

    memset(&head, 0, sizeof(head));
    cur = &head;
    param = fn->params;
    i = 1;
    for (arg = node->lhs; arg != NULL; arg = next) {
        next = arg->next;
        arg->next = NULL;
        if (param->is_ref) {
            if (arg->kind != ND_REFARG) {
                error_tok(arg->tok, "%d 番目の引数は ref の引数です。呼び出し側にも ref を書いてください（例：f(ref x)）", i);
            }
            check_ref_arg(arg, param, i);
            cur = cur->next = arg;
        } else {
            if (arg->kind == ND_REFARG) {
                error_tok(arg->tok, "%d 番目の引数は値渡しの引数なので、ref は付けられません", i);
            }
            check_expected(arg, param->ty);
            check_value_transfer(arg);
            snprintf(what, sizeof(what), "%d 番目の引数", i);
            cur = cur->next = coerce(arg, param->ty, arg->tok, what);
            if (type_is_model(param->ty)) {
                Node *converted = copy_model(cur); Node *before = &head;
                while (before->next != cur) before = before->next;
                before->next = converted; cur = converted;
            }
            if (type_is_array(param->ty)) {
                Node *converted = copy_array(cur, param->dims, 0);
                /* cur の直前を探索し、変換ノードを引数リストへ入れる。 */
                Node *before = &head;
                while (before->next != cur) before = before->next;
                before->next = converted;
                cur = converted;
            }
        }
        if (param->is_ref && type_is_array(param->ty)) cur->dims = param->dims;
        param = param->next;
        i++;
    }
    node->lhs = head.next;
    /* ref は変数の別名。同じ場所を複数の引数に渡しても、通常の実行順で読み書きする。 */
    node->func = fn;
    node->ty = fn->ret;

}

static void check_expr(Node *node)
{
    Type ty;
    Node *cmp;

    if (node->kind == ND_VAR && node->var && type_is_interface(node->var->ty) &&
        (allowed_interface_use != node || node->explicit_value))
        error_tok(node->tok, "interface 引数は契約メソッドの呼び出しと同じ契約への ref 転送にだけ使用できます");

    /* 比較の連鎖では同じ式を2つの比較から参照するので、確認済みなら何もしない */
    if (node->ty != TY_NONE) {
        return;
    }

    if (node->kind == ND_VAR && node->var == NULL) resolve_module_name(node);
    switch (node->kind) {
    case ND_UNIT: node->ty=TY_VOID;return;
    case ND_WIDE: return;
    case ND_NUM:
        node->ty = node->val >= INT32_MIN && node->val <= INT32_MAX ? TY_INT : TY_I64;
        return;
    case ND_FNUM:
        node->ty = TY_DOUBLE;
        return;
    case ND_BOOL:
        node->ty = TY_BOOL;
        return;
    case ND_STRLIT:
        node->ty = TY_STR;
        return;
    case ND_INT_PARSE:
    case ND_INT_FORMAT:
        return;
    case ND_MODEL_INIT:
        check_model_init(node, node->cmp_ty);
        return;
    case ND_MODEL_COPY:
        check_expr(node->lhs); node->ty = node->lhs->ty; return;
    case ND_FIELD:
        check_expr(node->lhs);
        if (type_is_complex(node->lhs->ty)) {
            if (!strcmp(node->name, "real")) node->kind = ND_CREAL;
            else if (!strcmp(node->name, "imag")) node->kind = ND_CIMAG;
            else error_tok(node->tok, "複素数の成分は real / imag です");
            node->ty = complex_component(node->lhs->ty); return;
        }
        check_model_field(node); return;
    case ND_MODEL_METHOD:
        check_model_method(node); return;
    case ND_ARRAY_LITERAL:
        check_array_literal(node, TY_NONE);
        return;
    case ND_METHOD:
        { int allow = allow_void; allow_void = 0; check_interface_receiver(node->lhs); allow_void = allow;
          if (type_is_model(node->lhs->ty) || type_is_interface(node->lhs->ty)) { check_model_method(node); return; } }
        check_method(node);
        return;
    case ND_LENGTH:
    case ND_INDEX:
        check_expr(node->lhs);
        if (!type_is_text(node->lhs->ty) && !type_is_array(node->lhs->ty)) {
            error_tok(node->tok, "添字と length は str・char に使えます（%s が指定されています）", type_name(node->lhs->ty));
        }
        node->ty = node->kind == ND_LENGTH ? TY_I64 :
            type_is_array(node->lhs->ty) ? array_element(node->lhs->ty) : TY_STR;
        if (node->kind == ND_INDEX) {
            check_expr(node->rhs);
            if (!type_is_integer(node->rhs->ty)) error_tok(node->rhs->tok, "添字は整数型にしてください");
            node->rhs = new_cast(node->rhs, TY_I64);
            if (is_const_expr(node->rhs)) {
                long long index = eval_const(node->rhs).i;
                long long maximum = expression_max_length(node->lhs);
                if (index < 0 || (maximum >= 0 && index >= maximum)) {
                    error_tok(node->rhs->tok, "%sの添字が範囲外です", type_is_array(node->lhs->ty) ? "配列" : "文字列");
                }
            }
        }
        if (is_const_expr(node)) {
            (void)eval_const(node);
        }
        return;
    case ND_VAR:
        if (node->var->is_const && node->var->is_inferred && node->var->ty == TY_NONE)
            ensure_const(node->var);
        node->ty = node->var->ty;
        return;
    case ND_CALL:
        check_call(node);
        return;
    case ND_BUILTIN:
        check_builtin(node);
        return;
    case ND_FORMAT:
        error_tok(node->tok, "文字列の {式} は print または io.file.write の出力に使ってください");
        return;
    case ND_INPUT:
        check_input(node);
        return;
    case ND_ADDR:
        check_expr(node->lhs);
        if (node->lhs->var->is_readonly)
            error_tok(node->tok, "readonly の参照から addr を取り出せません");
        if (node->lhs->var->is_const) {
            error_tok(node->tok, "定数 %.*s は場所（addr）を持ちません",
                     node->lhs->var->name_len, node->lhs->var->name);
        }
        node->ty = TY_ADDR;
        node->cmp_ty = node->lhs->var->ty;
        return;
    case ND_NONE:
        node->ty = TY_ADDR;
        node->cmp_ty = TY_NONE;
        return;
    case ND_REFARG:
        error_tok(node->tok, "ref は、自分で定義した関数の引数にだけ書けます");
        return;
    case ND_ABS:
        check_expr(node->lhs);
        if (!is_number(node->lhs->ty)) {
            error_tok(node->tok, "絶対値 |x| の中は数値（int・float・double）にしてください（%s が指定されています）",
                     type_name(node->lhs->ty));
        }
        node->ty = type_is_complex(node->lhs->ty) ? complex_component(node->lhs->ty) : node->lhs->ty;
        return;
    case ND_CAST:
        return;
    case ND_CHAIN:
        for (cmp = node->lhs; cmp != NULL; cmp = cmp->next) {
            check_cmp(cmp);
        }
        node->ty = TY_BOOL;
        return;
    case ND_AND:
    case ND_OR:
    case ND_XOR:
        check_expr(node->lhs); check_expr(node->rhs);
        if (type_is_integer(node->lhs->ty) && type_is_integer(node->rhs->ty)) {
            ty = binary_type(node->lhs, node->rhs);
            node->lhs = coerce(node->lhs, ty, node->tok, "ビット演算");
            node->rhs = coerce(node->rhs, ty, node->tok, "ビット演算");
        }
        if (node->lhs->ty != node->rhs->ty || (node->lhs->ty != TY_BOOL && !type_is_integer(node->lhs->ty)))
            error_tok(node->tok, "and / or / xor は bool 同士または int 同士にしてください。型を混在できません");
        node->ty = node->lhs->ty;
        if (type_is_integer(node->ty)) node->kind = node->kind == ND_AND ? ND_BIT_AND : node->kind == ND_OR ? ND_BIT_OR : ND_BIT_XOR;
        return;
    case ND_BIT_NOT:
        check_expr(node->lhs);
        if (!type_is_integer(node->lhs->ty)) error_tok(node->tok, "bnot は int にだけ使用できます");
        node->ty = node->lhs->ty; return;
    case ND_SHL:
    case ND_SHR:
        check_expr(node->lhs); check_expr(node->rhs);
        if (!type_is_integer(node->lhs->ty) || !type_is_integer(node->rhs->ty)) error_tok(node->tok, "シフトの値と量は int にしてください");
        node->ty = node->lhs->ty;
        node->rhs = new_cast(node->rhs,TY_I64);
        if (is_const_expr(node->rhs)) {
            long long amount = eval_const(node->rhs).i;
            if ((uint64_t)amount >= (unsigned)integer_width(node->ty)) error_tok(node->tok, "シフト量は0以上%d以下にしてください", integer_width(node->ty) - 1);
        }
        return;
    case ND_NOT:
        check_expr(node->lhs);
        require_bool(node->lhs, "not の対象");
        node->ty = TY_BOOL;
        return;
    case ND_MOD:
        check_expr(node->lhs);
        check_expr(node->rhs);
        if (!type_is_integer(node->lhs->ty) || !type_is_integer(node->rhs->ty)) {
            error_tok(node->tok, "%% は int 同士でのみ使えます（%s と %s）",
                     type_name(node->lhs->ty), type_name(node->rhs->ty));
        }
        node->ty = binary_type(node->lhs, node->rhs);
        node->lhs = coerce(node->lhs, node->ty, node->tok, "余り");
        node->rhs = coerce(node->rhs, node->ty, node->tok, "余り");
        return;
    case ND_POW:
        check_expr(node->lhs);
        check_expr(node->rhs);
        if (!is_number(node->lhs->ty)) {
            error_tok(node->tok, "^ の底は数値（int・float・double）にしてください（%s が指定されています）",
                     type_name(node->lhs->ty));
        }
        if (!type_is_integer(node->rhs->ty)) {
            error_tok(node->tok, "^ の指数は int にしてください（%s の指数は未対応です）",
                     type_name(node->rhs->ty));
        }
        node->rhs = new_cast(node->rhs, TY_I64);
        node->ty = node->lhs->ty;
        check_pow_domain(node);
        return;
    case ND_NEG:
        check_expr(node->lhs);
        if (!is_number(node->lhs->ty)) {
            error_tok(node->tok, "単項 - は数値（int・float・double）にしか使えません（%s には使えません）",
                     type_name(node->lhs->ty));
        }
        node->ty = node->lhs->ty;
        return;
    case ND_ADD:
    case ND_SUB:
    case ND_MUL:
    case ND_DIV:
        check_expr(node->lhs);
        check_expr(node->rhs);
        if (type_is_text(node->lhs->ty) || type_is_text(node->rhs->ty)) {
            if (node->kind == ND_ADD && type_is_text(node->lhs->ty) && type_is_text(node->rhs->ty)) {
                node->ty = TY_STR;
                return;
            }
            error_tok(node->tok, "文字列は + で str 同士を連結することしかできません（%s と %s）",
                     type_name(node->lhs->ty), type_name(node->rhs->ty));
        }
        if (!is_number(node->lhs->ty) || !is_number(node->rhs->ty)) {
            error_tok(node->tok, "%s と %s は計算できません",
                     type_name(node->lhs->ty), type_name(node->rhs->ty));
        }
        ty = binary_type(node->lhs, node->rhs);
        node->lhs = coerce(node->lhs, ty, node->tok, "計算");
        node->rhs = coerce(node->rhs, ty, node->tok, "計算");
        node->ty = ty;
        if (node->kind == ND_DIV && type_is_complex(ty) && is_const_expr(node->rhs)) {
            Value divisor = eval_const(node->rhs);
            if (divisor.d == 0 && divisor.imag == 0) error_tok(node->tok, "0 の複素数では割れません");
        }
        return;
    default:
        fatal("内部エラー: 型検査で未対応の式です");
    }
}

/* ---------- const の値の計算 ---------- */

static void const_error(Node *node, const char *msg)
{
    error_tok(node->tok, "%s", msg);
}

/* 全体を検証し、文字数と指定添字のバイト範囲を得る。index < 0 は文字数だけ。 */
static long long string_info(Node *node, Value v, long long index, int *offset, int *length)
{
    uint64_t off,width,count;
    if(grapheme_scan((const unsigned char *)v.s,(uint64_t)v.len,index,&off,&width,&count))const_error(node,"不正な UTF-8 文字列です");
    *offset=off==UINT64_MAX?-1:(int)off;*length=(int)width;return (long long)count;
}

/* 値 v を to 型に変換する（型検査で許した変換のみ） */
static Value convert_value(Node *node, Value v, Type to)
{
    int offset;
    int length;

    if (v.ty == to) {
        return v;
    }
    if ((type_is_wide(v.ty)||type_is_wide(to)) && is_numeric(v.ty) && is_numeric(to)) return wide_calculate(node,W_CONVERT,v,(Value){0},to,0);
    if(v.ty==TY_VOID&&to==TY_STR){v.ty=TY_STR;v.s="()";v.len=2;return v;}
    if (type_is_complex(to)) {
        if (!type_is_complex(v.ty)) { v = convert_value(node, v, complex_component(to)); v.imag = 0; }
        v.ty = to; return complex_rounded(v);
    }
    if (type_is_text(v.ty) && type_is_text(to)) {
        if (to == TY_CHAR && string_info(node, v, -1, &offset, &length) != 1) {
            const_error(node, "char には 書記素クラスタをちょうど1つ含む文字列が必要です");
        }
        v.ty = to;
        return v;
    }
    if (type_is_integer(to)) {
        if (type_is_integer(v.ty)) {
            if (!integer_fits(v.i, v.ty, to)) const_error(node, "整数変換の結果が対象型の範囲外です");
        } else {
            if (!real_fits_integer(v.d, to)) const_error(node, "整数変換の結果が対象型の範囲外です");
            v.i = integer_unsigned(to) ? integer_bits((uint64_t)v.d) : (long long)v.d;
        }
    } else if (type_is_integer(v.ty)) {
        if (to == TY_FLOAT) v.d = integer_unsigned(v.ty) ? (double)(float)(uint64_t)v.i : (double)(float)v.i;
        else v.d = integer_unsigned(v.ty) ? (double)(uint64_t)v.i : (double)v.i;
    } else if (to == TY_FLOAT) {
        v.d = (double)(float)v.d;
    }
    v.ty = to;
    return v;
}

static int compare_values(Value l, Value r, CmpKind kind)
{
    int eq;
    Node *a;
    Node *b;

    if(type_is_wide(l.ty)) { WideRequest request={0};request.op=W_CMP;request.ty=wide_tag(l.ty);request.a=value_bits(l);request.b=value_bits(r);request.aux=kind;wide_execute(&request);return (int)request.out.lo; }
    if(l.ty==TY_VOID)return kind==CMP_EQ;
    if (type_is_complex(l.ty)) {
        eq = l.d == r.d && l.imag == r.imag;
        return kind == CMP_EQ ? eq : !eq;
    }
    if (type_is_array(l.ty) || type_is_model(l.ty)) {
        eq = l.len == r.len;
        for (a = l.items, b = r.items; eq && a != NULL; a = a->next, b = b->next) {
            eq = compare_values(eval_const(type_is_model(l.ty) ? a->lhs : a),
                                eval_const(type_is_model(l.ty) ? b->lhs : b), CMP_EQ);
        }
        return kind == CMP_EQ ? eq : !eq;
    }

    if (type_is_text(l.ty)) {
        eq = l.len == r.len && (l.len == 0 || memcmp(l.s, r.s, (size_t)l.len) == 0);
        return kind == CMP_EQ ? eq : !eq;
    }
    if (integer_unsigned(l.ty)) {
        uint64_t a = (uint64_t)l.i, b = (uint64_t)r.i;
        return kind == CMP_EQ ? a == b : kind == CMP_NE ? a != b : kind == CMP_LT ? a < b : kind == CMP_LE ? a <= b : kind == CMP_GT ? a > b : a >= b;
    }
    if (type_is_integer(l.ty) || l.ty == TY_BOOL || l.ty == TY_FILE) {
        switch (kind) {
        case CMP_EQ:
            return l.i == r.i;
        case CMP_NE:
            return l.i != r.i;
        case CMP_LT:
            return l.i < r.i;
        case CMP_LE:
            return l.i <= r.i;
        case CMP_GT:
            return l.i > r.i;
        default:
            return l.i >= r.i;
        }
    }
    switch (kind) {
    case CMP_EQ:
        return l.d == r.d;
    case CMP_NE:
        return l.d != r.d;
    case CMP_LT:
        return l.d < r.d;
    case CMP_LE:
        return l.d <= r.d;
    case CMP_GT:
        return l.d > r.d;
    default:
        return l.d >= r.d;
    }
}

/* 累乗の定義域。0^0 は 1、0 の負の累乗と int で非整数になる結果はエラー。 */
static void validate_pow(Node *node, Value base, long long exponent)
{
    if(type_is_wide(base.ty)) { Value b={0};b.ty=TY_I64;b.i=exponent;(void)wide_calculate(node,W_POW,base,b,base.ty,0);return; }
    if (exponent >= 0) {
        return;
    }
    if (type_is_complex(base.ty)) {
        if (base.d == 0 && base.imag == 0) const_error(node, "0 の負の累乗は定義されていません");
        return;
    }
    if ((type_is_integer(base.ty) && base.i == 0) ||
        (!type_is_integer(base.ty) && base.d == 0.0)) {
        const_error(node, "0 の負の累乗は定義されていません");
    }
    if (type_is_integer(base.ty) && base.i != 1 && (integer_unsigned(base.ty) || base.i != -1)) {
        const_error(node, "負の累乗の結果を int で表せません（2.0^(-1) のように底を小数にしてください）");
    }
}

/* リテラル・const・組み込み関数などで判明する不正は、通常の式でもコンパイル時に検出する。 */
static void check_pow_domain(Node *node)
{
    Value exponent;

    if (!is_const_expr(node->rhs)) {
        return;
    }
    exponent = eval_const(node->rhs);
    if (exponent.i < 0 && is_const_expr(node->lhs)) {
        validate_pow(node, eval_const(node->lhs), exponent.i);
    }
}

/*
 * 累乗（実行時ルーチンと同じ手順で計算し、結果を一致させる）
 * 指数の絶対値を2進数で見て、1のビットのところで掛ける。小数の負の指数は最後に逆数にする。
 * int の負の指数は、整数で表せる底 1 と -1 だけを受け付ける。
 */
static Value eval_pow(Node *node, Value base, long long e)
{
    Value v;
    unsigned long long n;
    unsigned long long ir;
    unsigned long long ib;
    double dr;
    double db;
    float fr;
    float fb;

    if(type_is_wide(base.ty)) { Value b={0};b.ty=TY_I64;b.i=e;return wide_calculate(node,W_POW,base,b,base.ty,0); }
    v = base;
    validate_pow(node, base, e);
    n = e < 0 ? 0ULL - (unsigned long long)e : (unsigned long long)e;
    if (type_is_complex(base.ty)) {
        Value result = {0}; result.ty = base.ty; result.d = 1;
        if (e < 0) base = complex_calculate(node, result, base, ND_DIV);
        while (n) {
            if (n & 1) result = complex_calculate(node, result, base, ND_MUL);
            n >>= 1;
            if (n) base = complex_calculate(node, base, base, ND_MUL);
        }
        return result;
    }
    if (type_is_integer(base.ty)) {
        if (e < 0) {
            v.i = base.i == 1 ? 1 : ((n & 1) ? -1 : 1);
            return v;
        }
        ir = 1;
        ib = (unsigned long long)base.i;
        while (n != 0) {
            if (n & 1) {
                ir *= ib;
            }
            ib *= ib;
            n >>= 1;
        }
        v.i = (long long)ir;
        return v;
    }
    if (base.ty == TY_FLOAT) {
        fr = 1.0f;
        fb = (float)base.d;
        while (n != 0) {
            if (n & 1) {
                fr = fr * fb;
            }
            fb = fb * fb;
            n >>= 1;
        }
        if (e < 0) {
            fr = 1.0f / fr;
        }
        v.d = (double)fr;
        return v;
    }
    dr = 1.0;
    db = base.d;
    while (n != 0) {
        if (n & 1) {
            dr = dr * db;
        }
        db = db * db;
        n >>= 1;
    }
    if (e < 0) {
        dr = 1.0 / dr;
    }
    v.d = dr;
    return v;
}

static Value eval_binary(Node *node, Value l, Value r)
{
    Value v;
    float fl;
    float fr;

    if(type_is_wide(node->ty)) return wide_calculate(node,wide_operation(node->kind),l,r,node->ty,0);
    v = l;
    v.ty = node->ty;
    if (type_is_complex(node->ty)) return complex_calculate(node, l, r, node->kind);
    if (node->ty == TY_STR) {
        v.len = l.len + r.len;
        v.s = malloc((size_t)v.len + 1);
        if (v.s == NULL) {
            fatal("メモリを確保できません");
        }
        if (l.len != 0) {
            memcpy(v.s, l.s, (size_t)l.len);
        }
        if (r.len != 0) {
            memcpy(v.s + l.len, r.s, (size_t)r.len);
        }
        return v;
    }
    if (type_is_integer(node->ty)) {
        switch (node->kind) {
        case ND_ADD:
            v.i = (long long)((unsigned long long)l.i + (unsigned long long)r.i);
            break;
        case ND_SUB:
            v.i = (long long)((unsigned long long)l.i - (unsigned long long)r.i);
            break;
        case ND_MUL:
            v.i = (long long)((unsigned long long)l.i * (unsigned long long)r.i);
            break;
        case ND_MOD:
            if (r.i == 0) {
                const_error(node, "const の計算で 0 で割った余りを求めています");
            }
            v.i = integer_unsigned(node->ty) ? integer_bits((uint64_t)l.i % (uint64_t)r.i) : r.i == -1 ? 0 : l.i % r.i;
            break;
        default:
            if (r.i == 0) {
                const_error(node, "const の計算で 0 で割っています");
            }
            if (!integer_unsigned(node->ty) && l.i == -(INT64_C(1) << (integer_width(node->ty) - 2)) * 2 && r.i == -1) {
                const_error(node, "const の計算が int の範囲を超えます");
            }
            v.i = integer_unsigned(node->ty) ? integer_bits((uint64_t)l.i / (uint64_t)r.i) : l.i / r.i;
            break;
        }
        return v;
    }
    if (node->ty == TY_FLOAT) {
        fl = (float)l.d;
        fr = (float)r.d;
        switch (node->kind) {
        case ND_ADD:
            fl = fl + fr;
            break;
        case ND_SUB:
            fl = fl - fr;
            break;
        case ND_MUL:
            fl = fl * fr;
            break;
        default:
            fl = fl / fr;
            break;
        }
        v.d = (double)fl;
        return v;
    }
    switch (node->kind) {
    case ND_ADD:
        v.d = l.d + r.d;
        break;
    case ND_SUB:
        v.d = l.d - r.d;
        break;
    case ND_MUL:
        v.d = l.d * r.d;
        break;
    default:
        v.d = l.d / r.d;
        break;
    }
    return v;
}

/* 10 の n 乗（実行時の累乗ルーチンと同じ手順で計算する） */
static double pow10_same_as_runtime(long long n)
{
    double r;
    double b;
    unsigned long long k;

    r = 1.0;
    b = 10.0;
    k = n < 0 ? 0ULL - (unsigned long long)n : (unsigned long long)n;
    while (k != 0) {
        if (k & 1) {
            r = r * b;
        }
        b = b * b;
        k >>= 1;
    }
    return r;
}

/* 整数への丸め（実行時ルーチンと同じ手順）。絶対値が 2^52 以上や NaN・無限大はそのまま */
static double round_integral(double x, Builtin mode)
{
    double t;
    double diff;

    if (x != x || x >= 4503599627370496.0 || x <= -4503599627370496.0) {
        return x;
    }
    t = (double)(long long)x;
    switch (mode) {
    case BI_ROUND:
        diff = x - t;
        if (diff >= 0.5) {
            t += 1.0;
        } else if (diff <= -0.5) {
            t -= 1.0;
        }
        break;
    case BI_ROUNDUP:
        if (t < x) {
            t += 1.0;
        }
        break;
    default:
        if (t > x) {
            t -= 1.0;
        }
        break;
    }
    return t;
}

/* 桁数付きの丸め。桁数が負なら 10 の |桁数| で割ってから丸めて掛け戻す */
static double round_digits(double x, long long n, Builtin mode)
{
    double s;

    s = pow10_same_as_runtime(n);
    if (n >= 0) {
        return round_integral(x * s, mode) / s;
    }
    return round_integral(x / s, mode) * s;
}

/*
 * int の丸めは浮動小数点へ変換せず、絶対値の商と余りで求める。
 * 負の桁数 -k は 10^k の倍数への丸め。半分ちょうどは 0 から離す。
 * 10^19 と |LLONG_MIN| も扱えるよう、途中の計算は符号なしにする。
 */
static long long round_integer(Node *node, long long x, long long digits, Builtin mode)
{
    unsigned long long magnitude;
    unsigned long long scale;
    unsigned long long quotient;
    unsigned long long remainder;
    unsigned long long limit;
    unsigned long long result;
    long long k;
    int increment;

    int unsign = integer_unsigned(node->ty);
    if (digits >= 0 || x == 0) {
        return x;
    }
    if (digits < -19) {
        /* 10^20 以上では、四捨五入は必ず 0。非0方向の天井・床は範囲外。 */
        if ((mode == BI_ROUNDUP && (unsign || x > 0)) || (mode == BI_ROUNDDOWN && !unsign && x < 0)) {
            const_error(node, "整数の丸め結果が int の範囲を超えます");
        }
        return 0;
    }
    magnitude = !unsign && x < 0 ? 0ULL - (unsigned long long)x : (unsigned long long)x;
    scale = 1;
    for (k = 0; k < -digits; k++) {
        scale *= 10;
    }
    quotient = magnitude / scale;
    remainder = magnitude % scale;
    increment = mode == BI_ROUND ? remainder >= scale / 2 :
                remainder != 0 && (mode == BI_ROUNDUP ? unsign || x > 0 : !unsign && x < 0);
    quotient += increment;
    limit = integer_width(node->ty) == 64 ? UINT64_MAX : (UINT64_C(1) << integer_width(node->ty)) - 1;
    if (!unsign) limit = (limit >> 1) + (x < 0);
    if (quotient > limit / scale) {
        const_error(node, "整数の丸め結果が int の範囲を超えます");
    }
    result = quotient * scale;
    if (unsign || x >= 0) {
        return integer_bits(result);
    }
    return result == (unsigned long long)LLONG_MAX + 1 ? LLONG_MIN : -(long long)result;
}

/* 組み込み関数の値をコンパイル時に計算する */
static Value eval_builtin(Node *node)
{
    Value v;
    Value x;
    long long digits;
    Builtin b;

    b = (Builtin)node->val;
    if (b >= BI_FILE_OPEN) const_error(node, "ファイル操作は const の初期値に使えません");
    if (b == BI_COMPLEX) {
        Node *arg; Value result = {0}; result.ty = node->ty;
        for (arg = node->lhs; arg; arg = arg->next) {
            Value component = convert_value(node, eval_const(arg), complex_component(node->ty));
            if (arg->option_index) result.imag = component.d; else result.d = component.d;
        }
        return result;
    }
    x = eval_const(node->lhs);
    if(b==BI_HALF||b==BI_QUAD) return type_is_text(x.ty)?wide_read_value(node,x,node->ty,0):wide_calculate(node,W_CONVERT,x,(Value){0},node->ty,0);
    if(type_is_wide(x.ty) && b<=BI_ROUNDDOWN) { Value amount={0};amount.ty=TY_I64;amount.i=node->lhs->next?eval_const(node->lhs->next).i:0;return wide_calculate(node,b==BI_SQRT?W_SQRT:b==BI_ABS?W_ABS:W_ROUND,x,amount,node->ty,b-BI_ROUND); }
    v = x;
    v.ty = node->ty;
    digits = node->lhs->next != NULL ? eval_const(node->lhs->next).i : 0;
    switch (b) {
    case BI_SQRT:
        if (type_is_complex(x.ty)) return complex_square_root(x);
        v.d = x.ty == TY_FLOAT ? (double)(float)sqrt(x.d) : sqrt(x.d);
        return v;
    case BI_ABS:
        if (type_is_complex(x.ty)) { v.d = complex_magnitude(x.d, x.imag); v.imag = 0; if (node->ty == TY_FLOAT) v.d = (float)v.d; return v; }
        if (type_is_integer(x.ty)) {
            v.i = !integer_unsigned(x.ty) && x.i < 0 ? (long long)(0ULL - (unsigned long long)x.i) : x.i;
        } else {
            v.d = x.d < 0 || (x.d == 0 && 1.0 / x.d < 0) ? -x.d : x.d;
        }
        return v;
    case BI_ROUND:
    case BI_ROUNDUP:
    case BI_ROUNDDOWN:
        if (type_is_integer(x.ty)) {
            v.i = round_integer(node, x.i, digits, b);
            return v;
        }
        v.d = round_digits(x.d, digits, b);
        if (x.ty == TY_FLOAT) {
            v.d = (double)(float)v.d;
        }
        return v;
    case BI_INT:
        return convert_value(node, x, node->ty);
    case BI_CHAR:
    case BI_STR:
        return convert_value(node, x, node->ty);
    default:
        /* float(x)・double(x) */
        return convert_value(node, x, node->ty == TY_FLOAT ? TY_FLOAT : TY_DOUBLE);
    }
}

#include "const_models.inc"

/* 式の値をコンパイル時に計算する（リテラル・他の const・演算のみ） */
static Value eval_const_inner(Node *node)
{
    Value v;
    Value l;
    Value r;
    Node *cmp;
    int offset;
    int length;

    memset(&v, 0, sizeof(v));
    v.ty = node->ty;
    switch (node->kind) {
    case ND_UNIT: return v;
    case ND_WIDE: v.wide=node->wide;return v;
    case ND_COMPLEX: v.d = node->fval; v.imag = node->imag; return v;
    case ND_CREAL: case ND_CIMAG:
        l = eval_const(node->lhs); v.d = node->kind == ND_CREAL ? l.d : l.imag; return v;
    case ND_INT_PARSE:
    case ND_INT_FORMAT:
        return eval_integer_conversion(node);
    case ND_BIT_NOT:
        if(type_is_wide(node->ty)) return wide_calculate(node,W_NOT,eval_const(node->lhs),(Value){0},node->ty,0);
        v.i = integer_bits(~(uint64_t)eval_const(node->lhs).i); return v;
    case ND_BIT_AND:
    case ND_BIT_OR:
    case ND_BIT_XOR:
    case ND_XOR:
    case ND_SHL:
    case ND_SHR:
        l = eval_const(node->lhs); r = eval_const(node->rhs);
        if(type_is_wide(node->ty))return wide_calculate(node,wide_operation(node->kind),l,r,node->ty,0);
        if (node->kind == ND_BIT_AND) v.i = integer_bits((uint64_t)l.i & (uint64_t)r.i);
        else if (node->kind == ND_BIT_OR) v.i = integer_bits((uint64_t)l.i | (uint64_t)r.i);
        else if (node->kind == ND_BIT_XOR || node->kind == ND_XOR) v.i = integer_bits((uint64_t)l.i ^ (uint64_t)r.i);
        else {
            uint64_t bits = (uint64_t)l.i; int amount = (int)r.i;
            if ((uint64_t)r.i >= (unsigned)integer_width(node->ty)) const_error(node, "シフト量は0以上63以下にしてください");
            if (node->kind == ND_SHL) bits <<= amount;
            else if (amount) { bits >>= amount; if (!integer_unsigned(l.ty) && l.i < 0) bits |= UINT64_MAX << (64 - amount); }
            v.i = integer_bits(bits);
        }
        return v;
    case ND_MODEL_INIT:
    case ND_MODEL_NEW:
        return constant_model(node);
    case ND_MODEL_COPY:
        return eval_const(node->lhs);
    case ND_ARRAY_LITERAL:
        {
            if (node->constant_value) { v.items = node->lhs; v.len = node->nargs; return v; }
            Node head = {0};
            Node *tail = &head;
            for (cmp = node->lhs; cmp != NULL; cmp = cmp->next) {
                tail = tail->next = value_node(eval_const(cmp), cmp->tok);
            }
            v.items = head.next;
            v.len = node->nargs;
            return v;
        }
    case ND_ARRAY_COPY:
        return constant_array(node, eval_const(node->lhs), node->ty, node->dims, (int)node->val);
    case ND_ARRAY_NEW:
        memset(&l, 0, sizeof(l));
        l.ty = node->ty;
        return constant_array(node, l, node->ty, node->dims, 1);
    case ND_NUM:
    case ND_BOOL:
        v.i = node->val;
        return v;
    case ND_FNUM:
        v.d = node->ty == TY_FLOAT ? (double)(float)node->fval : node->fval;
        return v;
    case ND_STRLIT:
        v.s = node->str;
        v.len = node->str_len;
        return v;
    case ND_VAR:
        if (!node->var->is_const) {
            const_error(node, "const の初期値には、リテラルと他の const だけを使った式を書いてください（変数は使えません）");
        }
        ensure_const(node->var);
        return eval_const(node->var->const_val);
    case ND_CAST:
        return convert_value(node, eval_const(node->lhs), node->ty);
    case ND_LENGTH:
        l = eval_const(node->lhs);
        v.i = type_is_array(l.ty) ? l.len : string_info(node, l, -1, &offset, &length);
        return v;
    case ND_INDEX:
        l = eval_const(node->lhs);
        r = eval_const(node->rhs);
        if (type_is_model(l.ty)) {
            if (r.i < 0 || r.i >= l.len) const_error(node, "モデルのフィールド番号が範囲外です");
            cmp = l.items;
            while (r.i-- > 0) cmp = cmp->next;
            return eval_const(cmp->lhs);
        }
        if (type_is_array(l.ty)) {
            if (r.i < 0 || r.i >= l.len) const_error(node, "配列の添字が範囲外です");
            cmp = l.items;
            while (r.i-- > 0) cmp = cmp->next;
            return eval_const(cmp);
        }
        string_info(node, l, r.i, &offset, &length);
        if (r.i < 0 || offset < 0) {
            const_error(node, "文字列の添字が範囲外です");
        }
        v.s = l.s + offset;
        v.len = length;
        return v;
    case ND_NEG:
        v = eval_const(node->lhs);
        if(type_is_wide(v.ty))return wide_calculate(node,W_NEG,v,(Value){0},v.ty,0);
        if (type_is_integer(v.ty)) {
            v.i = (long long)(0ULL - (unsigned long long)v.i);
        } else {
            v.d = -v.d; if (type_is_complex(v.ty)) v.imag = -v.imag;
        }
        return v;
    case ND_ADD:
    case ND_SUB:
    case ND_MUL:
    case ND_DIV:
    case ND_MOD:
        l = eval_const(node->lhs);
        r = eval_const(node->rhs);
        return eval_binary(node, l, r);
    case ND_POW:
        l = eval_const(node->lhs);
        r = eval_const(node->rhs);
        return eval_pow(node, l, r.i);
    case ND_BUILTIN:
        return eval_builtin(node);
    case ND_ABS:
        l = eval_const(node->lhs);
        if(type_is_wide(l.ty))return wide_calculate(node,W_ABS,l,(Value){0},l.ty,0);
        if (type_is_complex(l.ty)) { v.d = complex_magnitude(l.d, l.imag); if (v.ty == TY_FLOAT) v.d = (float)v.d; return v; }
        if (type_is_integer(l.ty)) {
            l.i = !integer_unsigned(l.ty) && l.i < 0 ? (long long)(0ULL - (unsigned long long)l.i) : l.i;
        } else if (l.d < 0 || (l.d == 0 && 1.0 / l.d < 0)) {
            l.d = -l.d;
        }
        return l;
    case ND_CHAIN:
        v.i = 1;
        for (cmp = node->lhs; cmp != NULL; cmp = cmp->next) {
            l = convert_value(cmp, eval_const(cmp->lhs), cmp->cmp_ty);
            r = convert_value(cmp, eval_const(cmp->rhs), cmp->cmp_ty);
            if (!compare_values(l, r, (CmpKind)cmp->val)) {
                v.i = 0;
                break;
            }
        }
        return v;
    case ND_AND:
        v.i = eval_const(node->lhs).i && eval_const(node->rhs).i;
        return v;
    case ND_OR:
        v.i = eval_const(node->lhs).i || eval_const(node->rhs).i;
        return v;
    case ND_NOT:
        v.i = !eval_const(node->lhs).i;
        return v;
    default:
        const_error(node, "const の初期値に使えない式です");
        return v;
    }
}

/* コンパイル時に値を計算できる式か（リテラルと const だけでできているか） */
static int is_const_expr(Node *node)
{
    Node *cmp;
    if (contains_initializer(node->ty)) return 0;
    if (type_is_model(array_base(node->ty)) && contains_file(node->ty)) return 0;

    switch (node->kind) {
    case ND_MODEL_NEW:
        return 1;
    case ND_MODEL_INIT:
        for (cmp = node->lhs; cmp; cmp = cmp->next)
            if (!is_const_expr(cmp->lhs)) return 0;
        return 1;
    case ND_MODEL_COPY:
        return is_const_expr(node->lhs);
    case ND_ARRAY_NEW:
        return 1;
    case ND_ARRAY_LITERAL:
        for (cmp = node->lhs; cmp != NULL; cmp = cmp->next) {
            if (!is_const_expr(cmp)) return 0;
        }
        return 1;
    case ND_ARRAY_COPY:
        return is_const_expr(node->lhs);
    case ND_UNIT:
    case ND_WIDE:
    case ND_COMPLEX:
    case ND_NUM:
    case ND_FNUM:
    case ND_BOOL:
    case ND_STRLIT:
        return 1;
    case ND_VAR:
        return node->var->is_const;
    case ND_CREAL: case ND_CIMAG:
    case ND_CAST:
    case ND_NEG:
    case ND_BIT_NOT:
    case ND_NOT:
    case ND_ABS:
    case ND_LENGTH:
        return is_const_expr(node->lhs);
    case ND_INT_PARSE:
    case ND_INT_FORMAT:
    case ND_BUILTIN:
        if (node->kind == ND_BUILTIN && node->val >= BI_FILE_OPEN) return 0;
        for (cmp = node->lhs; cmp != NULL; cmp = cmp->next) {
            if (!is_const_expr(cmp)) {
                return 0;
            }
        }
        return 1;
    case ND_ADD:
    case ND_SUB:
    case ND_MUL:
    case ND_DIV:
    case ND_MOD:
    case ND_POW:
    case ND_INDEX:
    case ND_AND:
    case ND_OR:
    case ND_XOR:
    case ND_BIT_AND:
    case ND_BIT_OR:
    case ND_BIT_XOR:
    case ND_SHL:
    case ND_SHR:
        return is_const_expr(node->lhs) && is_const_expr(node->rhs);
    case ND_CHAIN:
        for (cmp = node->lhs; cmp != NULL; cmp = cmp->next) {
            if (!is_const_expr(cmp->lhs) || !is_const_expr(cmp->rhs)) {
                return 0;
            }
        }
        return 1;
    default:
        return 0;           /* 関数呼び出しなど */
    }
}

/* 計算した値をリテラルのノードにする */
static Node *value_node(Value v, Token *tok)
{
    Node *node;

    node = calloc(1, sizeof(Node));
    if (node == NULL) {
        fatal("メモリを確保できません");
    }
    node->ty = v.ty;
    node->tok = tok;
    if(v.ty==TY_VOID){node->kind=ND_UNIT;return node;}
    if (type_is_model(v.ty)) {
        node->kind = ND_MODEL_INIT; node->lhs = v.items; node->nargs = v.len; node->constant_value = 1;
        return node;
    }
    if (type_is_array(v.ty)) {
        node->constant_value = 1;
        node->kind = ND_ARRAY_LITERAL;
        node->lhs = v.items;
        node->nargs = v.len;
        return node;
    }
    if(type_is_wide(v.ty)){node->kind=ND_WIDE;node->wide=v.wide;return node;}
    if (type_is_complex(v.ty)) { node->kind = ND_COMPLEX; node->fval = v.d; node->imag = v.imag; return node; }
    if (type_is_integer(v.ty)) { node->kind = ND_NUM; node->val = v.i; return node; }
    switch (v.ty) {
    case TY_INT:
    case TY_FILE:
        node->kind = ND_NUM;
        node->val = v.i;
        break;
    case TY_BOOL:
        node->kind = ND_BOOL;
        node->val = v.i;
        break;
    case TY_FLOAT:
    case TY_DOUBLE:
        node->kind = ND_FNUM;
        node->fval = v.d;
        break;
    default:
        node->kind = ND_STRLIT;
        node->str = v.s;
        node->str_len = v.len;
        break;
    }
    return node;
}

/* ---------- 文 ---------- */

static void check_stmts(Node *list);

/* 定数のヘッダーも、実行時の比較・代入と同じ型へ変換して有限性を調べる。 */
static void check_for_finite_constant(Node *expr, Type ty, const char *part)
{
    Value v;
    if (type_is_integer(ty) || !is_const_expr(expr)) return;
    v = convert_value(expr, eval_const(expr), ty);
    if (type_is_wide(ty)?!wide_finite(v):!isfinite(v.d)) error_tok(expr->tok, "for の%sは有限の数値にしてください", part);
}

/* 増分はループ変数の型でも有限・非0の定数で、方向が一致すること。 */
static void check_for_step(Node *node)
{
    Value v;
    Type ty = node->init->var->ty;
    int direction;

    if (!is_const_expr(node->inc)) {
        error_tok(node->inc->tok,
                 "for の増分の量は定数（リテラルや const）にしてください（向きと 0 をコンパイル時に確かめるため）");
    }
    v = convert_value(node->inc, eval_const(node->inc), ty);
    if (type_is_wide(ty)) {
        int sign=wide_sign(v);
        if(!wide_finite(v))error_tok(node->inc->tok,"for の増分は有限の数値にしてください");
        if(!sign)error_tok(node->inc->tok,"for の増分が 0 です");
        if(sign*node->inc_sign!=node->val)error_tok(node->inc->tok,"増分の向きが範囲の向きと逆です");
        node->for_amount=value_node(v,node->inc->tok);return;
    }
    if (!type_is_integer(ty) && !isfinite(v.d)) {
        error_tok(node->inc->tok, "for の増分は有限の数値にしてください");
    }
    if (type_is_integer(ty) ? v.i == 0 : v.d == 0) {
        error_tok(node->inc->tok, "for の増分が 0 です（ループが終わりません）");
    }
    direction = (type_is_integer(ty) ? (integer_unsigned(ty) || v.i > 0 ? 1 : -1) : (v.d > 0 ? 1 : -1)) * node->inc_sign;
    if (direction != node->val) {
        error_tok(node->inc->tok, "増分の向きが範囲の向きと逆です（%s範囲なので、ループ変数を%sしてください）",
                 node->val > 0 ? "増やしていく" : "減らしていく", node->val > 0 ? "増や" : "減ら");
    }
    node->for_amount = value_node(v, node->inc->tok);
}

static void check_for_header(Node *node)
{
    Type ty = node->init->var->ty;
    Node *limit = node->cond->lhs->rhs;
    if (node->for_open && type_is_integer(ty) && (!type_is_integer(limit->ty) || !type_is_integer(node->inc->ty))) {
        error_tok(limit->tok, "開始側が開いた int の for は、境界・増分も int にしてください");
    }
    check_for_finite_constant(node->init->lhs, ty, "開始値");
    check_for_finite_constant(limit, node->cond->lhs->cmp_ty, "終了境界");
    if (node->for_open && !type_is_integer(ty) && is_const_expr(node->init->lhs)) {
        Value start = eval_const(node->init->lhs);
        Value amount = eval_const(node->for_amount);
        double next;
        if(type_is_wide(ty)){Value candidate=wide_calculate(node,node->inc_sign>0?W_ADD:W_SUB,start,amount,ty,0);
            if(!wide_finite(candidate))error_tok(node->inc->tok,"for の初回の更新結果が有限ではありません");
            if(!compare_values(candidate,start,node->val>0?CMP_GT:CMP_LT))error_tok(node->inc->tok,"for の増分では値が前進しません");
            return;}
        if (ty == TY_FLOAT) {
            next = node->inc_sign > 0 ? (float)((float)start.d + (float)amount.d) :
                                        (float)((float)start.d - (float)amount.d);
        } else {
            next = node->inc_sign > 0 ? start.d + amount.d : start.d - amount.d;
        }
        if (!isfinite(next)) error_tok(node->inc->tok, "for の初回の更新結果が有限ではありません");
        if (node->val > 0 ? !(next > start.d) : !(next < start.d)) {
            error_tok(node->inc->tok, "for の増分では値が前進しません（丸め後の値を確認してください）");
        }
    }
}

/* print の部分の並び（固定部分以外は任意の型の式。void は不可） */
static void check_print_parts(Node *list)
{
    Node *part;

    for (part = list; part != NULL; part = part->next) {
        if (part->kind == ND_STR) {
            continue;
        }
        check_expr(part);
        if (part->format_base && !type_is_integer(part->ty)) error_tok(part->tok, "基数指定は int の式にだけ使えます");
        if (part->ty == TY_FILE || (type_is_array(part->ty) && array_base(part->ty) == TY_FILE))
            error_tok(part->tok, "file は出力・文字列埋め込みに使えません");
    }
}

static void check_print(Node *node)
{
    if (node->name != NULL) resolve_module_name(node);
    check_print_parts(node->lhs);
    check_print_parts(node->end);
}

/* ---------- すべての道筋で return するか ---------- */

static int stmts_return(Node *list);

/* ループの本体（入れ子のループの中は除く）に break() があるか */
static int has_break(Node *list)
{
    Node *node;

    for (node = list; node != NULL; node = node->next) {
        if (node->kind == ND_BREAK) {
            return 1;
        }
        if (node->kind == ND_IF && (has_break(node->then) || has_break(node->els))) {
            return 1;
        }
    }
    return 0;
}

static int stmt_returns(Node *node)
{
    switch (node->kind) {
    case ND_RETURN:
        return 1;
    case ND_IF:
        return node->els != NULL && stmts_return(node->then) && stmts_return(node->els);
    case ND_WHILE:
        /* while(true) で break() しないループは抜けない */
        return node->cond->kind == ND_BOOL && node->cond->val == 1 && !has_break(node->then);
    default:
        return 0;
    }
}

static int stmts_return(Node *list)
{
    Node *node;

    for (node = list; node != NULL; node = node->next) {
        if (stmt_returns(node)) {
            return 1;
        }
    }
    return 0;
}

static void check_stmt(Node *node)
{
    switch (node->kind) {
    case ND_DECL:
        if (node->var->is_const) {
            if (node->var->const_state == 2) return;
            if (node->var->const_state == 1)
                error_tok(node->var->tok, "const %.*s の定義が循環しています",
                    node->var->name_len, node->var->name);
            if (++const_check_depth > MAX_CONST_DEPTH)
                error_tok(node->var->tok, "const の依存は %d 段以下にしてください", MAX_CONST_DEPTH);
            node->var->const_state = 1;
        }
        if (node->var->is_inferred && node->var->ty == TY_NONE) {
            check_expr(node->lhs);
            if (node->lhs->ty == TY_NONE || node->lhs->ty == TY_ADDR) {
                error_tok(node->lhs->tok, "auto の初期値には具体的な値型が必要です（addr は使用できません）");
            }
            concrete_only(node->lhs->ty, node->lhs->tok);
            node->var->ty = node->lhs->ty;
        }
        if (node->var->is_ref) {
            check_dims(node->var->dims);
            if (node->lhs != NULL) {
                check_ref_target(node->var, node->lhs, "初期値");
            }
            return;
        }
        if (node->var->is_const) {
            if (contains_initializer(node->var->ty)) error_tok(node->tok, "init を含むモデルは const にできません");
            if (type_is_model(array_base(node->var->ty)) && contains_file(node->var->ty))
                error_tok(node->tok, "file を含むモデルは const にできません");
        }
        check_dims(node->var->dims);
        if (node->lhs != NULL) {
            check_expected(node->lhs, node->var->ty);
            check_value_transfer(node->lhs);
            node->lhs = coerce(node->lhs, node->var->ty, node->lhs->tok, "初期値");
            if (type_is_array(node->var->ty)) node->lhs = copy_array(node->lhs, node->var->dims, 1);
            if (type_is_model(node->var->ty)) node->lhs = copy_model(node->lhs);
        } else if (type_is_model(node->var->ty)) {
            node->lhs = model_default(node->var->ty, NULL, node->tok);
        } else if (type_is_array(node->var->ty)) {
            node->lhs = model_default(node->var->ty, node->var->dims, node->tok);
        }
        if (node->var->is_const) {
            long long budget = 1000000;
            node->var->const_val = value_node(eval_const(node->lhs), node->lhs->tok);
            constant_size(node, node->var->const_val, &budget);
            node->var->const_state = 2;
            const_check_depth--;
        }
        return;
    case ND_USING:
        return; /* 読み込み後に全スコープを検証済み */
    case ND_ADDR_ASSIGN:
        if (!node->var->is_ref) {
            error_tok(node->tok, "%.*s は ref の変数ではないので、場所（addr）は変えられません",
                     node->var->name_len, node->var->name);
        }
        if (node->var->is_param) {
            error_tok(node->tok, "ref の引数 %.*s の指す先は変えられません（呼び出し側の変数に固定されます）",
                     node->var->name_len, node->var->name);
        }
        check_ref_target(node->var, node->lhs, "場所の代入");
        return;
    case ND_EXPR_STMT:
        allow_void = 1;
        check_expr(node->lhs);
        allow_void = 0;
        return;
    case ND_IF:
        check_expr(node->cond);
        require_bool(node->cond, "if の条件");
        check_stmts(node->then);
        check_stmts(node->els);
        return;
    case ND_WHILE:
        check_expr(node->cond);
        require_bool(node->cond, "while の条件");
        check_stmts(node->then);
        return;
    case ND_FOR:
        if (node->for_infer) {
            Type start;
            check_expr(node->init->lhs); start = node->init->lhs->ty;
            if (type_is_integer(start)) {
                Type counter = integer_common(start, TY_INT);
                node->init->var->ty = counter == TY_NONE ? start : counter;
            }
        }
        if (!is_numeric(node->init->var->ty)) {
            error_tok(node->init->var->tok, "for のループ変数は int・float・double にしてください（%s は使えません）",
                      type_name(node->init->var->ty));
        }
        check_stmt(node->init);
        check_expr(node->cond);
        check_stmt(node->step);
        check_for_step(node);
        check_for_header(node);
        check_stmts(node->then);
        return;
    case ND_BREAK:
    case ND_CONTINUE:
        return;
    case ND_ASSIGN:
        if (type_is_interface(node->var->ty)) error_tok(node->tok, "interface 引数全体には代入できません");
        if (node->var->is_const || node->var->is_readonly)
            error_tok(node->tok, "const / readonly の値は変更できません");
        check_expected(node->lhs, node->var->ty);
        check_value_transfer(node->lhs);
        node->lhs = coerce(node->lhs, node->var->ty, node->lhs->tok, "代入");
        if (type_is_array(node->var->ty) && is_const_expr(node->lhs)) {
            validate_const_array(node, eval_const(node->lhs), node->var->dims);
        }
        return;
    case ND_STORE:
        check_store(node);
        return;
    case ND_RETURN:
        if (cur_func->infer_return && cur_func->ret == TY_NONE) {
            if (node->lhs) {
                check_expr(node->lhs);
                check_value_transfer(node->lhs);
                concrete_only(node->lhs->ty, node->lhs->tok);
                if (node->lhs->ty == TY_NONE || array_base(node->lhs->ty) == TY_ADDR)
                    error_tok(node->lhs->tok, "推論する戻り値には具体的な値型が必要です（addr は値として返せません）");
            }
            return;
        }
        if (cur_func->ret == TY_VOID) {
            if(node->lhs){check_expr(node->lhs);if(node->lhs->ty!=TY_VOID)error_tok(node->lhs->tok,"void の戻り値は () にしてください");}
            return;
        }
        if (node->lhs == NULL) {
            error_tok(node->tok, "return に値が必要です（関数 %.*s の戻り値の型は %s）",
                     cur_func->name_len, cur_func->name, type_name(cur_func->ret));
        }
        check_expected(node->lhs, cur_func->ret);
        check_value_transfer(node->lhs);
        node->lhs = coerce(node->lhs, cur_func->ret, node->lhs->tok, "戻り値");
        if (type_is_array(cur_func->ret)) node->lhs = copy_array(node->lhs, cur_func->return_dims, 0);
        if (type_is_model(cur_func->ret)) node->lhs = copy_model(node->lhs);
        return;
    case ND_PRINT:
        check_print(node);
        return;
    default:
        fatal("内部エラー: 型検査で未対応の文です");
    }
}

static void check_stmts(Node *list)
{
    Node *node;

    for (node = list; node != NULL; node = node->next) {
        check_stmt(node);
    }
}

#include "generic_functions.inc"
#include "return_inference.inc"

static void ensure_const(Var *v)
{
    Function *saved_func = cur_func;
    int saved_allow = allow_void;
    if (v->is_global) cur_func = NULL;
    allow_void = 0;
    check_stmt(v->decl);
    cur_func = saved_func;
    allow_void = saved_allow;
}

/* 名前の重なりと main を確かめる */
static void check_names(Ast *ast)
{
    Function *fn;
    Function *other;
    Var *v;
    Var *g;
    Function *main_fn;

    validate_module_names(ast);

    main_fn = NULL;
    for (fn = ast->funcs; fn != NULL; fn = fn->next) {
        for (other = ast->funcs; other != fn; other = other->next) {
            if (fn->model == other->model && fn->tok->source->module == other->tok->source->module &&
                same_name(fn->name, fn->name_len, other->name, other->name_len)) {
                error_tok(fn->tok, "関数 %.*s は %d 行目ですでに定義されています",
                         fn->name_len, fn->name, other->tok->line);
            }
        }
        g = find_global(fn->tok->source->module, fn->name, fn->name_len);
        if (g != NULL && !fn->model) {
            error_tok(fn->tok, "関数 %.*s は %d 行目の const と同じ名前です",
                     fn->name_len, fn->name, g->tok->line);
        }
        for (v = fn->params; v != NULL; v = v->next) {
            g = find_global(fn->tok->source->module, v->name, v->name_len);
            if (g != NULL) {
                error_tok(v->tok, "引数 %.*s は %d 行目の const と同じ名前です",
                         v->name_len, v->name, g->tok->line);
            }
        }
        for (v = fn->locals; v != NULL; v = v->next) {
            g = find_global(fn->tok->source->module, v->name, v->name_len);
            if (g != NULL) {
                error_tok(v->tok, "%.*s は %d 行目の const と同じ名前です",
                         v->name_len, v->name, g->tok->line);
            }
        }
        if (!fn->model && fn->tok->source->module == ast->entry && same_name(fn->name, fn->name_len, "main", 4)) {
            main_fn = fn;
        }
    }
    if (main_fn == NULL) {
        fatal("main 関数がありません（int main(); が必要です）");
    }
    if (main_fn->infer_return || main_fn->ret != TY_INT || main_fn->param_count != 0) {
        error_tok(main_fn->tok, "main 関数は int main(); の形（戻り値 int、引数なし）で定義してください");
    }
}

#include "initialization_flow.inc"
#include "file_close_analysis.inc"
#include "interfaces_specialize.inc"
#include "inference_warnings.inc"

void typecheck(Ast *ast)
{
    Function *fn;

    cur_ast = ast;
    cur_func = NULL;
    allow_void = 0;
    const_check_depth = 0;
    function_check_depth = 0;
    generic_specs = NULL; generic_spec_count = 0;
    check_names(ast);
    resolve_model_types(ast);
    prepare_builders(ast);
    check_interface_positions(ast);
    { Model *m, *other; Function *f;
      for (m = ast->models; m; m = m->next) {
          for (other = ast->models; other != m; other = other->next)
              if (m->tok->source->module == other->tok->source->module && !strcmp(m->name, other->name)) error_tok(m->tok, "モデル %s が重複しています", m->name);
          if (find_global(m->tok->source->module, m->name, (int)strlen(m->name))) error_tok(m->tok, "モデル名が const と重複しています");
          for (f = ast->funcs; f; f = f->next) if (!f->model && f->tok->source->module == m->tok->source->module && same_name(f->name, f->name_len, m->name, (int)strlen(m->name))) error_tok(m->tok, "モデル名が関数と重複しています");
          check_model_definition(m);
      }
      finish_builders(ast);
      for (m = ast->models; m; m = m->next) visit_default_dependencies(m);
    }

    /* ファイル先頭の const（ほかの const から参照されたものは、その時点で計算済み） */
    check_stmts(ast->globals);

    for (fn = ast->funcs; fn != NULL; fn = fn->next) {
        Var *param;
        check_dims(fn->return_dims);
        for (param = fn->params; param != NULL; param = param->next) check_dims(param->dims);
    }

    for (fn = ast->funcs; fn != NULL; fn = fn->next) {
        if (fn->is_internal || fn->is_generic) continue;
        ensure_function(fn, fn->tok);
    }
    check_implementations(ast);
    for (fn = ast->funcs; fn; fn = fn->next) if (fn->is_initializer) check_initializer_completion(fn);
    specialize_interfaces(ast);
    check_file_closes(ast);
    warn_unused_inferred_locals(ast);
    cur_func = NULL;
}
