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

    /* "hello2\n" -> fork+exec disk/embedded "hello2" */
    lea cmd_hello2(%rip), %rdi
    mov %rsp, %rsi
    mov $7, %rcx
    repe cmpsb
    test %rcx, %rcx
    jnz try_run
    /* strip the trailing newline into a NUL */
    movb $0, 6(%rsp)
    jmp do_exec

try_run:
    /* "run x\n" -> fork+exec disk file "x" */
    lea cmd_run(%rip), %rdi
    mov %rsp, %rsi
    mov $4, %rcx
    repe cmpsb
    test %rcx, %rcx
    jnz echo_back

    /* copy "x\n" over the line start: shift by -0? name starts
     * right after "run " (4 chars); copy from rsp+4 to rsp until
     * newline, NUL-terminate */
    mov %rsp, %rdi
    lea 4(%rsp), %rsi
strip_loop:
    movzbq (%rsi), %rax
    cmp $10, %al
    je strip_done
    mov %al, (%rdi)
    inc %rdi
    inc %rsi
    jmp strip_loop
strip_done:
    movb $0, (%rdi)

do_exec:
    /* fork(); child execs */
    mov $7, %rax
    int $0x80
    test %rax, %rax
    jnz shell_parent

    /* child: execve(line, 0, 0); on failure exit(1) */
    mov $8, %rax
    mov %rsp, %rdi
    xor %rsi, %rsi
    xor %rdx, %rdx
    int $0x80
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
cmd_run: .ascii "run "

.set STACK_TOP, 0x800000
