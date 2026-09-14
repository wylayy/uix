/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX first userspace program: raw code at 0x400000, int 0x80 syscalls.
 * Syscall numbers: 0=exit 1=write 2=getpid 3=yield. */

.section .text
.code64
.global entry_point
entry_point:
    /* write(1, msg1, 6) */
    mov $1, %rax
    mov $1, %rdi
    lea msg1(%rip), %rsi
    mov $6, %rdx
    int $0x80

    /* yield loop (lazy: sets need_resched, the timer performs the switch) */
    mov $3, %r13
yield_loop:
    mov $3, %rax
    int $0x80
    dec %r13
    jnz yield_loop

    /* pid = getpid() */
    mov $2, %rax
    int $0x80
    mov %rax, %r12

    /* write(1, msg2, 7) */
    mov $1, %rax
    mov $1, %rdi
    lea msg2(%rip), %rsi
    mov $7, %rdx
    int $0x80

    /* write(1, msg3, 7) */
    mov $1, %rax
    mov $1, %rdi
    lea msg3(%rip), %rsi
    mov $7, %rdx
    int $0x80

    /* exit(pid) */
    mov $0, %rax
    mov %r12, %rdi
    int $0x80

2:  hlt
    jmp 2b

.section .rodata
msg1: .ascii "hello\n"
msg2: .ascii " world\n"
msg3: .ascii "bye!!\n\n"
