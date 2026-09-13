/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX kprintf: printf subset over serial + framebuffer console.
 * Supported: %c %s %d/%i %u %x %X %p %% and %<n> (field width pad).
 */

#ifndef UIX_KPRINTF_H
#define UIX_KPRINTF_H

#include <stdarg.h>
#include <uix/types.h>

void kprintf(const char *fmt, ...);
void kvprintf(const char *fmt, va_list ap);
void kputs(const char *s);  /* kprintf-style puts: appends \n, logs to both */
void kputc(char c);

/* Level prefixes, cheap logging for now: */
#define KLOG_INFO    "[info] "
#define KLOG_WARN    "[warn] "
#define KLOG_ERR     "[err ] "
#define KLOG_PANIC   "[PANIC] "

/* Fatal error. Halts forever. */
__attribute__((noreturn, format(printf, 1, 2)))
void panic(const char *fmt, ...);

#endif /* UIX_KPRINTF_H */
