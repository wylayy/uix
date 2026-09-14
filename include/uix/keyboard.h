/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX PS/2 keyboard driver (IRQ1 via IOAPIC, scancode set 1). */

#ifndef UIX_KEYBOARD_H
#define UIX_KEYBOARD_H

#include <uix/types.h>

void keyboard_init(void);
void keyboard_irq(void); /* called on IRQ1 */

/* consume up to len chars from the keyboard buffer (non-blocking) */
u32 keyboard_read(char *buf, u32 len);

#endif /* UIX_KEYBOARD_H */
