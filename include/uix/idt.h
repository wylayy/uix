/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX IDT and exception/interrupt dispatch. */

#ifndef UIX_IDT_H
#define UIX_IDT_H

#include <uix/types.h>

/* CPU trap frame pushed by entry stubs (cpu.S) */
struct iframe {
    u64 r15, r14, r13, r12, r11, r10, r9, r8;
    u64 rdi, rsi, rbp, rbx, rdx, rcx, rax;
    u64 int_no, err;
    u64 rip, cs, rflags, rsp, ss;
};

void idt_init(void);

/* called from asm stubs; one C dispatch for all vectors */
void isr_dispatch(struct iframe *fr);

#endif /* UIX_IDT_H */
