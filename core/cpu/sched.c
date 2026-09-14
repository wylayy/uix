/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX scheduler: round-robin runqueue, preemption from the timer tick. */

#include <uix/sched.h>
#include <uix/apic.h>
#include <uix/heap.h>
#include <uix/kprintf.h>
#include <uix/lib.h>
#include <uix/vmm.h>

extern void task_set_current(struct task *t);

/* TSS from cpu.S (gdt.c owns the layout) */
void tss_set_rsp0(u64 rsp);

static struct task *runqueue_head;
static struct task *runqueue_tail;
static u64 rq_size;

static u64 ticks_since_switch;
static volatile int need_resched;
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

void sched_enqueue_user(struct task *t)
{
    extern u64 task_alloc_pid(void);
    t->pid = task_alloc_pid();
    sched_enqueue(t);
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

    /* switch address space if the next task has its own */
    extern paddr_t kernel_cr3;
    if (next->cr3 && next->cr3 != kernel_cr3)
        vmm_switch(next->cr3);

    switch_to(&prev->rsp, next->rsp);

    /* We may have been switched from inside an interrupt gate (IF=0);
     * re-enable so this task (and the idle hlt loop) can be preempted
     * again. iretq on the way out restores the task's own RFLAGS. */
    __asm__ volatile ("sti");

    /* execution resumes here when this task is switched back in */
    if (zombie && zombie != task_current()) {
        kfree((void *)zombie->kstack);
        kfree(zombie);
        zombie = NULL;
    }
}

void sched_tick(void)
{
    if (!need_resched && ++ticks_since_switch < TIME_SLICE)
        return;
    need_resched = 0;
    ticks_since_switch = 0;
    schedule();
}

/* yield does not switch by itself: switching out of a software-int
 * frame reliably kills LAPIC delivery on this QEMU. Instead mark the
 * task reschedulable; the next timer tick performs the switch (the
 * timer-interrupt switch path is proven stable). */
void task_yield(void)
{
    ticks_since_switch = 0;
    need_resched = 1;
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
