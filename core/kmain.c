/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX kmain: kernel C entry. */

#include <uix/boot.h>
#include <uix/console.h>
#include <uix/kprintf.h>
#include <uix/lib.h>

void kmain(void)
{
    console_init();
    boot_init();

    kprintf("\n");
    kprintf("uix v%d.%d: microkernel + POSIX personality\n", 0, 1);
    kprintf("(x86_64, Limine boot, Mach-like core, BSD personality)\n");
    kprintf("\n");
    kprintf(KLOG_INFO "M0 complete: boot, serial, framebuffer console, kprintf.\n");
    kprintf(KLOG_INFO "Next: M1 (GDT/IDT, exceptions, APIC timer, keyboard).\n");

    /* idle loop until M1 gives us a scheduler */
    for (;;)
        __asm__ volatile ("hlt");
}
