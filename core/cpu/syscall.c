/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX syscalls: int 0x80, vector 128. rax = number, args rdi/rsi/rdx.
 * Return value in rax; errors are -errno (POSIX convention from day 1). */

#include <uix/syscall.h>
#include <uix/console.h>
#include <uix/idt.h>
#include <uix/kprintf.h>
#include <uix/sched.h>
#include <uix/task.h>

void syscall_init(void)
{
    /* gate 128 already exists (isr_stub_table covers all 256);
     * make it accessible from ring 3 */
    idt_gate_set_dpl3(128);
}

u64 syscall_dispatch(struct iframe *fr)
{
    struct task *t = task_current();

    switch (fr->rax) {
    case SYS_exit:
        kprintf(KLOG_INFO "pid %u exited with %u\n",
                (u32)t->pid, (u32)fr->rdi);
        t->state = TASK_DEAD;
        schedule(); /* never returns */
        __builtin_unreachable();

    case SYS_write: {
        /* write(fd, buf, len): fd ignored (console) for now */
        const char *buf = (const char *)fr->rsi;
        u64 len = fr->rdx;
        for (u64 i = 0; i < len; i++)
            console_putc(buf[i]);
        return len;
    }

    case SYS_getpid:
        return t->pid;

    case SYS_yield:
        task_yield();
        return 0;

    default:
        return (u64)-38; /* -ENOSYS */
    }
}
