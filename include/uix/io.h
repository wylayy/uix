/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX x86_64 port I/O and MMIO helpers. */

#ifndef UIX_IO_H
#define UIX_IO_H

#include <uix/types.h>

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

static inline void outw(u16 port, u16 val)
{
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline u16 inw(u16 port)
{
    u16 v;
    __asm__ volatile ("inw %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void outl(u16 port, u32 val)
{
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline u32 inl(u16 port)
{
    u32 v;
    __asm__ volatile ("inl %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void io_wait(void)
{
    outb(0x80, 0);
}

static inline void wrmmio32(volatile void *addr, u32 val)
{
    *(volatile u32 *)addr = val;
}

static inline u32 rdmmio32(volatile void *addr)
{
    return *(volatile u32 *)addr;
}

#endif /* UIX_IO_H */
