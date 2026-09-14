/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX userspace program: mini shell (line echo with prompt).
 * Syscalls: 0=exit 1=write 2=getpid 3=yield 4=read. */

.section .text
.code64
.global entry_point
entry_point:
    mov $STACK_TOP, %rbp
    mov %rbp, %rsp
    sub $256, %rsp

    /* banner */
    mov $1, %rax
    mov $1, %rdi
    lea banner(%rip), %rsi
    mov $17, %rdx
    int $0x80

    /* pid = fork() */
    mov $7, %rax
    int $0x80

    test %rax, %rax
    jnz parent

    /* ---- child ---- */
    mov $1, %rax
    mov $1, %rdi
    lea childmsg(%rip), %rsi
    mov $13, %rdx
    int $0x80

    /* pid = getpid(); exit(pid) */
    mov $2, %rax
    int $0x80
    mov %rax, %rdi
    mov $0, %rax
    int $0x80

parent:
    /* parent waits a bit via yields so the child prints first */
    mov $30, %r14
pw: mov $3, %rax
    int $0x80
    dec %r14
    jnz pw

    mov $1, %rax
    mov $1, %rdi
    lea parentmsg(%rip), %rsi
    mov $14, %rdx
    int $0x80
    /* fall into the shell loop */

prompt:
    mov $1, %rax
    mov $1, %rdi
    lea promptstr(%rip), %rsi
    mov $5, %rdx
    int $0x80

read_again:
    mov $4, %rax
    xor %rdi, %rdi
    mov %rsp, %rsi
    mov $128, %rdx
    int $0x80

    test %rax, %rax
    jle idle
    mov %rax, %r12

    mov $1, %rax
    mov $1, %rdi
    lea gotstr(%rip), %rsi
    mov $5, %rdx
    int $0x80

    mov $1, %rax
    mov $1, %rdi
    mov %rsp, %rsi
    mov %r12, %rdx
    int $0x80
    jmp prompt

idle:
    mov $3, %rax
    int $0x80
    jmp read_again

.section .rodata
banner:    .ascii "uix echo!\nready:\n"
promptstr: .ascii "uix> "
gotstr:    .ascii "got: "
childmsg:  .ascii "hello from k"
parentmsg: .ascii "hello from p\n"

.set STACK_TOP, 0x800000
