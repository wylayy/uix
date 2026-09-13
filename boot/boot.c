/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX boot: parse Limine protocol responses, hand them to core. */

#include <uix/boot.h>
#include <uix/kprintf.h>
#include <uix/lib.h>
#include <limine.h>

/* Limine requests, scanned between start/end markers */

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(2);
__attribute__((used, section(".limine_requests")))
volatile struct limine_framebuffer_request fb_req = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_req = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_req = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

/* boot info */

static struct uix_bootinfo bootinfo;

const struct uix_bootinfo *boot_info(void)
{
    return &bootinfo;
}

/* init */

void boot_init(void)
{
    if (memmap_req.response == NULL)
        panic("no memory map from Limine");
    if (hhdm_req.response == NULL)
        panic("no HHDM from Limine");

    bootinfo.memmap = memmap_req.response->entries;
    bootinfo.memmap_entries = memmap_req.response->entry_count;
    bootinfo.hhdm = hhdm_req.response->offset;

    u64 usable = 0;
    for (u64 i = 0; i < bootinfo.memmap_entries; i++) {
        struct limine_memmap_entry *e = bootinfo.memmap[i];
        kprintf(KLOG_INFO "memmap: base=%p len=%pK type=%s\n",
                e->base, (void *)(e->length / 1024),
                e->type == LIMINE_MEMMAP_USABLE ? "usable" :
                e->type == LIMINE_MEMMAP_RESERVED ? "reserved" :
                e->type == LIMINE_MEMMAP_ACPI_RECLAIMABLE ? "acpi" :
                e->type == LIMINE_MEMMAP_ACPI_NVS ? "acpi-nvs" :
                e->type == LIMINE_MEMMAP_BAD_MEMORY ? "bad" :
                e->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE ? "bootdata" :
                e->type == LIMINE_MEMMAP_EXECUTABLE_AND_MODULES ? "modules" :
                "kernel");
        if (e->type == LIMINE_MEMMAP_USABLE)
            usable += e->length;
    }
    kprintf(KLOG_INFO "HHDM offset: %p\n", (void *)bootinfo.hhdm);
    kprintf(KLOG_INFO "usable RAM: %u MiB\n", (u32)(usable >> 20));
}
