/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX scheduler: round-robin runqueue, preemption from the timer tick. */

#include <uix/sched.h>
#include <uix/apic.h>
#include <uix/heap.h>
#include <uix/kprintf.h>
#include <uix/lib.h>

extern void task_set_current(struct task *t);

/* TSS from cpu.S (gdt.c owns the layout) */
void tss_set_rsp0(u64 rsp);

static struct task *runqueue_head;
static struct task *runqueue_tail;
static u64 rq_size;

static u64 ticks_since_switch;
#define TIME_SLICE 2 /* ticks per task switch */

void sched_enqueue(struct task *t)
{
    t->next = NULL;
    if (runqueue_tail)
        runqueue_tail->next = t;
    else
        runqueue_head = t;
    runqueue_tail = t;
    rq_size++;
}

static struct task *dequeue(void)
{
    struct task *t = runqueue_head;
    if (!t)
        return NULL;
    runqueue_head = t->next;
    if (!runqueue_head)
        runqueue_tail = NULL;
    t->next = NULL;
    rq_size--;
    return t;
}

void sched_init(void)
{
    runqueue_head = runqueue_tail = NULL;
}

void schedule(void)
{
    struct task *prev = task_current();

    /* reap the dead task we just left (cannot free own stack mid-use) */
    static struct task *zombie;
    if (zombie && zombie != prev) {
        kfree((void *)zombie->kstack);
        kfree(zombie);
        zombie = NULL;
    }

    struct task *next = dequeue();
    if (!next) {
        if (prev->state == TASK_DEAD) {
            /* nothing else to run: halt forever */
            for (;;)
                __asm__ volatile ("hlt");
        }
        return; /* keep running prev */
    }

    if (prev->state == TASK_RUNNING) {
        prev->state = TASK_READY;
        sched_enqueue(prev);
    } else if (prev->state == TASK_DEAD) {
        zombie = prev;
    }

    next->state = TASK_RUNNING;
    task_set_current(next);

    /* ring-0 tasks run on their own stacks; TSS.RSP0 matters for ring-3
     * entry (M4) but keep it accurate anyway */
    tss_set_rsp0(next->kstack + next->kstack_size);

    switch_to(&prev->rsp, next->rsp);

    /* execution resumes here when this task is switched back in */
    if (zombie && zombie != task_current()) {
        kfree((void *)zombie->kstack);
        kfree(zombie);
        zombie = NULL;
    }
}

void sched_tick(void)
{
    if (++ticks_since_switch < TIME_SLICE)
        return;
    ticks_since_switch = 0;
    schedule();
}

void task_yield(void)
{
    ticks_since_switch = 0;
    schedule();
}

void sched_start(void)
{
    kprintf(KLOG_INFO "sched: %u tasks, starting preemptive round-robin\n",
            (u32)task_count());
    schedule();
    /* we are the bootstrap task again (or never left): behave as idle */
    /* we are the bootstrap task again (or never left): behave as idle */
    for (;;)
        __asm__ volatile ("hlt");
}
