/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX serial 16550 UART driver interface (port I/O). */

#ifndef UIX_SERIAL_H
#define UIX_SERIAL_H

#include <uix/types.h>

void serial_init(void);
void serial_putc(char c);

#endif /* UIX_SERIAL_H */
