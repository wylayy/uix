/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * UIX x86_64 low-level CPU setup and interrupt entry stubs.
 */

.section .bss, "aw", @nobits
.align 16
stack_bottom:
    .skip 65536 /* 64 KiB kernel stack */
stack_top:

/* IST stacks for the nasty exceptions */
.align 16
.global ist_df_stack
ist_df_stack:
    .skip 16384
.global ist_nmi_stack
ist_nmi_stack:
    .skip 16384
.global ist_pf_stack
ist_pf_stack:
    .skip 16384

.section .text.boot
.code64
.global _start
.type _start, @function
_start:
    movabs $stack_top, %rsp
    xorl %ebp, %ebp

    call gdt_init          /* installs GDT + TSS (descriptors built in C) */

    call kmain

    cli
1:  hlt
    jmp 1b
.size _start, . - _start

/* ---------------- interrupt entry stubs ---------------- */

.section .text
.code64

/* one stub per vector. has_err: CPU pushed an error code */
.macro ISR vec, has_err=0
.global isr_\vec
isr_\vec:
    .if \has_err == 0
    pushq $0            /* dummy error code */
    .endif
    pushq $\vec
    jmp isr_common
.endm

/* CPU exceptions 0..31 */
ISR 0, 0
ISR 1, 0
ISR 2, 0
ISR 3, 0
ISR 4, 0
ISR 5, 0
ISR 6, 0
ISR 7, 0
ISR 8, 1
ISR 9, 0
ISR 10, 1
ISR 11, 1
ISR 12, 1
ISR 13, 1
ISR 14, 1
ISR 15, 0
ISR 16, 0
ISR 17, 1
ISR 18, 0
ISR 19, 0
ISR 20, 0
ISR 21, 0
ISR 22, 0
ISR 23, 0
ISR 24, 0
ISR 25, 0
ISR 26, 0
ISR 27, 0
ISR 28, 0
ISR 29, 0
ISR 30, 0
ISR 31, 0
/* external IRQs 32..47 (ISA 0..15) */
ISR 32, 0
ISR 33, 0
ISR 34, 0
ISR 35, 0
ISR 36, 0
ISR 37, 0
ISR 38, 0
ISR 39, 0
ISR 40, 0
ISR 41, 0
ISR 42, 0
ISR 43, 0
ISR 44, 0
ISR 45, 0
ISR 46, 0
ISR 47, 0
/* syscall vector */
ISR 128, 0

/* stub for vectors 48..255 (not routed; shouldn't fire) */
.global isr_unhandled
isr_unhandled:
    pushq $0
    pushq $0xFF
    jmp isr_common

/* table of stub addresses, indexed by vector */
.section .rodata
.global isr_stub_table
isr_stub_table:
    .quad isr_0, isr_1, isr_2, isr_3, isr_4, isr_5, isr_6, isr_7
    .quad isr_8, isr_9, isr_10, isr_11, isr_12, isr_13, isr_14, isr_15
    .quad isr_16, isr_17, isr_18, isr_19, isr_20, isr_21, isr_22, isr_23
    .quad isr_24, isr_25, isr_26, isr_27, isr_28, isr_29, isr_30, isr_31
    .quad isr_32, isr_33, isr_34, isr_35, isr_36, isr_37, isr_38, isr_39
    .quad isr_40, isr_41, isr_42, isr_43, isr_44, isr_45, isr_46, isr_47
    .rept 80
    .quad isr_unhandled          /* 48..127 */
    .endr
    .quad isr_128                /* syscall */
    .rept 127
    .quad isr_unhandled          /* 129..255 */
    .endr

/* common path: save regs, call C, restore */
.section .text
.type isr_common, @function
isr_common:
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rdi
    pushq %rsi
    pushq %rbp
    pushq %rbx
    pushq %rdx
    pushq %rcx
    pushq %rax

    movq %rsp, %rdi
    call isr_dispatch

    popq %rax
    popq %rcx
    popq %rdx
    popq %rbx
    popq %rbp
    popq %rsi
    popq %rdi
    popq %r8
    popq %r9
    popq %r10
    popq %r11
    popq %r12
    popq %r13
    popq %r14
    popq %r15
    addq $16, %rsp        /* drop int_no + err */
    iretq
.size isr_common, . - isr_common

/* syscalls use int 0x80 (IDT gate 128): the CPU switches to TSS.RSP0
 * automatically, so no GS-base or STAR setup is needed. Handled in
 * isr_dispatch like any other vector. */
