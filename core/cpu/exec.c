/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX execve: replace the calling task's user image with one of the
 * embedded flat binaries. Static "process table" of two programs. */

#include <uix/heap.h>
#include <uix/kprintf.h>
#include <uix/lib.h>
#include <uix/pmm.h>
#include <uix/task.h>
#include <uix/uixfs.h>
#include <uix/user.h>
#include <uix/vmm.h>

extern const u8 user_program_start[], user_program_end[];
extern const u8 user_hello2_start[], user_hello2_end[];

struct uxn {
    const char *name;
    const u8 *start;
    const u8 *end;
};

static const struct uxn programs[] = {
    { "/sh",     user_program_start, user_program_end },
    { "/hello2", user_hello2_start,  user_hello2_end },
};

/* loaded-from-disk image (owned here, freed after the copy) */
static void *disk_img;
static long disk_size;

/* free the page tables of the user half and drop page refs */
static void free_user_tables(u64 *table, int level)
{
    for (int i = 0; i < 512; i++) {
        u64 e = table[i];
        if (!(e & VMM_PRESENT))
            continue;
        if (level > 0) {
            free_user_tables(pmm_phys_to_virt(e & 0x000FFFFFFFFFF000ULL),
                             level - 1);
            pmm_free(e & 0x000FFFFFFFFFF000ULL);
        } else {
            pmm_unref(e & 0x000FFFFFFFFFF000ULL); /* shared COW-aware */
        }
    }
}

/* called from the syscall dispatcher; on success the task never
 * returns to the old image (fresh user_enter trampoline) */
int do_execve(struct task *t, const char *path)
{
    const u8 *img = NULL;
    u64 len = 0;

    /* embedded table first (leading '/') */
    for (u32 i = 0; i < sizeof(programs) / sizeof(programs[0]); i++)
        if (strcmp(path, programs[i].name) == 0) {
            img = programs[i].start;
            len = (u64)(programs[i].end - programs[i].start);
            break;
        }

    /* then the disk (bare name, e.g. "hello2") */
    if (!img) {
        void *dimg;
        long dsize = uixfs_read(path, &dimg);
        if (dsize < 0) {
            kprintf(KLOG_ERR "execve: no such program '%s'\n", path);
            return -2; /* -ENOENT */
        }
        disk_img = dimg;
        disk_size = dsize;
        img = dimg;
        len = (u64)dsize;
    }

    extern paddr_t kernel_cr3;

    /* free the old user tables (frees only tables; leaf pages are
     * COW-shared and left alone: ceiling until page refcounts, M6) */
    if (t->cr3) {
        vmm_switch(t->cr3);
        u64 *old = pmm_phys_to_virt(t->cr3);
        for (int i = 0; i < 256; i++)
            if (old[i] & VMM_PRESENT) {
                free_user_tables(
                    pmm_phys_to_virt(old[i] & 0x000FFFFFFFFFF000ULL), 2);
                pmm_free(old[i] & 0x000FFFFFFFFFF000ULL);
            }
        pmm_free(t->cr3);
    }

    /* fresh space + image (same layout as user_task_create) */
    t->cr3 = vmm_create_space();
    vmm_switch(t->cr3);

    u64 off = 0;
    while (off < len) {
        paddr_t pg = pmm_alloc();
        vmm_map(USER_CODE_BASE + off, pg,
                VMM_PRESENT | VMM_WRITE | VMM_USER);
        u64 chunk = len - off;
        if (chunk > UIX_PAGE_SIZE)
            chunk = UIX_PAGE_SIZE;
        memcpy((void *)(USER_CODE_BASE + off), img + off, chunk);
        off += UIX_PAGE_SIZE;
    }
    for (u64 va = USER_STACK_TOP - UIX_PAGE_SIZE;
         va >= USER_STACK_TOP - USER_STACK_SIZE; va -= UIX_PAGE_SIZE) {
        paddr_t pg = pmm_alloc();
        vmm_map(va, pg, VMM_PRESENT | VMM_WRITE | VMM_USER);
    }
    /* stay on the new space: the trampoline iretq's into ring 3
     * immediately; the scheduler switches CR3 on the next task change */

    if (disk_img) {
        kfree(disk_img); /* kernel heap is visible in every space */
        disk_img = NULL;
    }

    /* reset the kernel stack to the user_launch trampoline frame */
    extern void user_launch_trampoline(void);
    u64 sp = t->kstack + t->kstack_size;
    sp &= ~0xFULL;
    sp -= 8;  *(u64 *)sp = (u64)user_launch_trampoline;
    sp -= 8;  *(u64 *)sp = 0;
    sp -= 8;  *(u64 *)sp = 0;
    sp -= 8;  *(u64 *)sp = (u64)USER_CODE_BASE;           /* r12: rip */
    sp -= 8;  *(u64 *)sp = (u64)(USER_STACK_TOP - 16);    /* r13: rsp */
    sp -= 8;  *(u64 *)sp = 0x202;                          /* r14: rflags */
    sp -= 8;  *(u64 *)sp = 0;                              /* r15 */
    t->rsp = sp;

    /* hijack the syscall return: switch straight to the trampoline.
     * The old rsp value is dropped (never resumed). */
    {
        u64 dummy_rsp;
        extern void switch_to(u64 *prev_rsp, u64 next_rsp);
        switch_to(&dummy_rsp, t->rsp);
    }
    __builtin_unreachable();
}
