/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX tasks: creation, exit, trampoline glue. */

#include <uix/task.h>
#include <uix/heap.h>
#include <uix/kprintf.h>
#include <uix/lib.h>
#include <uix/sched.h>

#define KSTACK_SIZE (16 * 1024)

static u64 next_pid = 1;

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
    t->pid = next_pid++;
    t->name = name;
    t->state = TASK_READY;
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

u64 task_count(void)
{
    return next_pid - 1;
}
