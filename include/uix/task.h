/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX tasks: kernel threads now, userspace threads in M4. */

#ifndef UIX_TASK_H
#define UIX_TASK_H

#include <uix/types.h>

/* task states */
#define TASK_READY  0
#define TASK_RUNNING 1
#define TASK_DEAD   2
#define TASK_ZOMBIE 3
#define TASK_BLOCKED 4

/* per-process open file descriptors (BSD-style ofile array) */
#define PROC_MAX_FDS 16

enum fd_type {
    FD_NONE = 0,
    FD_CONSOLE_IN,  /* keyboard/tty */
    FD_CONSOLE_OUT, /* serial+fb console */
};

struct proc_fd {
    u8 type;    /* enum fd_type */
    u8 in_use;
};

struct task {
    u64 pid;
    u64 rsp;          /* saved kernel stack pointer */
    u64 kstack;       /* kernel stack base (kmalloc'd) */
    u64 kstack_size;
    paddr_t cr3;      /* 0 = share the kernel space */
    int state;
    int is_user;
    int exit_status;  /* valid when TASK_ZOMBIE */
    u64 ppid;         /* parent pid for wait4 */
    u64 block_on;     /* pid we are waiting on when TASK_BLOCKED */
    const char *name;
    struct proc_fd fds[PROC_MAX_FDS];
    struct task *next;      /* runqueue / blocked-list link */
    struct task *next_all;  /* all-tasks registry link */
};

void task_init(void);                 /* creates the bootstrap (idle) task */

/* fd helpers: return fd number or negative errno */
int proc_fd_alloc(struct task *t, u8 type);
void proc_fd_free(struct task *t, int fd);
int proc_fd_type(const struct task *t, int fd);

/* spawn a kernel thread running fn(arg); returns the task */
struct task *task_create(const char *name, void (*fn)(void *), void *arg);
void task_exit(void) __attribute__((noreturn));

struct task *task_current(void);
u64 task_count(void);

/* arch */
extern void switch_to(u64 *prev_rsp, u64 next_rsp);
extern void task_entry_trampoline(void);

#endif /* UIX_TASK_H */
