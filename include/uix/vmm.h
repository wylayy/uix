/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX virtual memory manager: 4-level paging, 4KiB pages.
 * Own CR3 with kernel higher-half + HHDM; per-task address spaces in M3. */

#ifndef UIX_VMM_H
#define UIX_VMM_H

#include <uix/types.h>

/* flags for vmm_map */
#define VMM_PRESENT  (1u << 0)
#define VMM_WRITE    (1u << 1)
#define VMM_USER     (1u << 2)
#define VMM_NOEXEC   (1ull << 63)

void vmm_init(void);

/* create a new address space that shares the kernel half; returns the
 * PML4 physical address */
paddr_t vmm_create_space(void);
void vmm_switch(paddr_t pml4_pa);

/* fork: deep-copy the user half (PML4..PT), marking writable user
 * pages read-only in BOTH spaces for copy-on-write. Returns the new
 * PML4 physical address. */
paddr_t vmm_fork_user_space(void);

/* map one 4KiB page: va and pa must be page-aligned */
void vmm_map(u64 va, paddr_t pa, u64 flags);
void vmm_unmap(u64 va);
int vmm_translate(u64 va, paddr_t *pa_out); /* 0 on success */

/* page fault handler hook (called from isr_dispatch, vector 14);
 * returns 0 if the fault was handled (COW), -1 otherwise */
int vmm_page_fault(u64 fault_addr, u64 err);

/* kernel scratch area for early allocations before the heap exists */
void *vmm_early_alloc_pages(u64 count); /* returns virtual address, zeroed */

#endif /* UIX_VMM_H */
