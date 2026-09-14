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

prompt:
    /* print "uix> " */
    mov $1, %rax
    mov $1, %rdi
    lea promptstr(%rip), %rsi
    mov $5, %rdx
    int $0x80

read_again:
    /* line = read(0, rsp, 128); n in rax -> save in r12 */
    mov $4, %rax
    xor %rdi, %rdi
    mov %rsp, %rsi
    mov $128, %rdx
    int $0x80

    test %rax, %rax
    jle idle
    mov %rax, %r12

    /* echo it back: write(1, "got: ", 5) then n bytes of the line */
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
    /* nothing yet: yield, retry WITHOUT reprinting the prompt */
    mov $3, %rax
    int $0x80
    jmp read_again

.section .rodata
banner:    .ascii "uix echo!\nready:\n"
promptstr: .ascii "uix> "
gotstr:    .ascii "got: "

.set STACK_TOP, 0x800000
