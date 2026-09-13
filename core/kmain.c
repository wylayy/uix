/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX kmain: kernel C entry. */

#include <uix/boot.h>
#include <uix/apic.h>
#include <uix/console.h>
#include <uix/gdt.h>
#include <uix/idt.h>
#include <uix/keyboard.h>
#include <uix/kprintf.h>
#include <uix/lib.h>

void kmain(void)
{
    console_init();
    boot_init();
    idt_init();
    apic_init();
    keyboard_init();

    __asm__ volatile ("sti");

    kprintf("\n");
    kprintf("uix v%d.%d: microkernel + POSIX personality\n", 0, 1);
    kprintf(KLOG_INFO "M1: GDT/TSS, IDT, exceptions, APIC timer, keyboard.\n");

    /* exception self-test: deliberate #GP to prove the dump path works.
     * commented out by default; uncomment to see the panic machinery. */
    /* __asm__ volatile ("ud2"); */

    u64 last = 0;
    for (;;) {
        __asm__ volatile ("hlt");
        if (jiffies - last >= 100) { /* once a second */
            last = jiffies;
            kprintf(KLOG_INFO "uptime: %u s\n", (u32)(jiffies / 100));
        }
    }
}
