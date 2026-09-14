/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX userspace task launch. */

#include <uix/user.h>
#include <uix/heap.h>
#include <uix/pmm.h>
#include <uix/sched.h>
#include <uix/task.h>
#include <uix/vmm.h>
#include <uix/kprintf.h>
#include <uix/lib.h>

/* the first user instruction runs here from isr_common's iretq */
extern void user_launch_trampoline(void);

/* asm helper: enter ring 3 by pushing a full interrupt frame and iretq */
extern void user_enter(u64 rip, u64 rsp, u64 rflags);

struct task *user_task_create(const char *name, const void *code, u64 len)
{
    struct task *t = kzalloc(sizeof(*t));
    if (!t)
        return NULL;
    t->kstack = (u64)kmalloc(16 * 1024);
    if (!t->kstack) {
        kfree(t);
        return NULL;
    }
    t->kstack_size = 16 * 1024;
    t->pid = 0; /* assigned by sched path below */
    t->state = TASK_READY;
    t->is_user = 1;
    t->name = name;

    /* fresh address space sharing the kernel half */
    t->cr3 = vmm_create_space();

    /* map user code (must switch to the new space to fill it) */
    extern paddr_t kernel_cr3;
    vmm_switch(t->cr3);
    u64 off = 0;
    while (off < len) {
        paddr_t p = pmm_alloc();
        vmm_map(USER_CODE_BASE + off, p,
                VMM_PRESENT | VMM_WRITE | VMM_USER);
        u64 chunk = len - off;
        if (chunk > UIX_PAGE_SIZE)
            chunk = UIX_PAGE_SIZE;
        memcpy((void *)(USER_CODE_BASE + off),
               (const u8 *)code + off, chunk);
        off += UIX_PAGE_SIZE;
    }
    /* map user stack (RW, grows down from USER_STACK_TOP) */
    for (u64 va = USER_STACK_TOP - UIX_PAGE_SIZE;
         va >= USER_STACK_TOP - USER_STACK_SIZE; va -= UIX_PAGE_SIZE) {
        paddr_t p = pmm_alloc();
        vmm_map(va, p, VMM_PRESENT | VMM_WRITE | VMM_USER);
    }
    vmm_switch(kernel_cr3);

    /* kernel-side initial stack: switch_to frame + user_enter frame */
    u64 sp = t->kstack + t->kstack_size;
    sp &= ~0xFULL;

    /* switch_to pops: r15,r14,r13,r12,rbx,rbp,ret */
    sp -= 8;  *(u64 *)sp = (u64)user_launch_trampoline; /* ret addr */
    sp -= 8;  *(u64 *)sp = 0;               /* rbp */
    sp -= 8;  *(u64 *)sp = 0;               /* rbx */
    sp -= 8;  *(u64 *)sp = (u64)USER_CODE_BASE; /* r12: rip */
    sp -= 8;  *(u64 *)sp = (u64)(USER_STACK_TOP - 16); /* r13: rsp */
    sp -= 8;  *(u64 *)sp = 0x202;             /* r14: rflags (IF|1) */
    sp -= 8;  *(u64 *)sp = 0;               /* r15 */
    t->rsp = sp;

    sched_enqueue_user(t);
    return t;
}
