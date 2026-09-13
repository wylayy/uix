/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX console: serial 16550 UART + Limine framebuffer (8x16 cells, scroll). */

#ifndef UIX_CONSOLE_H
#define UIX_CONSOLE_H

#include <uix/types.h>

void console_init(void);
void console_putc(char c);
void console_write(const char *s);
void console_puts(const char *s); /* write + newline */

#endif /* UIX_CONSOLE_H */
