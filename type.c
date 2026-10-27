/*
 * 工程4：型検査
 *
 * 式の型を決めてノードに付け、必要な型変換（ND_CAST）を挿入する。
 * 型推論は行わず、変数の型は宣言に書かれたものを使う。
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
    case TY_INT:
        return "int";
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
    return ty == TY_INT || ty == TY_FLOAT || ty == TY_DOUBLE;
}

static Ast *cur_ast;
static Function *cur_func;
static int allow_void;          /* 次に調べる呼び出しが文として使われるなら 1 */

/* const の計算に使う値 */
typedef struct {
    Type ty;
    long long i;        /* int・bool */
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

/*
 * リテラルだけでできた式か（3.14、-0.5、100 * 3.14、2.0^2 など）。
 * double の値でもこの形なら float に自動変換できる。変数や const は含まない。
 */
static int is_float_literal(Node *node)
{
    Node *arg;

    switch (node->kind) {
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
    case ND_ABS:
        return is_float_literal(node->lhs);
    case ND_BUILTIN:
        if (node->val >= BI_FILE_OPEN) return 0;
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

/* expr を to 型に変換する。変換できなければ NULL */
static Node *convert(Node *expr, Type to)
{
    Type from;

    from = expr->ty;
    if (from == to) {
        return expr;
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
        (to == TY_INT || to == TY_FLOAT || to == TY_DOUBLE || to == TY_BOOL)) {
        expr->ty = to;
        return expr;
    }
    if (from == TY_INT && (to == TY_FLOAT || to == TY_DOUBLE)) {
        return new_cast(expr, to);
    }
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

    l = lhs->ty;
    r = rhs->ty;
    if (l == r) {
        return l;
    }
    if (l == TY_INT) {
        return r;
    }
    if (r == TY_INT) {
        return l;
    }
    /* float と double の組み合わせ */
    if (l == TY_FLOAT && is_float_literal(rhs)) {
        return TY_FLOAT;
    }
    if (r == TY_FLOAT && is_float_literal(lhs)) {
        return TY_FLOAT;
    }
    return TY_DOUBLE;
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
    if ((l == r && (l == TY_BOOL || l == TY_FILE)) || (type_is_text(l) && type_is_text(r))) {
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

static long long round_integer(Node *node, long long x, long long digits, Builtin mode);
static void check_pow_domain(Node *node);

static void check_file_builtin(Node *node)
{
    Builtin b = (Builtin)node->val;
    Node *first = node->lhs;
    Node *second = first != NULL ? first->next : NULL;
    int allowed = allow_void;
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
                if (part->kind != ND_STR && !is_numeric(part->ty) && part->ty != TY_BOOL && !type_is_text(part->ty))
                    error_tok(part->tok, "write の埋め込みは int・float・double・bool・str・char にしてください");
        }
        else {
            check_expr(second);
            if (!is_numeric(second->ty) && second->ty != TY_BOOL && !type_is_text(second->ty))
                error_tok(second->tok, "write できるのは int・float・double・bool・str・char です");
        }
        node->ty = TY_INT;
    } else if (b == BI_FILE_CLOSE) {
        node->ty = TY_VOID;
        if (!allowed) error_tok(node->tok, "close は戻り値がないので文として使ってください");
    } else node->ty = b == BI_FILE_READ ? TY_STR : TY_BOOL;
}

/* int(x) に渡せる値か（NaN でなく、int の範囲に収まる） */
static int fits_int(double d)
{
    return d == d && d >= -9223372036854775808.0 && d < 9223372036854775808.0;
}

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
    digits = x->next;
    check_expr(x);
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
    if (!is_numeric(x->ty)) {
        error_tok(x->tok, "%s に渡せるのは数値（int・float・double）です（%s が渡されています）",
                 builtin_name(b), type_name(x->ty));
    }
    if (digits != NULL) {
        check_expr(digits);
        if (digits->ty != TY_INT) {
            error_tok(digits->tok, "%s の桁数は int にしてください（%s が渡されています）",
                     builtin_name(b), type_name(digits->ty));
        }
    }

    switch (b) {
    case BI_SQRT:
        if (x->ty == TY_INT) {
            node->lhs = new_cast(x, TY_DOUBLE);
        }
        node->ty = node->lhs->ty;
        return;
    case BI_INT:
        node->ty = TY_INT;
        /* 値がコンパイル時に分かるなら、範囲外や NaN はここでエラーにする */
        if (x->ty != TY_INT && is_const_expr(x)) {
            v = eval_const(x);
            if (!fits_int(v.d)) {
                error_tok(node->tok, "int() に int の範囲外の値（または NaN）が渡されています");
            }
        }
        return;
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
        if (x->ty == TY_INT && is_const_expr(x) &&
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
            if (!is_numeric(ty) && ty != TY_BOOL && !type_is_text(ty))
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
        if (!is_numeric(ty) && ty != TY_BOOL && !type_is_text(ty))
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
    if (fn->ret == TY_VOID && !allow) {
        error_tok(node->tok, "関数 %.*s は戻り値がない（void）ので、値として使えません",
                 fn->name_len, fn->name);
    }
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
    case ND_NUM:
        node->ty = TY_INT;
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
    case ND_MODEL_INIT:
        check_model_init(node, node->cmp_ty);
        return;
    case ND_MODEL_COPY:
        check_expr(node->lhs); node->ty = node->lhs->ty; return;
    case ND_FIELD:
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
        node->ty = node->kind == ND_LENGTH ? TY_INT :
            type_is_array(node->lhs->ty) ? array_element(node->lhs->ty) : TY_STR;
        if (node->kind == ND_INDEX) {
            check_expr(node->rhs);
            if (node->rhs->ty != TY_INT) {
                error_tok(node->rhs->tok, "添字は int にしてください");
            }
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
        if (!is_numeric(node->lhs->ty)) {
            error_tok(node->tok, "絶対値 |x| の中は数値（int・float・double）にしてください（%s が指定されています）",
                     type_name(node->lhs->ty));
        }
        node->ty = node->lhs->ty;
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
        check_expr(node->lhs);
        check_expr(node->rhs);
        require_bool(node->lhs, node->kind == ND_AND ? "and の左辺" : "or の左辺");
        require_bool(node->rhs, node->kind == ND_AND ? "and の右辺" : "or の右辺");
        node->ty = TY_BOOL;
        return;
    case ND_NOT:
        check_expr(node->lhs);
        require_bool(node->lhs, "not の対象");
        node->ty = TY_BOOL;
        return;
    case ND_MOD:
        check_expr(node->lhs);
        check_expr(node->rhs);
        if (node->lhs->ty != TY_INT || node->rhs->ty != TY_INT) {
            error_tok(node->tok, "%% は int 同士でのみ使えます（%s と %s）",
                     type_name(node->lhs->ty), type_name(node->rhs->ty));
        }
        node->ty = TY_INT;
        return;
    case ND_POW:
        check_expr(node->lhs);
        check_expr(node->rhs);
        if (!is_numeric(node->lhs->ty)) {
            error_tok(node->tok, "^ の底は数値（int・float・double）にしてください（%s が指定されています）",
                     type_name(node->lhs->ty));
        }
        if (node->rhs->ty != TY_INT) {
            error_tok(node->tok, "^ の指数は int にしてください（%s の指数は未対応です）",
                     type_name(node->rhs->ty));
        }
        node->ty = node->lhs->ty;
        check_pow_domain(node);
        return;
    case ND_NEG:
        check_expr(node->lhs);
        if (!is_numeric(node->lhs->ty)) {
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
        if (!is_numeric(node->lhs->ty) || !is_numeric(node->rhs->ty)) {
            error_tok(node->tok, "%s と %s は計算できません",
                     type_name(node->lhs->ty), type_name(node->rhs->ty));
        }
        ty = binary_type(node->lhs, node->rhs);
        node->lhs = coerce(node->lhs, ty, node->tok, "計算");
        node->rhs = coerce(node->rhs, ty, node->tok, "計算");
        node->ty = ty;
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
    int pos = 0;
    long long count = 0;
    size_t width;
    uint32_t scalar;

    *offset = -1;
    *length = 0;
    while (pos < v.len) {
        if (!utf8_next((const unsigned char *)v.s + pos, (size_t)(v.len - pos), &scalar, &width)) {
            const_error(node, "不正な UTF-8 文字列です");
        }
        if (count == index) {
            *offset = pos;
            *length = (int)width;
        }
        pos += (int)width;
        count++;
    }
    return count;
}

/* 値 v を to 型に変換する（型検査で許した変換のみ） */
static Value convert_value(Node *node, Value v, Type to)
{
    int offset;
    int length;

    if (v.ty == to) {
        return v;
    }
    if (type_is_text(v.ty) && type_is_text(to)) {
        if (to == TY_CHAR && string_info(node, v, -1, &offset, &length) != 1) {
            const_error(node, "char には Unicode スカラー値をちょうど1つ含む文字列が必要です");
        }
        v.ty = to;
        return v;
    }
    if (v.ty == TY_INT) {
        v.d = to == TY_FLOAT ? (double)(float)v.i : (double)v.i;
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
    if (l.ty == TY_INT || l.ty == TY_BOOL || l.ty == TY_FILE) {
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
    if (exponent >= 0) {
        return;
    }
    if ((base.ty == TY_INT && base.i == 0) ||
        (base.ty != TY_INT && base.d == 0.0)) {
        const_error(node, "0 の負の累乗は定義されていません");
    }
    if (base.ty == TY_INT && base.i != 1 && base.i != -1) {
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

    v = base;
    validate_pow(node, base, e);
    n = e < 0 ? 0ULL - (unsigned long long)e : (unsigned long long)e;
    if (base.ty == TY_INT) {
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

    v = l;
    v.ty = node->ty;
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
    if (node->ty == TY_INT) {
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
            v.i = r.i == -1 ? 0 : l.i % r.i;
            break;
        default:
            if (r.i == 0) {
                const_error(node, "const の計算で 0 で割っています");
            }
            if (l.i == LLONG_MIN && r.i == -1) {
                const_error(node, "const の計算が int の範囲を超えます");
            }
            v.i = l.i / r.i;
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

    if (digits >= 0 || x == 0) {
        return x;
    }
    if (digits < -19) {
        /* 10^20 以上では、四捨五入は必ず 0。非0方向の天井・床は範囲外。 */
        if ((mode == BI_ROUNDUP && x > 0) || (mode == BI_ROUNDDOWN && x < 0)) {
            const_error(node, "整数の丸め結果が int の範囲を超えます");
        }
        return 0;
    }
    magnitude = x < 0 ? 0ULL - (unsigned long long)x : (unsigned long long)x;
    scale = 1;
    for (k = 0; k < -digits; k++) {
        scale *= 10;
    }
    quotient = magnitude / scale;
    remainder = magnitude % scale;
    increment = mode == BI_ROUND ? remainder >= scale / 2 :
                remainder != 0 && (mode == BI_ROUNDUP ? x > 0 : x < 0);
    quotient += increment;
    limit = x < 0 ? (unsigned long long)LLONG_MAX + 1 : (unsigned long long)LLONG_MAX;
    if (quotient > limit / scale) {
        const_error(node, "整数の丸め結果が int の範囲を超えます");
    }
    result = quotient * scale;
    if (x >= 0) {
        return (long long)result;
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
    x = eval_const(node->lhs);
    v = x;
    v.ty = node->ty;
    digits = node->lhs->next != NULL ? eval_const(node->lhs->next).i : 0;
    switch (b) {
    case BI_SQRT:
        v.d = x.ty == TY_FLOAT ? (double)(float)sqrt(x.d) : sqrt(x.d);
        return v;
    case BI_ABS:
        if (x.ty == TY_INT) {
            v.i = x.i < 0 ? (long long)(0ULL - (unsigned long long)x.i) : x.i;
        } else {
            v.d = x.d < 0 || (x.d == 0 && 1.0 / x.d < 0) ? -x.d : x.d;
        }
        return v;
    case BI_ROUND:
    case BI_ROUNDUP:
    case BI_ROUNDDOWN:
        if (x.ty == TY_INT) {
            v.i = round_integer(node, x.i, digits, b);
            return v;
        }
        v.d = round_digits(x.d, digits, b);
        if (x.ty == TY_FLOAT) {
            v.d = (double)(float)v.d;
        }
        return v;
    case BI_INT:
        if (x.ty == TY_INT) {
            return v;
        }
        if (!fits_int(x.d)) {
            const_error(node, "int() に int の範囲外の値（または NaN）が渡されています");
        }
        v.i = (long long)x.d;
        return v;
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
        if (v.ty == TY_INT) {
            v.i = (long long)(0ULL - (unsigned long long)v.i);
        } else {
            v.d = -v.d;
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
        if (l.ty == TY_INT) {
            l.i = l.i < 0 ? (long long)(0ULL - (unsigned long long)l.i) : l.i;
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
    case ND_NUM:
    case ND_FNUM:
    case ND_BOOL:
    case ND_STRLIT:
        return 1;
    case ND_VAR:
        return node->var->is_const;
    case ND_CAST:
    case ND_NEG:
    case ND_NOT:
    case ND_ABS:
    case ND_LENGTH:
        return is_const_expr(node->lhs);
    case ND_BUILTIN:
        if (node->val >= BI_FILE_OPEN) return 0;
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
    if (ty == TY_INT || !is_const_expr(expr)) return;
    v = convert_value(expr, eval_const(expr), ty);
    if (!isfinite(v.d)) error_tok(expr->tok, "for の%sは有限の数値にしてください", part);
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
    if (ty != TY_INT && !isfinite(v.d)) {
        error_tok(node->inc->tok, "for の増分は有限の数値にしてください");
    }
    if (ty == TY_INT ? v.i == 0 : v.d == 0) {
        error_tok(node->inc->tok, "for の増分が 0 です（ループが終わりません）");
    }
    direction = (ty == TY_INT ? (v.i > 0 ? 1 : -1) : (v.d > 0 ? 1 : -1)) * node->inc_sign;
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
    if (node->for_open && ty == TY_INT && (limit->ty != TY_INT || node->inc->ty != TY_INT)) {
        error_tok(limit->tok, "開始側が開いた int の for は、境界・増分も int にしてください");
    }
    check_for_finite_constant(node->init->lhs, ty, "開始値");
    check_for_finite_constant(limit, node->cond->lhs->cmp_ty, "終了境界");
    if (node->for_open && ty != TY_INT && is_const_expr(node->init->lhs)) {
        Value start = eval_const(node->init->lhs);
        Value amount = eval_const(node->for_amount);
        double next;
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
            if (node->var->const_state == 2) {
                return;         /* ほかの const から参照されて計算済み */
            }
            if (node->var->const_state == 1) {
                error_tok(node->var->tok, "const %.*s の定義が循環しています",
                         node->var->name_len, node->var->name);
            }
            node->var->const_state = 1;
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
        if (cur_func->ret == TY_VOID) {
            if (node->lhs != NULL) {
                error_tok(node->lhs->tok, "void の関数 %.*s は値を返せません",
                         cur_func->name_len, cur_func->name);
            }
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

static void ensure_const(Var *v)
{
    check_stmt(v->decl);
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
    if (main_fn->ret != TY_INT || main_fn->param_count != 0) {
        error_tok(main_fn->tok, "main 関数は int main(); の形（戻り値 int、引数なし）で定義してください");
    }
}

#include "initialization_flow.inc"
#include "file_close_analysis.inc"
#include "interfaces_specialize.inc"

void typecheck(Ast *ast)
{
    Function *fn;

    cur_ast = ast;
    cur_func = NULL;
    allow_void = 0;
    check_names(ast);
    resolve_model_types(ast);
    check_interface_positions(ast);
    prepare_builders(ast);
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

    check_implementations(ast);
    for (fn = ast->funcs; fn != NULL; fn = fn->next) {
        if (fn->is_internal) continue;
        cur_func = fn;
        check_stmts(fn->body);
        if (fn->ret != TY_VOID && (fn->model || !same_name(fn->name, fn->name_len, "main", 4)) && !stmts_return(fn->body)) {
            error_tok(fn->tok, "関数 %.*s には、最後まで進んだときの return がありません（すべての道筋で値を返してください）",
                     fn->name_len, fn->name);
        }
    }
    for (fn = ast->funcs; fn; fn = fn->next) if (fn->is_initializer) check_initializer_completion(fn);
    specialize_interfaces(ast);
    check_file_closes(ast);
    cur_func = NULL;
}
