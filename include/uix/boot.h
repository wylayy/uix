/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX boot protocol info (Limine). */

#ifndef UIX_BOOT_H
#define UIX_BOOT_H

#include <uix/types.h>
#include <limine.h>

struct uix_bootinfo {
    struct limine_memmap_entry **memmap;
    u64 memmap_entries;
    u64 hhdm; /* higher half direct map offset */
};

/* Parse Limine responses. Call once, first, in kmain. */
void boot_init(void);
const struct uix_bootinfo *boot_info(void);

/* framebuffer request lives here; console.c consumes it */
extern volatile struct limine_framebuffer_request fb_req;

#endif /* UIX_BOOT_H */
