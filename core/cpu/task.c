/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX tasks: creation, exit, trampoline glue. */

#include <uix/task.h>
#include <uix/heap.h>
#include <uix/kprintf.h>
#include <uix/lib.h>
#include <uix/sched.h>

#define KSTACK_SIZE (16 * 1024)

static u64 next_pid = 1;

/* registry of all tasks (for wait4/zombie lookup) */
static struct task *all_tasks;

void task_register(struct task *t)
{
    t->next_all = all_tasks;
    all_tasks = t;
}

struct task *task_find_zombie(u64 ppid, u64 want_pid)
{
    for (struct task *t = all_tasks; t; t = t->next_all)
        if (t->state == TASK_ZOMBIE && t->ppid == ppid &&
            (want_pid == (u64)-1 || t->pid == want_pid))
            return t;
    return NULL;
}

void task_reap_children(u64 ppid)
{
    /* drop registry entries of reaped/dead children of ppid */
    struct task **pp = &all_tasks;
    while (*pp) {
        if ((*pp)->state == TASK_DEAD && (*pp)->ppid == ppid) {
            struct task *t = *pp;
            *pp = t->next_all;
            continue;
        }
        pp = &(*pp)->next_all;
    }
}

u64 task_alloc_pid(void)
{
    return next_pid++;
}

/* the task we are currently executing (bootstrap task set by sched_init) */
static struct task *current;

struct task *task_current(void)
{
    return current;
}

static struct task *task_alloc(const char *name)
{
    struct task *t = kzalloc(sizeof(*t));
    if (!t)
        return NULL;
    t->kstack = (u64)kmalloc(KSTACK_SIZE);
    if (!t->kstack) {
        kfree(t);
        return NULL;
    }
    t->kstack_size = KSTACK_SIZE;
    t->pid = task_alloc_pid();
    t->name = name;
    t->state = TASK_READY;
    task_register(t);
    return t;
}

void task_init(void)
{
    /* bootstrap task: we are already running on the kernel stack from
     * cpu.S; just record it so task_current() works */
    current = task_alloc("bootstrap");
    if (!current)
        panic("task: cannot allocate bootstrap task");
    current->state = TASK_RUNNING;
}

struct task *task_create(const char *name, void (*fn)(void *), void *arg)
{
    struct task *t = task_alloc(name);
    if (!t)
        return NULL;

    /* build the initial stack so switch_to pops straight into the
     * trampoline. switch_to pops in this order: r15,r14,r13,r12,rbx,
     * rbp, then ret — so the stack (top to bottom) must be:
     *   [r15][r14][r13=arg][r12=fn][rbx][rbp][ret=trampoline] */
    u64 sp = t->kstack + t->kstack_size;
    sp &= ~0xFULL;

    sp -= 8;  *(u64 *)sp = (u64)task_entry_trampoline; /* ret addr */
    sp -= 8;  *(u64 *)sp = 0;               /* rbp */
    sp -= 8;  *(u64 *)sp = 0;               /* rbx */
    sp -= 8;  *(u64 *)sp = (u64)fn;         /* r12 */
    sp -= 8;  *(u64 *)sp = (u64)arg;        /* r13 */
    sp -= 8;  *(u64 *)sp = 0;               /* r14 */
    sp -= 8;  *(u64 *)sp = 0;               /* r15 */

    t->rsp = sp;
    sched_enqueue(t);
    return t;
}

void task_exit(void)
{
    current->state = TASK_DEAD;
    kprintf(KLOG_INFO "task '%s' (pid %u) exiting\n",
            current->name, (u32)current->pid);
    schedule(); /* never returns */
    __builtin_unreachable();
}
void task_set_current(struct task *t)
{
    current = t;
}

/* ---------------- fd table ---------------- */

int proc_fd_alloc(struct task *t, u8 type)
{
    for (int i = 0; i < PROC_MAX_FDS; i++) {
        if (!t->fds[i].in_use) {
            t->fds[i].type = type;
            t->fds[i].in_use = 1;
            return i;
        }
    }
    return -24; /* -EMFILE */
}

void proc_fd_free(struct task *t, int fd)
{
    if (fd >= 0 && fd < PROC_MAX_FDS)
        t->fds[fd].in_use = 0;
}

int proc_fd_type(const struct task *t, int fd)
{
    if (fd < 0 || fd >= PROC_MAX_FDS || !t->fds[fd].in_use)
        return 0; /* FD_NONE */
    return t->fds[fd].type;
}

u64 task_count(void)
{
    return next_pid - 1;
}

/* declared in user.h semantics; keeps task.c the pid owner */
