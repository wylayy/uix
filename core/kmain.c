/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX kmain: kernel C entry. */

#include <uix/apic.h>
#include <uix/boot.h>
#include <uix/console.h>
#include <uix/gdt.h>
#include <uix/heap.h>
#include <uix/idt.h>
#include <uix/keyboard.h>
#include <uix/kprintf.h>
#include <uix/lib.h>
#include <uix/pmm.h>
#include <uix/sched.h>
#include <uix/task.h>
#include <uix/vmm.h>

static void selftest_memory(void)
{
    u64 before = pmm_free_pages();

    /* PMM: alloc/free 64 pages */
    paddr_t pages[64];
    for (int i = 0; i < 64; i++) {
        pages[i] = pmm_alloc();
        if (!pages[i])
            panic("selftest: pmm OOM at %d", i);
        memset(pmm_phys_to_virt(pages[i]), 0xA5, UIX_PAGE_SIZE);
    }
    for (int i = 0; i < 64; i++)
        pmm_free(pages[i]);
    if (pmm_free_pages() != before)
        panic("selftest: pmm leak (%u != %u)",
              (u32)pmm_free_pages(), (u32)before);

    /* VMM: map a scratch page, write canary, translate, unmap */
    paddr_t p = pmm_alloc();
    u64 probe_va = 0xffff800100000000ULL; /* HHDM + 4GiB: unmapped area */
    vmm_map(probe_va, p, VMM_PRESENT | VMM_WRITE | VMM_NOEXEC);
    *(volatile u64 *)probe_va = 0xCAFEBABEDEADBEEFull;
    paddr_t back;
    if (vmm_translate(probe_va, &back) != 0 || back != p)
        panic("selftest: translate mismatch %p != %p", (void *)back, (void *)p);
    if (*(volatile u64 *)probe_va != 0xCAFEBABEDEADBEEFull)
        panic("selftest: canary lost");
    vmm_unmap(probe_va);
    pmm_free(p);

    /* heap: alloc, write, free */
    void *h1 = kmalloc(100);
    void *h2 = kmalloc(4096);
    if (!h1 || !h2)
        panic("selftest: kmalloc failed");
    strcpy(h1, "heap works");
    u64 h2sum = 0;
    u8 *bytes = h2;
    for (int i = 0; i < 4096; i++) {
        bytes[i] = (u8)i;
        h2sum += bytes[i];
    }
    if (h2sum != 522240) /* sum(0..255)*16 */
        panic("selftest: heap corruption sum=%u", (u32)h2sum);
    kfree(h1);
    kfree(h2);

    kprintf(KLOG_INFO "selftest: pmm/vmm/heap OK (%u pages free)\n",
            (u32)pmm_free_pages());
}

/* ---- M3 demo tasks ---- */

static volatile u32 task_a_count, task_b_count;

static void demo_task_a(void *arg)
{
    (void)arg;
    for (int i = 0; i < 5; i++) {
        kprintf("A%d ", i);
        task_a_count++;
        /* busy loop: proves preemption (no yield, no hlt) */
        for (volatile int j = 0; j < 4000000; j++)
            ;
    }
}

static void demo_task_b(void *arg)
{
    (void)arg;
    for (int i = 0; i < 5; i++) {
        kprintf("B%d ", i);
        task_b_count++;
        for (volatile int j = 0; j < 4000000; j++)
            ;
    }
}

static void demo_task_report(void *arg)
{
    (void)arg;
    /* waits (by polling) until both loops finish, then reports */
    while (task_a_count < 5 || task_b_count < 5)
        task_yield();
    kprintf(KLOG_INFO "tasks done: A=%u B=%u, free pages %u\n",
            task_a_count, task_b_count, (u32)pmm_free_pages());
    kprintf(KLOG_INFO "M3: preemptive scheduler with round-robin works.\n");
}

void kmain(void)
{
    console_init();
    boot_init();
    idt_init();
    pmm_init();
    vmm_init();
    heap_init();
    selftest_memory();
    apic_init();
    keyboard_init();

    task_init();
    sched_init();
    task_create("A", demo_task_a, NULL);
    task_create("B", demo_task_b, NULL);
    task_create("report", demo_task_report, NULL);

    __asm__ volatile ("sti");

    kprintf("\n");
    kprintf("uix v%d.%d: microkernel + POSIX personality\n", 0, 1);
    kprintf(KLOG_INFO "M3: scheduler, context switch, kernel tasks.\n");

    /* becomes the idle loop once all tasks block/exit */
    sched_start();
}
