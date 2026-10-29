#ifndef QYNO_TIME_RUNTIME_H
#define QYNO_TIME_RUNTIME_H
#include "wide_runtime.h"
enum {
#define TIME_API(id,name,min,max,args,result) T_##id,
#include "time_api.def"
#undef TIME_API
    T_COUNT, T_WAIT_PARTS
};
typedef struct {
    int64_t year,month,day,hour,minute;
    WideBits second;
    int64_t offset_minutes;
} TimeDate;
typedef struct {
    int64_t op,status;
    WideBits args[3];
    TimeDate date;
    const char *text;
    int64_t length;
    const char *pattern;
    int64_t pattern_length;
    char *buffer;
    int64_t capacity;
    WideBits out;
    int64_t used;
} TimeRequest;
typedef char TimeDate_layout_check[sizeof(TimeDate)==64?1:-1];
typedef char TimeRequest_layout_check[sizeof(TimeRequest)==200?1:-1];
#endif
