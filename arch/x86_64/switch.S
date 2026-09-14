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

/* ---------------- ring-3 entry ---------------- */

/* first run of a user task: r12=rip, r13=rsp, r14=rflags */
.global user_launch_trampoline
.type user_launch_trampoline, @function
user_launch_trampoline:
    movq %r12, %rdi
    movq %r13, %rsi
    movq %r14, %rdx
    call user_enter
    hlt                        /* never returns */
.size user_launch_trampoline, . - user_launch_trampoline

/* enter ring 3 via iretq with a hand-built interrupt frame */
.global user_enter
.type user_enter, @function
user_enter:
    cli
    pushq $0x23                /* SS (user data) */
    pushq %rsi                 /* RSP */
    pushq %rdx                 /* RFLAGS */
    pushq $0x1b                /* CS (user code) */
    pushq %rdi                 /* RIP */
    mov $0x23, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    iretq
.size user_enter, . - user_enter

/* fork child: entered from switch_to with the copied iframe below.
 * Mimics returning from isr_dispatch: pop 15 GPRs, skip int_no+err,
 * iretq back to ring 3. */
.global fork_child_stub
.type fork_child_stub, @function
fork_child_stub:
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
    addq $16, %rsp        /* int_no + err */
    iretq
.size fork_child_stub, . - fork_child_stub
