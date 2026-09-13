/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX kprintf implementation. */


#include <uix/kprintf.h>
#include <uix/console.h>
#include <uix/lib.h>

static void emit_pad(char c, int n)
{
    while (n-- > 0)
        console_putc(c);
}

void kvprintf(const char *fmt, va_list ap)
{
    char buf[32];

    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            console_putc(*fmt);
            continue;
        }

        fmt++;

        /* field width: %5d etc. */
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        /* length modifier (parse and ignore: we promote to 64-bit) */
        while (*fmt == 'l' || *fmt == 'h' || *fmt == 'z')
            fmt++;

        char spec = *fmt;
        const char *s;
        int len;

        switch (spec) {
        case 'c':
            emit_pad(' ', width - 1);
            console_putc((char)va_arg(ap, int));
            break;
        case 's':
            s = va_arg(ap, const char *);
            if (!s)
                s = "(null)";
            len = (int)strlen(s);
            emit_pad(' ', width - len);
            console_write(s);
            break;
        case 'd':
        case 'i': {
            i64 v = va_arg(ap, i64);
            uix_itoa(v, buf, 10);
            len = (int)strlen(buf);
            if (v < 0)
                width--; /* sign occupies a pad slot */
            emit_pad(' ', width - len);
            console_write(buf);
            break;
        }
        case 'u':
            uix_utoa(va_arg(ap, u64), buf, 10, false);
            len = (int)strlen(buf);
            emit_pad(' ', width - len);
            console_write(buf);
            break;
        case 'x':
            uix_utoa(va_arg(ap, u64), buf, 16, false);
            len = (int)strlen(buf);
            emit_pad(' ', width - len);
            console_write(buf);
            break;
        case 'X':
            uix_utoa(va_arg(ap, u64), buf, 16, true);
            len = (int)strlen(buf);
            emit_pad(' ', width - len);
            console_write(buf);
            break;
        case 'p':
            console_write("0x");
            uix_utoa((u64)va_arg(ap, void *), buf, 16, false);
            len = (int)strlen(buf);
            emit_pad('0', 16 - len);
            console_write(buf);
            break;
        case '%':
            console_putc('%');
            break;
        default:
            console_putc('%');
            console_putc(spec);
            break;
        }
    }
}

void kprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
}

void kputs(const char *s)
{
    console_write(s);
    console_putc('\n');
}

void kputc(char c)
{
    console_putc(c);
}

__attribute__((noreturn, format(printf, 1, 2)))
void panic(const char *fmt, ...)
{
    va_list ap;
    console_write(KLOG_PANIC);
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
    console_putc('\n');

    __asm__ volatile ("cli");
    for (;;)
        __asm__ volatile ("hlt");
}
