/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX PS/2 keyboard driver (IRQ1 via IOAPIC, scancode set 1). */

#ifndef UIX_KEYBOARD_H
#define UIX_KEYBOARD_H

void keyboard_init(void);
void keyboard_irq(void); /* called on IRQ1 */

#endif /* UIX_KEYBOARD_H */
