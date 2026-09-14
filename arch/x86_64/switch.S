/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX context switch and new-task trampoline. */

.section .text
.code64

/* void switch_to(u64 *prev_rsp, u64 next_rsp)
 * save callee-saved regs on the old stack, swap RSP, restore. */
.global switch_to
.type switch_to, @function
switch_to:
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %rsp, (%rdi)     /* prev->rsp = current sp */
    movq %rsi, %rsp       /* switch stacks */

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    ret
.size switch_to, . - switch_to

/* new tasks "return" here from switch_to; r12 = fn, r13 = arg */
.global task_entry_trampoline
.type task_entry_trampoline, @function
task_entry_trampoline:
    movq %r13, %rdi
    call *%r12
    call task_exit        /* fn returned: terminate */
    hlt
.size task_entry_trampoline, . - task_entry_trampoline
