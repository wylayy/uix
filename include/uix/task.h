/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX tasks: kernel threads now, userspace threads in M4. */

#ifndef UIX_TASK_H
#define UIX_TASK_H

#include <uix/types.h>

/* task states */
#define TASK_READY  0
#define TASK_RUNNING 1
#define TASK_DEAD   2

struct task {
    u64 pid;
    u64 rsp;          /* saved kernel stack pointer */
    u64 kstack;       /* kernel stack base (kmalloc'd) */
    u64 kstack_size;
    paddr_t cr3;      /* 0 = share the kernel space */
    int state;
    int is_user;
    const char *name;
    struct task *next; /* runqueue link */
};

void task_init(void);                 /* creates the bootstrap (idle) task */

/* spawn a kernel thread running fn(arg); returns the task */
struct task *task_create(const char *name, void (*fn)(void *), void *arg);
void task_exit(void) __attribute__((noreturn));

struct task *task_current(void);
u64 task_count(void);

/* arch */
extern void switch_to(u64 *prev_rsp, u64 next_rsp);
extern void task_entry_trampoline(void);

#endif /* UIX_TASK_H */
