/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX second userspace program: executed via execve() from the shell.
 * Prints a banner, spins briefly, exits. Syscalls: 0=exit 1=write
 * 2=getpid 3=yield. */

.section .text
.code64
.global entry_point
entry_point:
    mov $STACK_TOP, %rbp
    mov %rbp, %rsp
    sub $128, %rsp

    /* write(1, banner, 20) */
    mov $1, %rax
    mov $1, %rdi
    lea banner(%rip), %rsi
    mov $20, %rdx
    int $0x80

    /* marker = getpid(); write single digit */
    mov $2, %rax
    int $0x80
    movb $'0', (%rsp)
    add %al, (%rsp)
    mov $1, %rax
    mov $1, %rdi
    mov %rsp, %rsi
    mov $1, %rdx
    int $0x80

    /* write(1, tail, 10) */
    mov $1, %rax
    mov $1, %rdi
    lea tail(%rip), %rsi
    mov $10, %rdx
    int $0x80

    /* exit(7) */
    mov $0, %rax
    mov $7, %rdi
    int $0x80

2:  hlt
    jmp 2b

.section .rodata
banner: .ascii "[exec] hello2 pid="
tail:   .ascii " exiting\n"

.set STACK_TOP, 0x800000
