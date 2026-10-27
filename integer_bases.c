#include "qyc.h"
#include <stdlib.h>

long long integer_bits(uint64_t n)
{
    return n <= INT64_MAX ? (long long)n : -1 - (long long)(UINT64_MAX - n);
}
int integer_digit(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
int integer_prefix(int c)
{
    switch (c) {
    case 'b': case 'B': return 2;
    case 'o': case 'O': return 8;
    case 'd': case 'D': return 10;
    case 'x': case 'X': return 16;
    default: return 0;
    }
}
int integer_base_valid(long long base, int automatic)
{
    return (automatic && base == 0) || base == 2 || base == 8 || base == 10 || base == 16;
}
/* Whole text, unlike the token parser used for delimited input. Space/tab
 * trimming and optional signs match the language's existing int input. */
int integer_read(const char *s, size_t len, int base, long long *value)
{
    size_t i = 0, digits; int negative = 0, detected, digit; uint64_t n = 0, limit;
    while (i < len && (s[i] == ' ' || s[i] == '\t')) i++;
    while (len > i && (s[len-1] == ' ' || s[len-1] == '\t')) len--;
    if (i < len && (s[i] == '+' || s[i] == '-')) negative = s[i++] == '-';
    if (len - i >= 2 && s[i] == '0' && (detected = integer_prefix((unsigned char)s[i+1]))) {
        if (base && base != detected) return 1;
        base = detected; i += 2;
    }
    if (!base) base = 10;
    if (!integer_base_valid(base, 0)) return 1;
    limit = base == 10 ? (uint64_t)INT64_MAX + (unsigned)negative : UINT64_MAX;
    digits = i;
    for (; i < len; i++) {
        digit = integer_digit((unsigned char)s[i]);
        if (digit < 0 || digit >= base) return 1;
        if (n > (limit - (unsigned)digit) / (unsigned)base) return 2;
        n = n * (unsigned)base + (unsigned)digit;
    }
    if (digits == i) return 1;
    *value = integer_bits(negative ? UINT64_C(0) - n : n); return 0;
}
char *integer_format(long long value, int base, int prefix, int *length)
{
    char digits[64], *s; int count = 0, i, pos = 0, negative = base == 10 && value < 0;
    uint64_t n = (uint64_t)value;
    if (negative) n = UINT64_C(0) - n;
    if (!integer_base_valid(base, 0)) return NULL;
    do { digits[count++] = "0123456789abcdef"[n % (unsigned)base]; n /= (unsigned)base; } while (n);
    s = malloc((size_t)count + 4); if (!s) fatal("整数の文字列を確保できません");
    if (negative) s[pos++] = '-';
    if (prefix) { s[pos++] = '0'; s[pos++] = base == 2 ? 'b' : base == 8 ? 'o' : base == 10 ? 'd' : 'x'; }
    for (i = count - 1; i >= 0; i--) s[pos++] = digits[i];
    s[pos] = '\0'; *length = pos; return s;
}
