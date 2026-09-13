/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX physical memory manager: bitmap allocator over Limine memmap. */

#include <uix/pmm.h>
#include <uix/vmm.h>
#include <uix/boot.h>
#include <uix/kprintf.h>
#include <uix/lib.h>

/* linker symbols */
extern u8 _kernel_phys_start[], _kernel_phys_end[];

/* bitmap: 1 = free, 0 = used/reserved. One bit per 4KiB page. */
static u64 *bitmap;
static u64 bitmap_len;      /* bits */
static u64 total_pages;
static u64 free_count;
static paddr_t bitmap_phys;

#define BM_WORD(i) ((i) >> 6)
#define BM_BIT(i) (1ull << ((i) & 63))

static void set_free(u64 i)   { bitmap[BM_WORD(i)] |= BM_BIT(i); }
static void set_used(u64 i)   { bitmap[BM_WORD(i)] &= ~BM_BIT(i); }
static bool is_free(u64 i)    { return bitmap[BM_WORD(i)] & BM_BIT(i); }

static u64 next_free_scan; /* allocation cursor */

void pmm_init(void)
{
    const struct uix_bootinfo *bi = boot_info();
    total_pages = 0;

    /* find top of memory */
    u64 max_pa = 0;
    for (u64 i = 0; i < bi->memmap_entries; i++) {
        struct limine_memmap_entry *e = bi->memmap[i];
        if (e->type != LIMINE_MEMMAP_USABLE)
            continue;
        u64 top = e->base + e->length;
        if (top > max_pa)
            max_pa = top;
    }
    total_pages = max_pa >> UIX_PAGE_SHIFT;
    bitmap_len = total_pages;

    /* borrow usable pages for the bitmap itself (early, before alloc) */
    u64 bm_bytes = (bitmap_len + 7) / 8;
    u64 bm_pages = (bm_bytes + UIX_PAGE_SIZE - 1) / UIX_PAGE_SIZE;

    paddr_t bm_page = 0;
    for (u64 i = 0; i < bi->memmap_entries; i++) {
        struct limine_memmap_entry *e = bi->memmap[i];
        if (e->type == LIMINE_MEMMAP_USABLE &&
            e->length >= (bm_pages + 1) * UIX_PAGE_SIZE) {
            bm_page = e->base;
            break;
        }
    }
    if (!bm_page)
        panic("pmm: no usable memory for bitmap (%u pages)", (u32)bm_pages);

    bitmap = (u64 *)pmm_phys_to_virt(bm_page);
    bitmap_phys = bm_page;
    memset(bitmap, 0, bm_bytes);

    /* mark usable regions free */
    free_count = 0;
    for (u64 i = 0; i < bi->memmap_entries; i++) {
        struct limine_memmap_entry *e = bi->memmap[i];
        if (e->type != LIMINE_MEMMAP_USABLE)
            continue;
        u64 first = e->base >> UIX_PAGE_SHIFT;
        u64 count = e->length >> UIX_PAGE_SHIFT;
        for (u64 p = first; p < first + count; p++) {
            set_free(p);
            free_count++;
        }
    }

    /* reserve every page the bitmap occupies */
    for (u64 p = 0; p < bm_pages; p++) {
        set_used((bm_page >> UIX_PAGE_SHIFT) + p);
        free_count--;
    }

    kprintf(KLOG_INFO "pmm: %u pages (%u MiB), bitmap %u KiB @ %p\n",
            (u32)total_pages, (u32)(total_pages >> 8),
            (u32)((bitmap_len + 7) / 8 / 1024), (void *)bitmap_phys);
}

paddr_t pmm_alloc(void)
{
    for (u64 n = 0; n < bitmap_len; n++) {
        u64 i = (next_free_scan + n) % bitmap_len;
        if (is_free(i)) {
            set_used(i);
            next_free_scan = (i + 1) % bitmap_len;
            free_count--;
            return i << UIX_PAGE_SHIFT;
        }
    }
    return 0;
}

void pmm_free(paddr_t page)
{
    u64 i = page >> UIX_PAGE_SHIFT;
    if (i >= bitmap_len || is_free(i))
        panic("pmm: double free or bad page %p", (void *)page);
    set_free(i);
    free_count++;
}

u64 pmm_free_pages(void)
{
    return free_count;
}

void *pmm_phys_to_virt(paddr_t pa)
{
    return (void *)(boot_info()->hhdm + pa);
}

paddr_t pmm_virt_to_phys(void *va)
{
    return (paddr_t)((u64)va - boot_info()->hhdm);
}
