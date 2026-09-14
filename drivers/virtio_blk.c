/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX virtio-blk (legacy virtio-pci 1af4:1001): I/O-port access,
 * split virtqueue, polling completion. All DMA structures live in
 * PMM pages accessed through the HHDM (kernel heap pages proved
 * unreliable for translation here). */

#include <uix/virtio_blk.h>
#include <uix/pci.h>
#include <uix/pmm.h>
#include <uix/io.h>
#include <uix/kprintf.h>
#include <uix/lib.h>
#include <uix/vmm.h>

/* legacy virtio-pci register offsets from BAR0 (I/O) */
#define VIO_FEATURES  0x00
#define VIO_GUEST      0x04
#define VIO_QADDR      0x08
#define VIO_QNUM       0x0C
#define VIO_QSEL       0x0E
#define VIO_QNOTIFY    0x10
#define VIO_STATUS     0x12
#define VIO_ISR        0x13

#define VIRTIO_ACK       0x01
#define VIRTIO_DRIVER    0x02
#define VIRTIO_DRIVER_OK 0x04
#define VIRTIO_FAILED    0x80

#define VRING_DESC_F_NEXT  1
#define VRING_DESC_F_WRITE 2

struct vring_desc {
    u64 addr;
    u32 len;
    u16 flags;
    u16 next;
};

struct virtio_blk_req {
    u32 type;      /* 0 = IN, 1 = OUT */
    u32 reserved;
    u64 sector;
} __attribute__((packed));

static u16 io_base;
static u64 capacity_sectors;
static u16 qsize;

static struct vring_desc *desc;
static u16 *avail_idx_p, *avail_ring_p;
static volatile u16 *used_idx_p;

static paddr_t ring_pa;  /* DMA page (ring + hdr + status) */
static paddr_t buf_pa;   /* DMA page for sector data */
static u8 *buf_va;

/* runtime offsets (bytes) of hdr/status inside the ring allocation */
static u64 hdr_off, st_off;

static inline void vio_out(u16 off, u32 v, int width)
{
    if (width == 4)
        outl(io_base + off, v);
    else if (width == 2)
        outw(io_base + off, (u16)v);
    else
        outb(io_base + off, (u8)v);
}

static inline u32 vio_in(u16 off, int width)
{
    if (width == 4)
        return inl(io_base + off);
    if (width == 2)
        return inw(io_base + off);
    return inb(io_base + off);
}

static void set_status(u8 st)
{
    vio_out(VIO_STATUS, st, 1);
}

int virtio_blk_init(void)
{
    struct pci_dev dev;
    if (pci_find_by_id(0x1af4, 0x1001, &dev) != 0) {
        kprintf(KLOG_WARN "virtio-blk: device not found\n");
        return -1;
    }
    io_base = (u16)dev.bars[0];

    /* DMA requires bus mastering; I/O BAR access requires I/O decode */
    pci_enable_bus_master(&dev);

    set_status(0);
    set_status(VIRTIO_ACK);
    set_status(VIRTIO_ACK | VIRTIO_DRIVER);
    vio_out(0x04, 0, 4); /* guest features: none */

    vio_out(VIO_QSEL, 0, 2);
    qsize = vio_in(VIO_QNUM, 2);
    if (qsize == 0 || qsize > 256) {
        kprintf(KLOG_ERR "virtio-blk: bad queue size %u\n", qsize);
        set_status(VIRTIO_FAILED);
        return -1;
    }

    /* three contiguous pages: desc/avail/used packed, then data.
     * Legacy layout (virtio 0.9.5): avail immediately after desc
     * (16*q bytes), used ring 4-aligned after avail. */
    ring_pa = pmm_alloc_pages(4);
    if (!ring_pa)
        return -1;
    buf_pa = ring_pa + 3 * UIX_PAGE_SIZE;
    desc = pmm_phys_to_virt(ring_pa);
    buf_va = pmm_phys_to_virt(buf_pa);
    memset(desc, 0, 4 * UIX_PAGE_SIZE);

    u64 avail_off = 16 * qsize; /* flags idx ring[q] used_event */
    u16 *avail_base = (u16 *)((u8 *)desc + avail_off);
    avail_idx_p = avail_base + 1;
    avail_ring_p = avail_base + 2;

    /* QEMU transitional places the used ring 4096-aligned:
     * page 1 = desc, page 2 = avail, page 3 = used, page 4 = data */
    u64 used_off = (avail_off + 6 + 2 * qsize + 0xFFF) & ~0xFFFULL;
    used_idx_p = (u16 *)((u8 *)desc + used_off + 2);

    hdr_off = 3 * UIX_PAGE_SIZE + 0x400;
    st_off = 3 * UIX_PAGE_SIZE + 0x4FF;

    vio_out(VIO_QSEL, 0, 2);
    vio_out(VIO_QADDR, (u32)(ring_pa >> 12), 4);
    set_status(VIRTIO_ACK | VIRTIO_DRIVER | VIRTIO_DRIVER_OK);

    capacity_sectors = inl(io_base + 0x14) |
                       ((u64)inl(io_base + 0x18) << 32);

    kprintf(KLOG_INFO "virtio-blk: io %x qsz %u, %u sectors (%u MiB)\n",
            io_base, qsize, (u32)capacity_sectors,
            (u32)(capacity_sectors / 2048));
    return 0;
}

static int do_request(u32 type, u64 sector, void *data)
{
    struct virtio_blk_req *hdr =
        (struct virtio_blk_req *)((u8 *)desc + hdr_off);
    u8 *st = (u8 *)desc + st_off;

    hdr->type = type;
    hdr->sector = sector;
    *st = 0xFF;

    desc[0].addr = ring_pa + hdr_off;
    desc[0].len = sizeof(*hdr);
    desc[0].flags = VRING_DESC_F_NEXT;
    desc[0].next = 1;

    desc[1].addr = buf_pa;
    desc[1].len = 512;
    desc[1].flags = VRING_DESC_F_NEXT | (type == 0 ? VRING_DESC_F_WRITE : 0);
    desc[1].next = 2;

    desc[2].addr = ring_pa + st_off;
    desc[2].len = 1;
    desc[2].flags = VRING_DESC_F_WRITE;
    desc[2].next = 0;

    if (type == 1)
        memcpy(buf_va, data, 512);

    u16 idx = *avail_idx_p;
    avail_ring_p[idx % qsize] = 0;
    __asm__ volatile ("" : : : "memory");
    *avail_idx_p = idx + 1;
    __asm__ volatile ("" : : : "memory");

    vio_out(VIO_QNOTIFY, 0, 2);

    u16 last = *used_idx_p;
    u64 spins = 0;
    while (*used_idx_p == last) {
        if (++spins > 200000000ULL) {
            kprintf(KLOG_ERR "virtio-blk: request timeout "
                    "(status=%p avail=%u used=%u)\n",
                    (void *)(u64)vio_in(VIO_STATUS, 1),
                    *avail_idx_p, *used_idx_p);
            return -5;
        }
    }
    (void)inb(io_base + VIO_ISR);

    if (type == 0)
        memcpy(data, buf_va, 512);

    return *st == 0 ? 0 : -5;
}

int virtio_blk_read(u64 sector, void *buf)
{
    return do_request(0, sector, buf);
}

int virtio_blk_write(u64 sector, const void *buf)
{
    return do_request(1, sector, (void *)buf);
}

u64 virtio_blk_sectors(void)
{
    return capacity_sectors;
}

int virtio_blk_selftest(void)
{
    if (!io_base)
        return -1;

    u8 scratch[512];
    memcpy(scratch, "UIXROOTFS!", 10);
    if (virtio_blk_write(0, scratch) != 0) {
        kprintf(KLOG_ERR "virtio-blk: selftest write failed\n");
        return -1;
    }
    memset(scratch, 0, sizeof(scratch));
    if (virtio_blk_read(0, scratch) != 0) {
        kprintf(KLOG_ERR "virtio-blk: selftest read failed\n");
        return -1;
    }
    kprintf(KLOG_INFO "virtio-blk: selftest OK ('%s' roundtrip)\n", scratch);
    return 0;
}
