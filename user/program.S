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

    /* try commands: "hello2" -> fork+exec /hello2 */
    lea cmd_hello2(%rip), %rdi
    mov %rsp, %rsi
    mov $7, %rcx
    repe cmpsb
    test %rcx, %rcx
    jnz echo_back

    /* fork(); child execs /hello2 */
    mov $7, %rax
    int $0x80
    test %rax, %rax
    jnz shell_parent

    /* child: execve("/hello2", 0, 0) */
    mov $8, %rax
    lea path_hello2(%rip), %rdi
    xor %rsi, %rsi
    xor %rdx, %rdx
    int $0x80
    /* execve failed: exit(1) */
    mov $0, %rax
    mov $1, %rdi
    int $0x80

shell_parent:
    /* wait4(-1, 0, 0, 0): block until the child exits */
    mov $9, %rax
    mov $-1, %rdi
    xor %rsi, %rsi
    xor %rdx, %rdx
    int $0x80
    jmp prompt

echo_back:
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
cmd_hello2: .ascii "hello2\n"
path_hello2: .ascii "/hello2"

.set STACK_TOP, 0x800000
