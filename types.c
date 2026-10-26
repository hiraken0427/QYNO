#include "qyc.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    Model *model;
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
    return (int)ty >= TY_ARRAY && types[(int)ty - TY_ARRAY].model == NULL && types[(int)ty - TY_ARRAY].path == NULL;
}

Type array_type(Type element)
{
    int i;
    size_t n;
    DynamicType *grown;
    const char *name;
    for (i = 0; i < count; i++) {
        if (!types[i].model && !types[i].path && types[i].element == element) {
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
    else if (types[index].model) return ty;
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
    for (v = ast->global_vars; v; v = v->next) v->ty = resolve_type(v->ty);
}
