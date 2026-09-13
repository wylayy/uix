/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX libc freestanding: mem*, str*, angka-ke-string. */

#ifndef UIX_LIB_H
#define UIX_LIB_H

#include <uix/types.h>
#include <stdbool.h>

/* memori */
void *memset(void *dst, int c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
int   memcmp(const void *a, const void *b, size_t n);

/* string */
size_t strlen(const char *s);
char  *strcpy(char *dst, const char *src);
char  *strncpy(char *dst, const char *src, size_t n);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);

/* angka ke string, untuk kprintf %d %u %x */
char *uix_utoa(u64 v, char *buf, u32 base, bool upper);
char *uix_itoa(i64 v, char *buf, u32 base);

#endif /* UIX_LIB_H */
