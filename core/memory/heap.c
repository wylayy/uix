/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX kernel heap: kmalloc/kfree, first-fit free list over PMM pages.
 * Blocks < HEAP_MAX carry a small header; anything bigger is rejected
 * (take pages from the PMM directly instead). */

#include <uix/heap.h>
#include <uix/pmm.h>
#include <uix/vmm.h>
#include <uix/kprintf.h>
#include <uix/lib.h>

#define HEAP_VA_BASE   0xffffffff40000000ULL /* below kernel, above scratch */
#define HEAP_MAX       (32 * 1024)
#define SUPER_PAGES    16                    /* grow by 64 KiB chunks */

/* block header: size includes header; free blocks form a singly linked list */
struct block {
    u32 size;
    u32 magic;
    struct block *next; /* valid when free */
};

#define BMAGIC_FREE 0xFEEEF00D
#define BMAGIC_USED 0xDEADC0DE
#define HDR_SIZE    ((u32)sizeof(struct block))

static struct block *freelist;
static u64 heap_brk; /* next unbacked virtual page */

static void grow(u64 npages)
{
    for (u64 i = 0; i < npages; i++) {
        paddr_t p = pmm_alloc();
        if (!p)
            panic("heap: OOM growing");
        vmm_map(heap_brk, p, VMM_PRESENT | VMM_WRITE | VMM_NOEXEC);
        heap_brk += UIX_PAGE_SIZE;
    }
}

void heap_init(void)
{
    heap_brk = HEAP_VA_BASE;
    grow(SUPER_PAGES);

    /* one big free block spanning the whole region */
    freelist = (struct block *)HEAP_VA_BASE;
    freelist->size = SUPER_PAGES * UIX_PAGE_SIZE;
    freelist->magic = BMAGIC_FREE;
    freelist->next = NULL;

    kprintf(KLOG_INFO "heap: %u KiB ready\n",
            (u32)(SUPER_PAGES * UIX_PAGE_SIZE / 1024));
}

void *kmalloc(size_t n)
{
    if (n == 0 || n + HDR_SIZE > HEAP_MAX)
        return NULL;

    u32 need = (u32)n + HDR_SIZE;
    struct block **prev = &freelist;
    for (struct block *b = freelist; b; prev = &b->next, b = b->next) {
        if (b->magic != BMAGIC_FREE)
            panic("heap: corrupted freelist");
        if (b->size >= need) {
            /* split if the remainder is worth keeping */
            if (b->size - need >= HDR_SIZE + 16) {
                struct block *split = (struct block *)((u8 *)b + need);
                split->size = b->size - need;
                split->magic = BMAGIC_FREE;
                split->next = b->next;
                b->size = need;
                *prev = split;
            } else {
                *prev = b->next;
            }
            b->magic = BMAGIC_USED;
            b->next = NULL;
            return (u8 *)b + HDR_SIZE;
        }
    }

    /* no fit: grow and retry once */
    grow(SUPER_PAGES);
    struct block *nb = (struct block *)(heap_brk - SUPER_PAGES * UIX_PAGE_SIZE);
    nb->size = SUPER_PAGES * UIX_PAGE_SIZE;
    nb->magic = BMAGIC_FREE;
    nb->next = freelist;
    freelist = nb;
    return kmalloc(n);
}

void kfree(void *p)
{
    if (!p)
        return;

    struct block *b = (struct block *)((u8 *)p - HDR_SIZE);
    if (b->magic != BMAGIC_USED)
        panic("heap: kfree of non-heap pointer %p", p);

    b->magic = BMAGIC_FREE;
    /* coalesce with adjacent free blocks by re-scanning (simple, slow, fine) */
    struct block *arr[256];
    u32 count = 0;
    for (struct block *it = freelist; it && count < 256; it = it->next)
        arr[count++] = it;
    arr[count++] = b;

    /* sort by address (insertion sort, n is tiny) */
    for (u32 i = 1; i < count; i++) {
        struct block *key = arr[i];
        i64 j = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }

    /* merge neighbors and rebuild list */
    freelist = NULL;
    struct block *tail = NULL;
    for (u32 i = 0; i < count; i++) {
        struct block *cur = arr[i];
        if (tail && (u8 *)tail + tail->size == (u8 *)cur) {
            tail->size += cur->size;
            continue;
        }
        cur->next = NULL;
        if (!tail)
            freelist = cur;
        else
            tail->next = cur;
        tail = cur;
    }
}

void *kzalloc(size_t n)
{
    void *p = kmalloc(n);
    if (p)
        memset(p, 0, n);
    return p;
}
