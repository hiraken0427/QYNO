#include "qyc.h"

/* UTF-8 の最短表現だけを受け付け、サロゲート・範囲外・途中切れも拒否する。 */
int utf8_next(const unsigned char *s, size_t remaining, uint32_t *scalar, size_t *width)
{
    uint32_t value;
    uint32_t minimum;
    size_t n;
    size_t i;

    if (remaining == 0) {
        return 0;
    }
    value = s[0];
    if (value < 0x80) {
        n = 1;
        minimum = 0;
    } else if (value >= 0xC2 && value <= 0xDF) {
        n = 2;
        minimum = 0x80;
        value &= 0x1F;
    } else if (value >= 0xE0 && value <= 0xEF) {
        n = 3;
        minimum = 0x800;
        value &= 0x0F;
    } else if (value >= 0xF0 && value <= 0xF4) {
        n = 4;
        minimum = 0x10000;
        value &= 0x07;
    } else {
        return 0;
    }
    if (remaining < n) {
        return 0;
    }
    for (i = 1; i < n; i++) {
        if (s[i] < 0x80 || s[i] > 0xBF) {
            return 0;
        }
        value = (value << 6) | (s[i] & 0x3F);
    }
    if (value < minimum || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) {
        return 0;
    }
    *scalar = value;
    *width = n;
    return 1;
}
