/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * UIX kernel 64-bit entry point (x86_64, Limine protocol).
 * Limine hands us: long mode, higher half mapping, valid GDT (with TSS),
 * a usable stack. We just switch to our own stack and call kmain.
 *
 * Our own GDT/TSS is deferred to M1 (needed for IDT + IST); loading a GDT
 * without a matching TSS descriptor breaks Limine's live TR selector.
 */

.section .bss, "aw", @nobits
.align 16
stack_bottom:
    .skip 65536 /* 64 KiB */
stack_top:

.section .text.boot
.code64
.global _start
.type _start, @function
_start:
    movabs $stack_top, %rsp
    xorl %ebp, %ebp

    /* Limine guarantees 16-byte stack alignment at call boundary */
    call kmain

    /* kmain never returns; if it does, halt forever */
    cli
1:  hlt
    jmp 1b
.size _start, . - _start
