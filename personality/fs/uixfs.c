/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIXFS read-only filesystem over the virtio-blk driver. */

#include <uix/uixfs.h>
#include <uix/heap.h>
#include <uix/kprintf.h>
#include <uix/lib.h>
#include <uix/pmm.h>
#include <uix/virtio_blk.h>

#define NFILES_MAX 32

struct uixfs_entry {
    char name[24];
    u32 size;
    u32 start_lba;
} __attribute__((packed));

static struct uixfs_entry entries[NFILES_MAX];
static u32 nfiles;
static int mounted;

static u8 sector[512] __attribute__((aligned(512)));

/* read-file result buffer: PMM pages via the HHDM (not the kernel
 * heap: exec can run on a child CR3 where heap growth misbehaved) */
static void *file_buf;
static u64 file_buf_pages;

static void file_buf_free(void)
{
    if (file_buf) {
        for (u64 i = 0; i < file_buf_pages; i++)
            pmm_free(pmm_virt_to_phys(file_buf) + i * UIX_PAGE_SIZE);
        file_buf = NULL;
        file_buf_pages = 0;
    }
}

static int file_buf_alloc(u32 size)
{
    file_buf_free();
    file_buf_pages = (size + UIX_PAGE_SIZE - 1) / UIX_PAGE_SIZE;
    paddr_t pa = pmm_alloc_pages(file_buf_pages);
    if (!pa) {
        file_buf_pages = 0;
        return -1;
    }
    file_buf = pmm_phys_to_virt(pa);
    return 0;
}

int uixfs_mount(void)
{
    if (virtio_blk_read(0, sector) != 0) {
        kprintf(KLOG_ERR "uixfs: cannot read LBA 0\n");
        return -1;
    }
    if (memcmp(sector, "UIXF", 4) != 0) {
        kprintf(KLOG_ERR "uixfs: bad magic\n");
        return -1;
    }
    nfiles = *(u32 *)(sector + 4);
    if (nfiles > NFILES_MAX)
        nfiles = NFILES_MAX;

    for (u32 i = 0; i < nfiles; i++) {
        /* entries start at LBA1; entry i may span sectors (32*16=512:
         * exactly 16 entries per sector) */
        u32 lba = 1 + (i * 32) / 512;
        u32 off = (i * 32) % 512;
        if (off == 0 && virtio_blk_read(lba, sector) != 0)
            return -1;
        memcpy(&entries[i], sector + off, sizeof(entries[i]));
    }

    mounted = 1;
    kprintf(KLOG_INFO "uixfs: mounted, %u files\n", nfiles);
    for (u32 i = 0; i < nfiles; i++)
        kprintf(KLOG_INFO "uixfs: '%s' %u bytes @ LBA %u\n",
                entries[i].name, entries[i].size, entries[i].start_lba);
    return 0;
}

void uixfs_release_buf(void)
{
    file_buf_free();
}

long uixfs_read(const char *name, void **out)
{
    if (!mounted)
        return -1;

    for (u32 i = 0; i < nfiles; i++) {
        if (strcmp(entries[i].name, name) != 0)
            continue;

        u32 size = entries[i].size;
        if (file_buf_alloc(size) != 0)
            return -12;

        u8 *buf = file_buf;
        u32 lba = entries[i].start_lba;
        u32 done = 0;
        while (done < size) {
            if (virtio_blk_read(lba, sector) != 0)
                return -5;
            u32 chunk = size - done;
            if (chunk > 512)
                chunk = 512;
            memcpy(buf + done, sector, chunk);
            done += chunk;
            lba++;
        }
        *out = buf;
        return size;
    }
    return -2; /* ENOENT */
}
