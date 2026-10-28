#include "qyc.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    Model *model;
    Interface *contract;
    Token *tok;
    char *path;
    Using *scope;
    Type resolved;
    Type element;
    int rank;
    char *name;
} DynamicType;

static DynamicType *types;
static int count;

int type_is_array(Type ty)
{
    return (int)ty >= TY_ARRAY && types[(int)ty - TY_ARRAY].model == NULL && types[(int)ty - TY_ARRAY].contract == NULL && types[(int)ty - TY_ARRAY].path == NULL;
}

Type array_type(Type element)
{
    int i;
    size_t n;
    DynamicType *grown;
    const char *name;
    for (i = 0; i < count; i++) {
        if (!types[i].model && !types[i].contract && !types[i].path && types[i].element == element) {
            return (Type)(TY_ARRAY + i);
        }
    }
    grown = realloc(types, (size_t)(count + 1) * sizeof(*types));
    if (grown == NULL) {
        fatal("配列型のメモリを確保できません");
    }
    types = grown;
    memset(&types[count], 0, sizeof(types[count]));
    name = type_name(element);
    n = strlen(name);
    types[count].name = malloc(n + 3);
    if (types[count].name == NULL) {
        fatal("配列型の名前を確保できません");
    }
    memcpy(types[count].name, name, n);
    memcpy(types[count].name + n, "[]", 3);
    types[count].element = element;
    types[count].rank = array_rank(element) + 1;
    return (Type)(TY_ARRAY + count++);
}

Type array_element(Type array)
{
    return types[(int)array - TY_ARRAY].element;
}

Type array_base(Type array)
{
    while (type_is_array(array)) {
        array = array_element(array);
    }
    return array;
}

int array_rank(Type array)
{
    return type_is_array(array) ? types[(int)array - TY_ARRAY].rank : 0;
}

const char *array_type_name(Type array)
{
    return types[(int)array - TY_ARRAY].name;
}

int type_is_model(Type ty)
{
    return (int)ty >= TY_ARRAY && types[(int)ty - TY_ARRAY].model != NULL;
}
Model *type_model(Type ty) { return types[(int)ty - TY_ARRAY].model; }
static Type add_type(DynamicType entry)
{
    DynamicType *grown = realloc(types, (size_t)(count + 1) * sizeof(*types));
    if (!grown) fatal("型の領域を確保できません");
    types = grown; types[count] = entry; return (Type)(TY_ARRAY + count++);
}
Type model_type(Model *model)
{
    DynamicType e = {0}; e.model = model; return add_type(e);
}
int type_is_interface(Type ty)
{
    return (int)ty >= TY_ARRAY && types[(int)ty - TY_ARRAY].contract != NULL;
}
Interface *type_interface(Type ty) { return types[(int)ty - TY_ARRAY].contract; }
Type interface_type(Interface *contract)
{
    DynamicType e = {0}; e.contract = contract; return add_type(e);
}
Type named_type(Token *tok, const char *name, Using *scope)
{
    DynamicType e = {0}; size_t n = strlen(name);
    e.path = malloc(n + 1); if (!e.path) fatal("型名を確保できません");
    memcpy(e.path, name, n + 1); e.tok = tok; e.scope = scope;
    return add_type(e);
}
Type resolve_type(Type ty)
{
    int index;
    Type element, result;
    if ((int)ty < TY_ARRAY) return ty;
    index = (int)ty - TY_ARRAY;
    if (types[index].resolved) return types[index].resolved;
    if (types[index].path) result = resolve_module_type(types[index].tok, types[index].path, types[index].scope);
    else if (types[index].model || types[index].contract) return ty;
    else { element = resolve_type(types[index].element); result = array_type(element); }
    types[index].resolved = result; return result;
}
void resolve_model_types(Ast *ast)
{
    Function *fn; Model *m; ModelField *f; Var *v;
    for (m = ast->models; m; m = m->next)
        for (f = m->fields; f; f = f->next) f->ty = resolve_type(f->ty);
    for (fn = ast->funcs; fn; fn = fn->next) {
        fn->ret = resolve_type(fn->ret);
        for (v = fn->params; v; v = v->next) v->ty = resolve_type(v->ty);
        for (v = fn->locals; v; v = v->next) v->ty = resolve_type(v->ty);
    }
    { Interface *in;
      for (in = ast->interfaces; in; in = in->next)
          for (fn = in->methods; fn; fn = fn->next) {
              fn->ret = resolve_type(fn->ret);
              for (v = fn->params; v; v = v->next) v->ty = resolve_type(v->ty);
          }
    }
    for (m = ast->models; m; m = m->next) {
        Implementation *im;
        for (im = m->implements; im; im = im->next) im->ty = resolve_type(im->ty);
    }
    for (v = ast->global_vars; v; v = v->next) v->ty = resolve_type(v->ty);
}

/* Logical integer widths; runtime scalar slots stay eight bytes. */
int type_is_integer(Type ty) { return ty >= TY_I64 && ty <= TY_U64; }
int integer_unsigned(Type ty) { return ty >= TY_U8 && ty <= TY_U64; }
int integer_width(Type ty)
{
    switch (ty) {
    case TY_I8: case TY_U8: return 8;
    case TY_I16: case TY_U16: return 16;
    case TY_I32: case TY_U32: return 32;
    default: return 64;
    }
}
Type integer_type(int bits, int u)
{
    return bits == 8 ? (u ? TY_U8 : TY_I8) : bits == 16 ? (u ? TY_U16 : TY_I16) :
           bits == 32 ? (u ? TY_U32 : TY_I32) : (u ? TY_U64 : TY_I64);
}
long long integer_normalize(long long value, Type ty)
{
    int bits = integer_width(ty); uint64_t n = (uint64_t)value;
    if (bits < 64) {
        uint64_t mask = (UINT64_C(1) << bits) - 1;
        n &= mask;
        if (!integer_unsigned(ty) && (n & (UINT64_C(1) << (bits - 1)))) n |= ~mask;
    }
    return integer_bits(n);
}
int integer_fits(long long value, Type from, Type to)
{
    int bits = integer_width(to); uint64_t n = (uint64_t)value;
    if (integer_unsigned(to)) {
        if (!integer_unsigned(from) && value < 0) return 0;
        return bits == 64 || n < (UINT64_C(1) << bits);
    }
    if (integer_unsigned(from)) return n <= (bits == 64 ? INT64_MAX : (UINT64_C(1) << (bits - 1)) - 1);
    return bits == 64 || (value >= -(INT64_C(1) << (bits - 1)) && value < (INT64_C(1) << (bits - 1)));
}
Type integer_common(Type a, Type b)
{
    int aw = integer_width(a), bw = integer_width(b), bits = aw > bw ? aw : bw;
    int au = integer_unsigned(a), bu = integer_unsigned(b);
    if (au == bu) return integer_type(bits, au);
    if ((au ? aw : bw) >= (au ? bw : aw)) {
        if (bits == 64) return TY_NONE;
        bits = bits == 8 ? 16 : bits == 16 ? 32 : 64;
    }
    return integer_type(bits, 0);
}
