#include "qyc.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    Type element;
    int rank;
    char *name;
} ArrayType;

static ArrayType *types;
static int count;

int type_is_array(Type ty)
{
    return (int)ty >= TY_ARRAY;
}

Type array_type(Type element)
{
    int i;
    size_t n;
    ArrayType *grown;
    const char *name;
    for (i = 0; i < count; i++) {
        if (types[i].element == element) {
            return (Type)(TY_ARRAY + i);
        }
    }
    grown = realloc(types, (size_t)(count + 1) * sizeof(*types));
    if (grown == NULL) {
        fatal("配列型のメモリを確保できません");
    }
    types = grown;
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
