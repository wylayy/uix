/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX kernel heap: kmalloc/kfree over PMM-backed superblocks,
 * first-fit free list. Small allocations only (< 32 KiB); bigger
 * requests should take pages directly from the PMM. */

#ifndef UIX_HEAP_H
#define UIX_HEAP_H

#include <uix/types.h>

void heap_init(void);

void *kmalloc(size_t n);
void kfree(void *p);
void *kzalloc(size_t n);

#endif /* UIX_HEAP_H */
