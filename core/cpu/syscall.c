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
        /* write(fd, buf, len): console only for now */
        int fd = (int)fr->rdi;
        const char *buf = (const char *)fr->rsi;
        u64 len = fr->rdx;
        if (proc_fd_type(t, fd) != FD_CONSOLE_OUT)
            return (u64)-9; /* -EBADF */
        for (u64 i = 0; i < len; i++)
            console_putc(buf[i]);
        return len;
    }

    case SYS_getpid:
        return t->pid;

    case SYS_read: {
        /* read(fd, buf, len): tty line via fd 0 (non-blocking poll) */
        int fd = (int)fr->rdi;
        char *buf = (char *)fr->rsi;
        u64 len = fr->rdx;
        if (proc_fd_type(t, fd) != FD_CONSOLE_IN)
            return (u64)-9; /* -EBADF */
        if (len < 1)
            return 0;
        extern void tty_poll(void);
        extern u32 tty_readline(char *, u32);
        tty_poll();
        return tty_readline(buf, (u32)len);
    }

    case SYS_close: {
        int fd = (int)fr->rdi;
        if (proc_fd_type(t, fd) == FD_NONE)
            return (u64)-9;
        proc_fd_free(t, fd);
        return 0;
    }

    case SYS_open: {
        /* open(path, flags): only "/dev/console" (rdwr console) */
        const char *path = (const char *)fr->rdi;
        static const char cons[] = "/dev/console";
        u32 i = 0;
        while (i < sizeof(cons) && path[i] == cons[i]) {
            if (path[i] == '\0')
                break;
            i++;
        }
        if (path[i] != '\0' || cons[i] != '\0')
            return (u64)-2; /* -ENOENT */
        return proc_fd_alloc(t, FD_CONSOLE_IN); /* console is rdwr */
    }

    case SYS_fork: {
        extern struct task *do_fork(struct task *, struct iframe *);
        struct task *child = do_fork(t, fr);
        if (!child)
            return (u64)-12; /* -ENOMEM */
        return child->pid;   /* child returns 0 via its frame copy */
    }

    case SYS_execve: {
        /* execve(path, 0, 0): flat binaries, two embedded programs */
        const char *path = (const char *)fr->rdi;
        extern int do_execve(struct task *t, const char *path);
        int r = do_execve(t, path);
        return (u64)r; /* on success this never returns to user code */
    }

    case SYS_yield:
        task_yield();
        return 0;

    default:
        return (u64)-38; /* -ENOSYS */
    }
}
