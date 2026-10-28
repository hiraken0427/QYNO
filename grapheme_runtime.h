#ifndef QYNO_GRAPHEME_H
#define QYNO_GRAPHEME_H
#include <stdint.h>
/* Strict UTF-8; offsets/lengths are bytes, indices/counts are graphemes.
 * A missing selection has offset UINT64_MAX and width 0. */
int grapheme_scan(const unsigned char *s,uint64_t length,int64_t index,
                  uint64_t *offset,uint64_t *width,uint64_t *count);
uint64_t grapheme_cursor(const unsigned char *s,uint64_t length,uint64_t cursor,int direction);
#endif
