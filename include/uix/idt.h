/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX IDT and exception/interrupt dispatch. */

#ifndef UIX_IDT_H
#define UIX_IDT_H

#include <uix/types.h>

/* CPU trap frame pushed by entry stubs (cpu.S).
 * Layout matches the asm push order: r15 pushed first (highest
 * address), rax pushed last (lowest), then int_no, err, and the
 * CPU-pushed RIP/CS/RFLAGS/RSP/SS. */
struct iframe {
    u64 rax, rcx, rdx, rbx, rbp, rsi, rdi;
    u64 r8, r9, r10, r11, r12, r13, r14, r15;
    u64 int_no, err;
    u64 rip, cs, rflags, rsp, ss;
};

void idt_init(void);
void idt_gate_set_dpl3(u8 vec);

/* called from asm stubs; one C dispatch for all vectors */
void isr_dispatch(struct iframe *fr);

#endif /* UIX_IDT_H */
