/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX 16550 UART driver (COM1, 115200 8N1). QEMU maps it to -serial stdio. */

#include <uix/serial.h>

/* port addresses are below 4G; use natural-width port io */
static inline void outb(u16 port, u8 val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline u8 inb(u16 port)
{
    u8 v;
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

#define COM1 0x3F8

#define REG_DATA   0
#define REG_IER    1
#define REG_FCR    2
#define REG_LCR    3
#define REG_MCR    4
#define REG_LSR    5

#define LSR_THRE   0x20 /* transmit holding register empty */

void serial_init(void)
{
    outb(COM1 + REG_IER, 0x00);    /* disable interrupts */
    outb(COM1 + REG_LCR, 0x80);    /* DLAB on */
    outb(COM1 + REG_DATA, 0x01);   /* divisor = 1 -> 115200 baud */
    outb(COM1 + REG_IER, 0x00);
    outb(COM1 + REG_LCR, 0x03);    /* 8N1, DLAB off */
    outb(COM1 + REG_FCR, 0xC7);    /* FIFO on, clear, 14-byte threshold */
    outb(COM1 + REG_MCR, 0x0B);    /* DTR + RTS + OUT2 */
}

void serial_putc(char c)
{
    while (!(inb(COM1 + REG_LSR) & LSR_THRE))
        ;
    outb(COM1 + REG_DATA, (u8)c);
}
