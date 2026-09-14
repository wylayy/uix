/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX interrupt controllers: disable legacy PIC, Local APIC + IOAPIC,
 * periodic LAPIC timer calibrated against the PIT. */

#include <uix/apic.h>
#include <uix/idt.h>
#include <uix/io.h>
#include <uix/kprintf.h>
#include <uix/lib.h>
#include <uix/boot.h>
#include <limine.h>

#define IA32_APIC_BASE_MSR 0x1B

/* LAPIC register offsets (bytes) */
#define LAPIC_ID        0x020
#define LAPIC_EOI       0x0B0
#define LAPIC_SVR       0x0F0
#define LAPIC_TIMER_LVT 0x320
#define LAPIC_TIMER_ICR 0x380
#define LAPIC_TIMER_CCR 0x390
#define LAPIC_TIMER_DCR 0x3E0

#define MSR_IA32_TSC_DEADLINE 0x660

/* TSC-deadline mode: LVT bit 18, one-shot armed via MSR */
#define LVT_TSC_DEADLINE (1u << 18)

/* IOAPIC (default address; MADT parsing replaces this in M6) */
#define IOAPIC_BASE 0xFEC00000
#define IOAPIC_REG_SELECT 0x00
#define IOAPIC_REG_WINDOW 0x10

/* PIC */
#define PIC1_CMD 0x20
#define PIC1_DAT 0x21
#define PIC2_CMD 0xA0
#define PIC2_DAT 0xA1

/* PIT channel 0 */
#define PIT_CH0 0x40
#define PIT_CMD 0x43

#define IRQ_BASE 32
#define IRQ_TIMER 0  /* LAPIC timer, not routed via IOAPIC */
#define IRQ_KEYBOARD 1

u64 apic_timer_hz;
volatile u64 jiffies;

static u64 rdtsc(void)
{
    u32 lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((u64)hi << 32) | lo;
}

static volatile u32 *lapic;

static u32 lapic_read(u32 reg)
{
    return lapic[reg / 4];
}

static void lapic_write(u32 reg, u32 val)
{
    lapic[reg / 4] = val;
}

void apic_eoi(void)
{
    lapic_write(LAPIC_EOI, 0);
}

/* route ISA IRQ n to vector IRQ_BASE + n (physical dest 0: the BSP) */
static void ioapic_set_isa(u8 isa_irq, u8 vec)
{
    volatile u32 *ioapic =
        (volatile u32 *)(boot_info()->hhdm + IOAPIC_BASE);
    u32 entry = ((u32)vec) | (0u << 11); /* edge, active-high, physical dest */
    ioapic[IOAPIC_REG_SELECT / 4] = 0x10 + isa_irq * 2;
    ioapic[IOAPIC_REG_WINDOW / 4] = entry;
    ioapic[IOAPIC_REG_SELECT / 4] = 0x11 + isa_irq * 2;
    ioapic[IOAPIC_REG_WINDOW / 4] = 0;
}

/* calibrate the TSC against PIT channel 2 (one-shot 50 ms gate) */
static u64 calibrate_tsc(void)
{
    outb(0x61, (inb(0x61) & ~0x02) | 0x01);
    outb(PIT_CMD, 0b10110000); /* ch2, lo/hi, mode 0 */
    u32 reload = 59659;        /* 50 ms */
    outb(0x42, reload & 0xFF);
    outb(0x42, (reload >> 8) & 0xFF);

    u64 t0 = rdtsc();
    while (!(inb(0x61) & 0x20))
        ;
    u64 t1 = rdtsc();

    return (t1 - t0) * 20; /* TSC ticks per second */
}

static u64 tsc_hz;
static int use_tsc_deadline;

static void arm_timer(void)
{
    if (use_tsc_deadline) {
        u64 deadline = rdtsc() + tsc_hz / 100; /* 100 Hz */
        __asm__ volatile ("wrmsr" :
            : "c"((u64)MSR_IA32_TSC_DEADLINE),
              "a"((u32)deadline), "d"((u32)(deadline >> 32)));
    }
    /* periodic mode re-arms itself */
}

/* CPUID.01H:ECX bit 24 = TSC-Deadline LAPIC timer support */
static bool cpu_has_tsc_deadline(void)
{
    u32 eax, ebx, ecx, edx;
    __asm__ volatile ("cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1));
    return ecx & (1u << 24);
}

/* calibrate the LAPIC bus clock against PIT channel 2 (for the
 * periodic fallback mode) */
static u64 calibrate_lapic(void)
{
    outb(0x61, (inb(0x61) & ~0x02) | 0x01);
    outb(PIT_CMD, 0b10110000);
    u32 reload = 59659; /* 50 ms */
    outb(0x42, reload & 0xFF);
    outb(0x42, (reload >> 8) & 0xFF);

    lapic_write(LAPIC_TIMER_DCR, 0x1);                /* divide by 1 */
    lapic_write(LAPIC_TIMER_LVT, (1u << 16) | 0x20);  /* masked one-shot */
    lapic_write(LAPIC_TIMER_ICR, 0xFFFFFFFF);

    u64 start = 0xFFFFFFFF - lapic_read(LAPIC_TIMER_CCR);
    while (!(inb(0x61) & 0x20))
        ;
    u64 end = 0xFFFFFFFF - lapic_read(LAPIC_TIMER_CCR);
    return (end - start) * 20;
}

void apic_init(void)
{
    /* disable legacy PICs (we use APIC) */
    outb(PIC1_DAT, 0xFF);
    outb(PIC2_DAT, 0xFF);

    /* map LAPIC via HHDM */
    u64 base;
    __asm__ volatile ("rdmsr" : "=a"(base) : "c"(IA32_APIC_BASE_MSR));
    u64 phys = base & ~0xFFFULL;
    lapic = (volatile u32 *)(boot_info()->hhdm + phys);

    lapic_write(LAPIC_SVR, 0x100 | 0xFF); /* APIC software enable + spurious vec */

    /* route keyboard IRQ1 to vector 33 */
    ioapic_set_isa(IRQ_KEYBOARD, IRQ_BASE + IRQ_KEYBOARD);

    /* timer at 100 Hz: prefer TSC-deadline (stable on QEMU); fall back
     * to periodic LAPIC where TSC-deadline is unsupported (VirtualBox) */
    use_tsc_deadline = cpu_has_tsc_deadline();

    if (use_tsc_deadline) {
        tsc_hz = calibrate_tsc();
        if (tsc_hz < 1000000)
            panic("apic: TSC calibration failed (%u)", (u32)tsc_hz);
        apic_timer_hz = tsc_hz;
        lapic_write(LAPIC_TIMER_LVT, IRQ_BASE | LVT_TSC_DEADLINE);
        arm_timer();
        kprintf(KLOG_INFO "apic: TSC %u Hz, TSC-deadline timer at 100 Hz\n",
                (u32)tsc_hz);
    } else {
        u64 bus_hz = calibrate_lapic();
        if (bus_hz < 100000)
            panic("apic: LAPIC calibration failed (%u)", (u32)bus_hz);
        apic_timer_hz = bus_hz;
        lapic_write(LAPIC_TIMER_DCR, 0x1);                   /* divide 1 */
        lapic_write(LAPIC_TIMER_LVT,
                    IRQ_BASE | (1u << 17));                  /* periodic */
        lapic_write(LAPIC_TIMER_ICR, (u32)(bus_hz / 100));   /* 100 Hz */
        kprintf(KLOG_INFO "apic: LAPIC %u Hz, periodic timer at 100 Hz\n",
                (u32)bus_hz);
    }
}

/* called from isr_dispatch for vector 32 */
void apic_timer_tick(void)
{
    jiffies++;
    arm_timer(); /* TSC-deadline is one-shot: rearm every tick */
}
