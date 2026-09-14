/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX TTY: line discipline over the PS/2 keyboard buffer.
 * Echo moves here (driver no longer echoes); read() returns one
 * line at a time, backspace edits, CR is returned as LF. */

#ifndef UIX_TTY_H
#define UIX_TTY_H

#include <uix/types.h>

void tty_init(void);

/* poll the raw keyboard buffer; call echo + line assembly */
void tty_poll(void);

/* copy out at most len-1 chars of the current line (LF-terminated)
 * into buf, NUL-terminated; returns length including LF, 0 if no
 * complete line yet */
u32 tty_readline(char *buf, u32 len);

#endif /* UIX_TTY_H */
