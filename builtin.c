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
    { "math", "roundup" },
    { "math", "rounddown" },
    { NULL, "int" },
    { NULL, "float" },
    { NULL, "double" },
    { NULL, "char" },
    { NULL, "str" },
    { NULL, "complex" },
    { "io", "print" },
    { "io", "input" },
    { "io.file", "open" },
    { "io.file", "close" },
    { "io.file", "is_open" },
    { "io.file", "read" },
    { "io.file", "readline" },
    { "io.file", "write" },
    { "io.file", "exists" }
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
