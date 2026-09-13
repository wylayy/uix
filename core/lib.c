/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX freestanding libc. */

#include <uix/lib.h>

void *memset(void *dst, int c, size_t n)
{
    u8 *d = dst;
    while (n--)
        *d++ = (u8)c;
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    u8 *d = dst;
    const u8 *s = src;
    while (n--)
        *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
    u8 *d = dst;
    const u8 *s = src;
    if (d < s) {
        while (n--)
            *d++ = *s++;
    } else if (d > s) {
        d += n;
        s += n;
        while (n--)
            *--d = *--s;
    }
    return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const u8 *x = a, *y = b;
    while (n--) {
        if (*x != *y)
            return *x - *y;
        x++; y++;
    }
    return 0;
}

size_t strlen(const char *s)
{
    size_t n = 0;
    while (*s++)
        n++;
    return n;
}

char *strcpy(char *dst, const char *src)
{
    char *d = dst;
    while ((*d++ = *src++))
        ;
    return dst;
}

char *strncpy(char *dst, const char *src, size_t n)
{
    char *d = dst;
    while (n && (*d++ = *src++))
        n--;
    while (n--)
        *d++ = '\0';
    return dst;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++; b++;
    }
    return (u8)*a - (u8)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    while (n && *a && *a == *b) {
        a++; b++; n--;
    }
    if (n == 0)
        return 0;
    return (u8)*a - (u8)*b;
}

char *uix_utoa(u64 v, char *buf, u32 base, bool upper)
{
    static const char ld[] = "0123456789abcdef";
    static const char lu[] = "0123456789ABCDEF";
    const char *lut = upper ? lu : ld;
    char tmp[32];
    int i = 0;

    if (base < 2 || base > 16)
        base = 10;
    if (v == 0)
        tmp[i++] = '0';
    while (v) {
        tmp[i++] = lut[v % base];
        v /= base;
    }
    char *p = buf;
    while (i--)
        *p++ = tmp[i];
    *p = '\0';
    return buf;
}

char *uix_itoa(i64 v, char *buf, u32 base)
{
    if (base == 10 && v < 0) {
        *buf++ = '-';
        uix_utoa((u64)(-(v + 1)) + 1, buf, base, false); /* avoid INT64_MIN overflow */
        return buf - 1;
    }
    return uix_utoa((u64)v, buf, base, false);
}
