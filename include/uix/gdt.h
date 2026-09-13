/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX GDT/TSS/IDT interface (x86_64). */

#ifndef UIX_GDT_H
#define UIX_GDT_H

#include <uix/types.h>

/* segment selectors */
#define K_CS 0x08
#define K_DS 0x10
#define U_CS 0x18
#define U_DS 0x20
#define TSS_SEL 0x28

void gdt_init(void);

/* interrupt stack table slots (IST1..IST3) */
#define IST_DF  0 /* double fault */
#define IST_NMI 1
#define IST_PF  2 /* page fault on kernel stacks (debug aid) */

#endif /* UIX_GDT_H */
