/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX physical memory manager: bitmap allocator over Limine memmap. */

#ifndef UIX_PMM_H
#define UIX_PMM_H

#include <uix/types.h>

void pmm_init(void);

paddr_t pmm_alloc(void);          /* returns 0 when out of memory */
paddr_t pmm_alloc_pages(u64 n);   /* contiguous run of n pages */
void pmm_free(paddr_t page);

/* page refcounts for copy-on-write sharing */
void pmm_ref(paddr_t page);
void pmm_unref(paddr_t page);
u16 pmm_getref(paddr_t page);
u64 pmm_free_pages(void);

/* direct physical access through the HHDM */
void *pmm_phys_to_virt(paddr_t pa);
paddr_t pmm_virt_to_phys(void *va);

#endif /* UIX_PMM_H */
