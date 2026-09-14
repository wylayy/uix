/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX TTY: line discipline (echo + line assembly) over the keyboard. */

#include <uix/tty.h>
#include <uix/console.h>
#include <uix/keyboard.h>
#include <uix/lib.h>

#define LINE_MAX 128

static char line[LINE_MAX];
static u32 line_len;
static int line_ready; /* a '\n' has been pressed */

void tty_init(void)
{
    line_len = 0;
    line_ready = 0;
}

void tty_poll(void)
{
    char raw[32];
    u32 n = keyboard_read(raw, sizeof(raw));

    for (u32 i = 0; i < n; i++) {
        char c = raw[i];

        if (c == '\r')
            c = '\n';

        if (c == '\b') {
            if (line_len > 0) {
                line_len--;
                console_putc('\b');
                console_putc(' ');
                console_putc('\b');
            }
            continue;
        }

        if (line_ready)
            continue; /* drop input until the line is consumed */

        if (c == '\n') {
            if (line_len < LINE_MAX - 1)
                line[line_len++] = '\n';
            line_ready = 1;
            console_putc('\n');
            continue;
        }

        if (line_len < LINE_MAX - 1) {
            line[line_len++] = c;
            console_putc(c); /* echo */
        }
    }
}

u32 tty_readline(char *buf, u32 len)
{
    if (!line_ready)
        return 0;

    u32 n = line_len;
    if (n > len - 1)
        n = len - 1;
    memcpy(buf, line, n);
    buf[n] = '\0';

    line_len = 0;
    line_ready = 0;
    return n;
}
