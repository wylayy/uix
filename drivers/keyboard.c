/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX PS/2 keyboard driver: scancode set 1, IRQ1 (vector 33). */

#include <uix/keyboard.h>
#include <uix/console.h>
#include <uix/io.h>
#include <uix/kprintf.h>

#define PS2_DATA 0x60
#define PS2_STAT 0x64

/* US QWERTY, scancode set 1, no shift map yet (M1: echo only) */
static const char map[128] = {
    0,   27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t','q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\','z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    0,   '*', 0,   ' ', 0,
};

void keyboard_init(void)
{
    /* flush pending output */
    while (inb(PS2_STAT) & 0x01)
        (void)inb(PS2_DATA);
}

void keyboard_irq(void)
{
    u8 sc = inb(PS2_DATA);

    if (sc & 0x80) /* key release: ignore */
        return;
    if (sc >= sizeof(map))
        return;

    char c = map[sc];
    if (c)
        console_putc(c);
}
