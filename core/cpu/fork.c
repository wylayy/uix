/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX fork: duplicate a user task (address space COW, fd table copy,
 * kernel stack seeded with the parent's syscall frame, rax=0). */

#include <uix/heap.h>
#include <uix/idt.h>
#include <uix/kprintf.h>
#include <uix/lib.h>
#include <uix/sched.h>
#include <uix/task.h>
#include <uix/vmm.h>

struct task *do_fork(struct task *parent, struct iframe *fr)
{

    struct task *child = kzalloc(sizeof(*child));
    if (!child)
        return NULL;
    child->kstack = (u64)kmalloc(16 * 1024);
    if (!child->kstack) {
        kfree(child);
        return NULL;
    }
    child->kstack_size = 16 * 1024;
    child->is_user = 1;
    child->state = TASK_READY;
    child->name = "forked";
    child->ppid = parent->pid;

    {
        extern void task_register(struct task *t);
        task_register(child);
    }

    /* address space: deep copy user half, COW-mark shared pages */
    child->cr3 = vmm_fork_user_space();

    /* fd table: copy (no refcount needed while console-only) */
    for (int i = 0; i < PROC_MAX_FDS; i++)
        child->fds[i] = parent->fds[i];

    /* kernel stack: copy the parent's iframe, patch rax=0 (child),
     * then wrap it in a switch_to frame that "returns" through
     * isr_common's epilogue (pop GPRs, add rsp, iretq). */
    u64 sp = child->kstack + child->kstack_size;
    sp &= ~0xFULL;

    /* the full CPU frame (GPRs + int_no/err + rip..ss), rax -> 0 */
    u64 frsize = sizeof(struct iframe);
    sp -= frsize;
    memcpy((void *)sp, fr, frsize);
    ((struct iframe *)sp)->rax = 0;

    /* isr_common epilogue after dispatch returns:
     * popq x15, addq $16, iretq. Simulate entering dispatch's return
     * by pushing a fake switch_to frame that rets into a stub doing
     * exactly that. */
    extern void fork_child_stub(void);
    sp -= 8;  *(u64 *)sp = (u64)fork_child_stub; /* ret addr */
    sp -= 8;  *(u64 *)sp = 0;                    /* rbp */
    sp -= 8;  *(u64 *)sp = 0;                    /* rbx */
    sp -= 8;  *(u64 *)sp = 0;                    /* r12 */
    sp -= 8;  *(u64 *)sp = 0;                    /* r13 */
    sp -= 8;  *(u64 *)sp = 0;                    /* r14 */
    sp -= 8;  *(u64 *)sp = 0;                    /* r15 */
    child->rsp = sp;

    sched_enqueue_user(child);
    kprintf(KLOG_INFO "fork: pid %u -> %u\n",
            (u32)parent->pid, (u32)child->pid);
    return child;
}
