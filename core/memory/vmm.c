/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX virtual memory manager: 4-level paging, 4KiB pages, own CR3.
 *
 * Address space layout (per address space, kernel shared):
 *   0x0000000000000000 .. user (M4+)
 *   0xffff800000000000 .. HHDM (direct map of first 4 GiB)
 *   0xffffffff80000000 .. kernel image + heap
 */

#include <uix/vmm.h>
#include <uix/pmm.h>
#include <uix/boot.h>
#include <uix/kprintf.h>
#include <uix/lib.h>

#define PML4_IDX(va) (((va) >> 39) & 0x1FF)
#define PDPT_IDX(va) (((va) >> 30) & 0x1FF)
#define PD_IDX(va)   (((va) >> 21) & 0x1FF)
#define PT_IDX(va)   (((va) >> 12) & 0x1FF)

#define KERN_VA_BASE 0xffffffff80000000ULL

/* scratch pages handed out before the heap exists (PMM + bitmap itself) */
#define SCRATCH_VA_BASE 0xffffffff30000000ULL
#define SCRATCH_PAGES  256
static u8 scratch_used[SCRATCH_PAGES];

static u64 *pml4; /* virtual, in HHDM */

#define PTE_MASK   0x000FFFFFFFFFF000ULL
#define PDE_PS     (1u << 7)
#define PDE2M_MASK 0x000FFFFFFFE00000ULL

/* returns the PT for va, or NULL. Fails on 2MiB pages (they have no PT). */
static u64 *pt_for(u64 va, bool create)
{
    if (!(pml4[PML4_IDX(va)] & VMM_PRESENT)) {
        if (!create)
            return NULL;
        paddr_t t = pmm_alloc();
        if (!t)
            panic("vmm: OOM building tables");
        memset(pmm_phys_to_virt(t), 0, UIX_PAGE_SIZE);
        pml4[PML4_IDX(va)] = t | VMM_PRESENT | VMM_WRITE | VMM_USER;
    }
    u64 *pdpt = pmm_phys_to_virt(pml4[PML4_IDX(va)] & PTE_MASK);

    u64 pdpte = pdpt[PDPT_IDX(va)];
    if (!(pdpte & VMM_PRESENT)) {
        if (!create)
            return NULL;
        paddr_t t = pmm_alloc();
        if (!t)
            panic("vmm: OOM building tables");
        memset(pmm_phys_to_virt(t), 0, UIX_PAGE_SIZE);
        pdpt[PDPT_IDX(va)] = t | VMM_PRESENT | VMM_WRITE | VMM_USER;
        pdpte = pdpt[PDPT_IDX(va)];
    }
    if (pdpte & PDE_PS) /* 1GiB page, unsupported here */
        return NULL;
    u64 *pd = pmm_phys_to_virt(pdpte & PTE_MASK);

    u64 pde = pd[PD_IDX(va)];
    if (pde & VMM_PRESENT && pde & PDE_PS)
        return NULL; /* 2MiB page: no PT to walk */
    if (!(pde & VMM_PRESENT)) {
        if (!create)
            return NULL;
        paddr_t t = pmm_alloc();
        if (!t)
            panic("vmm: OOM building tables");
        memset(pmm_phys_to_virt(t), 0, UIX_PAGE_SIZE);
        pd[PD_IDX(va)] = t | VMM_PRESENT | VMM_WRITE | VMM_USER;
    }
    return pmm_phys_to_virt(pd[PD_IDX(va)] & PTE_MASK); /* the PT */
}

static u64 *table_for(u64 va, bool create)
{
    return pt_for(va, create);
}

void vmm_map(u64 va, paddr_t pa, u64 flags)
{
    u64 *pt = table_for(va, true);
    u64 *pte = &pt[PT_IDX(va)];
    if (*pte & VMM_PRESENT)
        panic("vmm: remap of present page %p", (void *)va);
    *pte = (pa & 0x000FFFFFFFFFF000ULL) | flags;
    __asm__ volatile ("invlpg (%0)" : : "r"(va) : "memory");
}

void vmm_unmap(u64 va)
{
    u64 *pt = table_for(va, false);
    if (!pt)
        return;
    u64 *pte = &pt[PT_IDX(va)];
    *pte = 0;
    __asm__ volatile ("invlpg (%0)" : : "r"(va) : "memory");
}

int vmm_translate(u64 va, paddr_t *pa_out)
{
    if (!(pml4[PML4_IDX(va)] & VMM_PRESENT))
        return -1;
    u64 *pdpt = pmm_phys_to_virt(pml4[PML4_IDX(va)] & PTE_MASK);
    u64 pdpte = pdpt[PDPT_IDX(va)];
    if (!(pdpte & VMM_PRESENT))
        return -1;
    if (pdpte & PDE_PS) { /* 1GiB */
        *pa_out = (pdpte & 0x000FFFFFC0000000ULL) + (va & 0x3FFFFFFFULL);
        return 0;
    }
    u64 *pd = pmm_phys_to_virt(pdpte & PTE_MASK);
    u64 pde = pd[PD_IDX(va)];
    if (!(pde & VMM_PRESENT))
        return -1;
    if (pde & PDE_PS) { /* 2MiB */
        *pa_out = (pde & PDE2M_MASK) + (va & 0x1FFFFFULL);
        return 0;
    }
    u64 *pt = pmm_phys_to_virt(pde & PTE_MASK);
    u64 pte = pt[PT_IDX(va)];
    if (!(pte & VMM_PRESENT))
        return -1;
    *pa_out = pte & PTE_MASK;
    return 0;
}

void *vmm_early_alloc_pages(u64 count)
{
    if (count > SCRATCH_PAGES)
        return NULL;
    u64 run = 0;
    for (u64 i = 0; i < SCRATCH_PAGES; i++) {
        run = scratch_used[i] ? 0 : run + 1;
        if (run == count) {
            u64 first = i - count + 1;
            for (u64 j = first; j <= i; j++) {
                paddr_t p = pmm_alloc();
                if (!p)
                    panic("vmm: early alloc OOM");
                vmm_map(SCRATCH_VA_BASE + j * UIX_PAGE_SIZE, p,
                        VMM_PRESENT | VMM_WRITE | VMM_NOEXEC);
                scratch_used[j] = 1;
            }
            void *va = (void *)(SCRATCH_VA_BASE + first * UIX_PAGE_SIZE);
            memset(va, 0, count * UIX_PAGE_SIZE);
            return va;
        }
    }
    return NULL;
}

static void load_cr3(paddr_t pa)
{
    __asm__ volatile ("mov %0, %%cr3" : : "r"(pa) : "memory");
}

void vmm_switch(paddr_t pml4_pa)
{
    pml4 = pmm_phys_to_virt(pml4_pa);
    load_cr3(pml4_pa);
}

paddr_t vmm_create_space(void)
{
    /* fresh PML4 that shares every kernel-half (>= 0xffff800000000000)
     * entry with the current kernel space */
    paddr_t dst_pa = pmm_alloc();
    if (!dst_pa)
        panic("vmm: OOM creating address space");
    u64 *dst = pmm_phys_to_virt(dst_pa);
    memset(dst, 0, UIX_PAGE_SIZE);

    for (int i = 256; i < 512; i++)
        dst[i] = pml4[i];

    return dst_pa;
}

static u64 clone_table(u64 src_entry, int level)
{
    /* returns the physical address of the cloned table */
    u64 *src = pmm_phys_to_virt(src_entry & PTE_MASK);
    paddr_t dst = pmm_alloc();
    if (!dst)
        panic("vmm: OOM cloning tables");
    u64 *d = pmm_phys_to_virt(dst);

    for (int i = 0; i < 512; i++) {
        u64 e = src[i];
        if (!(e & VMM_PRESENT)) {
            d[i] = 0;
            continue;
        }
        if (level == 0 || (e & PDE_PS)) {
            /* PT leaf entries and 2MiB leaves: share as-is */
            d[i] = e;
        } else {
            /* interior pointer: clone the next level */
            u64 child = clone_table(e, level - 1);
            d[i] = child | (e & 0xFFF);
        }
    }
    return dst;
}

/* kernel address space (clone target), exported for the scheduler */
paddr_t kernel_cr3;

/* ---- fork: deep-copy the user half with COW marking ---- */

#define COW_MARK (1ull << 9) /* PTE "available0" bit: our COW marker */

/* copy one table level; for PT leaves: share the physical page and
 * strip W (and set our marker) on user pages, both in src and dst */
static void fork_table(u64 *src, u64 *dst, int level)
{
    for (int i = 0; i < 512; i++) {
        u64 e = src[i];
        if (!(e & VMM_PRESENT)) {
            dst[i] = 0;
            continue;
        }

        if (level == 0 || (level == 1 && (e & PDE_PS))) {
            /* leaf: share the page; user pages become COW */
            if (e & VMM_USER) {
                u64 cow = (e & ~VMM_WRITE) | COW_MARK;
                src[i] = cow; /* parent loses write too */
                dst[i] = cow;
            } else {
                dst[i] = e; /* kernel pages: share as-is */
            }
        } else {
            paddr_t t = pmm_alloc();
            if (!t)
                panic("vmm: OOM forking tables");
            u64 *nd = pmm_phys_to_virt(t);
            memset(nd, 0, UIX_PAGE_SIZE);
            fork_table(pmm_phys_to_virt(e & PTE_MASK), nd, level - 1);
            dst[i] = t | (e & 0xFFF);
        }
    }
}

paddr_t vmm_fork_user_space(void)
{
    /* called with the parent's CR3 active; pml4 points at it */
    paddr_t dst_pa = pmm_alloc();
    if (!dst_pa)
        panic("vmm: OOM forking PML4");
    u64 *dst = pmm_phys_to_virt(dst_pa);
    memset(dst, 0, UIX_PAGE_SIZE);

    /* kernel half (256..511) shared via direct copy of the pointers */
    for (int i = 256; i < 512; i++)
        dst[i] = pml4[i];

    /* user half: deep copy with COW */
    for (int i = 0; i < 256; i++) {
        if (pml4[i] & VMM_PRESENT) {
            paddr_t t = pmm_alloc();
            u64 *nd = pmm_phys_to_virt(t);
            memset(nd, 0, UIX_PAGE_SIZE);
            fork_table(pmm_phys_to_virt(pml4[i] & PTE_MASK), nd, 2);
            dst[i] = t | (pml4[i] & 0xFFF);
        }
    }
    return dst_pa;
}

void vmm_init(void)
{
    /* Clone the page tables Limine left active, then switch to the clone.
     * The clone is semantically identical, so the switch cannot fault.
     * New kernel mappings (scratch, heap, later per-task spaces) are then
     * created on top of it. */

    u64 cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));

    paddr_t pml4_pa = clone_table(cr3, 3);
    pml4 = pmm_phys_to_virt(pml4_pa);
    kernel_cr3 = pml4_pa;

    load_cr3(pml4_pa);

    kprintf(KLOG_INFO "vmm: switched to cloned CR3 @ phys %p\n",
            (void *)pml4_pa);
}

int vmm_page_fault(u64 fault_addr, u64 err)
{
    /* COW: user write to a present read-only marked page -> copy it.
     * The current space is the faulting task's (scheduler switched). */
    if ((err & 3) == 3 && (fault_addr < 0x0000800000000000ULL)) {
        u64 va = fault_addr & ~(UIX_PAGE_SIZE - 1);
        u64 *pt = pt_for(va, false);
        if (pt) {
            u64 pte = pt[PT_IDX(va)];
            if ((pte & VMM_PRESENT) && (pte & COW_MARK) &&
                (pte & VMM_USER)) {
                paddr_t newp = pmm_alloc();
                if (!newp)
                    panic("vmm: OOM in COW fault");
                /* copy the old page content through the HHDM */
                memcpy(pmm_phys_to_virt(newp),
                       pmm_phys_to_virt(pte & PTE_MASK),
                       UIX_PAGE_SIZE);
                pt[PT_IDX(va)] = (newp & PTE_MASK)
                               | VMM_PRESENT | VMM_WRITE | VMM_USER
                               | (pte & VMM_NOEXEC);
                __asm__ volatile ("invlpg (%0)" : : "r"(fault_addr) : "memory");
                return 0; /* handled: retry the faulting write */
            }
        }
    }

    /* not a COW fault we can fix: report (isr_dispatch panics next) */
    kprintf(KLOG_ERR "page fault: addr=%p err=%p %s%s%s%s\n",
            (void *)fault_addr, (void *)err,
            (err & 1) ? "present " : "non-present ",
            (err & 2) ? "write " : "read ",
            (err & 4) ? "user " : "kernel ",
            (err & 16) ? "instruction-fetch" : "data");
    return -1;
}

/* debug helper for drivers */
u64 *dbg_pml4(void) { return pml4; }
