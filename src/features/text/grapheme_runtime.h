#ifndef QYNO_GRAPHEME_H
#define QYNO_GRAPHEME_H
#include <stdint.h>
typedef struct {
    const unsigned char *s;
    uint64_t length,pos,start;
    unsigned prev,ri,ep,zwj_ep,indic,linker;
    int started,white;
} GraphemeIterator;
void grapheme_begin(GraphemeIterator *it,const unsigned char *s,uint64_t length);
/* 1: next cluster, 0: end, -1: malformed UTF-8. */
int grapheme_next(GraphemeIterator *it,uint64_t *offset,uint64_t *width,int *white);
/* Strict UTF-8; offsets/lengths are bytes, indices/counts are graphemes.
 * A missing selection has offset UINT64_MAX and width 0. */
int grapheme_scan(const unsigned char *s,uint64_t length,int64_t index,
                  uint64_t *offset,uint64_t *width,uint64_t *count);
uint64_t grapheme_cursor(const unsigned char *s,uint64_t length,uint64_t cursor,int direction);
#endif
