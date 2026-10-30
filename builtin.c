/*
 * 組み込み関数の一覧
 *
 * 標準モジュールの公開名と、型の名前で呼ぶ変換を持つ。
 * 標準モジュールはこの表から登録し、import なしで利用可能にする。
 */

#include "qyc.h"

#include <string.h>

typedef struct {
    const char *ns;         /* 名前空間（型変換は NULL） */
    const char *name;
} BuiltinInfo;

/* Builtin の順番と合わせる */
static const BuiltinInfo builtins[BI_COUNT] = {
    { "math", "sqrt" },
    { "math", "abs" },
    { "math", "round" },
    { "math", "ceil" },
    { "math", "floor" },
    { NULL, "int" },
    { NULL, "float" },
    { NULL, "double" },
    { NULL, "char" },
    { NULL, "str" },
    { NULL, "complex" },
    { NULL, "half" },
    { NULL, "quadruple" },
    { "io", "print" },
    { "io", "input" },
#define TIME_API(id,name,min,max,args,result) {"time",name},
#include "time_api.def"
#undef TIME_API
#define RANDOM_API(id,name,ty,op) {"lib.random",name},
#include "random_api.def"
#undef RANDOM_API
    { "lib", "size" }, { "lib", "length" },
    { "io.path", "join" }, { "io.path", "basename" },
    { "io.path", "dirname" }, { "io.path", "extension" },
    { "io.path", "stem" }, { "io.path", "is_absolute" },
    { "io.path", "is_relative" }, { "io.path", "normalize" },
    { "io.path", "with_extension" },
    { "io.dir", "current" }, { "io.dir", "change" }, { "io.path", "absolute" },
    { "io.dir", "exists" }, { "io.dir", "create" },
    { "io.dir", "list" }, { "io.dir", "remove" },
    { "io.file", "open" },
    { "io.file", "close" },
    { "io.file", "is_open" },
    { "io.file", "read" },
    { "io.file", "read_line" },
    { "io.file", "write" },
    { "io.file", "exists" },
    { "io.file", "read_bytes" }, { "io.file", "write_bytes" },
    { "io.file", "try_open" },
    { "io.file", "tell" }, { "io.file", "seek" },
    { "io.file", "size" }, { "io.file", "modified_at" },
    { "io.file", "read_text" }, { "io.file", "write_text" },
    { "io.file", "append_text" }, { "io.file", "read_lines" }, { "io.file", "write_lines" },
    { "io.file", "remove" }, { "io.file", "copy" }, { "io.file", "move" }
};

static int token_is(Token *t, const char *s)
{
    return t != NULL && (int)strlen(s) == t->len && strncmp(t->loc, s, (size_t)t->len) == 0;
}

const char *builtin_name(Builtin b)
{
    return builtins[b].name;
}

const char *builtin_namespace(Builtin b)
{
    return builtins[b].ns;
}

/* 名前空間 ns の name を探す（ns が NULL なら型変換） */
int builtin_find(Token *ns, Token *name)
{
    int i;

    for (i = 0; i < BI_COUNT; i++) {
        if (ns == NULL) {
            if (builtins[i].ns == NULL && token_is(name, builtins[i].name)) {
                return i;
            }
            continue;
        }
        if (builtins[i].ns != NULL && token_is(ns, builtins[i].ns) && token_is(name, builtins[i].name)) {
            return i;
        }
    }
    return -1;
}

/* name という名前の組み込み関数がある名前空間（なければ NULL） */
const char *builtin_namespace_of(const char *name, int len)
{
    int i;

    for (i = 0; i < BI_COUNT; i++) {
        if (builtins[i].ns != NULL && (int)strlen(builtins[i].name) == len && strncmp(builtins[i].name, name, (size_t)len) == 0) {
            return builtins[i].ns;
        }
    }
    return NULL;
}

int builtin_is_random(int b){return b>=BI_RANDOM_INT8&&b<=BI_RANDOM_INIT;}
int random_builtin_find(const char *name,int length){int b;for(b=BI_RANDOM_INT8;b<=BI_RANDOM_INIT;b++)if((int)strlen(builtin_name((Builtin)b))==length&&!memcmp(name,builtin_name((Builtin)b),(size_t)length))return b;return -1;}
Type random_return_type(int b){static const Type types[]={
#define RANDOM_API(id,name,ty,op) ty,
#include "random_api.def"
#undef RANDOM_API
};return types[b-BI_RANDOM_INT8];}
int random_operation(int b){static const int ops[]={
#define RANDOM_API(id,name,ty,op) op,
#include "random_api.def"
#undef RANDOM_API
};return ops[b-BI_RANDOM_INT8];}
int builtin_is_directory(int b){return b>=BI_DIR_CURRENT&&b<=BI_DIR_REMOVE;}
int builtin_is_time(int b){return b>=BI_TIME_NOW&&b<=BI_TIME_FREQUENCY;}
const TimeSpec *time_spec(int op)
{
    static const TimeSpec specs[]={
#define TIME_API(id,name,min,max,args,result) {min,max,args,TR_##result},
#include "time_api.def"
#undef TIME_API
    };
    return &specs[op];
}
int builtin_may_raise(int b)
{
    return (builtin_is_random(b)&&random_operation(b)!=0)||builtin_is_directory(b)||(b>=BI_FILE_OPEN&&b!=BI_FILE_IS_OPEN&&b!=BI_FILE_TRY_OPEN)||
        (builtin_is_time(b)&&(b<=BI_TIME_SLEEP_UNTIL||b>=BI_TIME_TICKS));
}
int builtin_node_may_raise(Node *n)
{return builtin_may_raise((int)n->val)&&!(builtin_is_random((int)n->val)&&n->rhs);}
Type standard_type(Ast *ast,const char *ns,const char *name)
{
    Module *mod;Model *m;EnumDef *e;
    for(mod=ast->modules;mod;mod=mod->next)if(mod->builtin&&!strcmp(mod->name,ns)&&mod->ast){
        for(m=mod->ast->models;m;m=m->next)if(m->tok->source->module==mod&&!strcmp(m->name,name))return m->ty;
        for(e=mod->ast->enums;e;e=e->next)if(e->tok->source->module==mod&&!strcmp(e->name,name))return e->ty;
    }
    fatal("標準型 %s.%s がありません",ns,name);return TY_NONE;
}

/* Old standard names are diagnostics, never callable aliases. */
const char *builtin_replacement(const char *ns,const char *name)
{
    if(!strcmp(ns,"math")&&!strcmp(name,"roundup"))return "ceil";
    if(!strcmp(ns,"math")&&!strcmp(name,"rounddown"))return "floor";
    if(!strcmp(ns,"io.file")&&!strcmp(name,"readline"))return "read_line";
    if(!strcmp(ns,"time")&&!strcmp(name,"isoformat"))return "iso_format";
    return NULL;
}
