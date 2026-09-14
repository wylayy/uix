/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX scheduler: round-robin, preempted by the LAPIC timer tick. */

#ifndef UIX_SCHED_H
#define UIX_SCHED_H

#include <uix/task.h>

void sched_init(void);
void sched_start(void) __attribute__((noreturn));

/* queue a task on the runqueue */
void sched_enqueue(struct task *t);

/* pick the next task and switch to it (called from tick or yield) */
void schedule(void);

/* called from isr_dispatch on vector 32 */
void sched_tick(void);

void task_yield(void);

#endif /* UIX_SCHED_H */
