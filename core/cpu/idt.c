/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX GDT/TSS/IDT setup (x86_64). */

#include <uix/gdt.h>
#include <uix/idt.h>
#include <uix/io.h>
#include <uix/kprintf.h>
#include <uix/lib.h>

/* from cpu.S */
extern void (*const isr_stub_table[256])(void);

/* 6 descriptors: null, K_CS, K_DS, U_CS, U_DS, TSS (16 bytes) */
static u64 gdt[8] __attribute__((aligned(16)));
static struct {
    u16 limit;
    u64 base;
} __attribute__((packed)) gdtr;

/* TSS layout (x86_64) */
struct tss {
    u32 rsvd0;
    u64 rsp0, rsp1, rsp2;
    u32 rsvd1;
    u64 ist[7];
    u32 rsvd2;
    u16 iopb;
    u16 rsvd3;
} __attribute__((packed, aligned(16)));

static struct tss tss;

/* IST stacks from cpu.S */
extern u8 ist_df_stack[], ist_nmi_stack[], ist_pf_stack[];
#define IST_STACK_SIZE 16384

void tss_set_rsp0(u64 rsp)
{
    tss.rsp0 = rsp;
}

void gdt_init(void)
{
    /* code: present, DPL0, executable, readable, L=1 */
    gdt[1] = (1ULL << 47) | (1ULL << 44) | (1ULL << 43) | (1ULL << 41) | (1ULL << 53);
    /* data: present, DPL0, writable */
    gdt[2] = (1ULL << 47) | (1ULL << 44) | (1ULL << 41);
    /* user code: present, DPL3, executable, readable, L=1 */
    gdt[3] = (1ULL << 47) | (1ULL << 46) | (1ULL << 45) | (1ULL << 44) |
             (1ULL << 43) | (1ULL << 41) | (1ULL << 53);
    /* user data: present, DPL3, writable */
    gdt[4] = (1ULL << 47) | (1ULL << 46) | (1ULL << 45) | (1ULL << 44) | (1ULL << 41);

    /* TSS descriptor (2 quads) */
    u64 base = (u64)&tss;
    u32 limit = sizeof(tss) - 1;
    gdt[5] = (limit & 0xFFFF)
           | ((base & 0xFF) << 16)
           | (((base >> 8) & 0xFF) << 24)
           | (((base >> 16) & 0xFF) << 32)
           | ((u64)0x89 << 40)                 /* present, TSS64 */
           | (((u64)(base >> 24) & 0xFF) << 56);
    gdt[6] = base >> 32;

    tss.ist[IST_DF] = (u64)ist_df_stack + IST_STACK_SIZE;
    tss.ist[IST_NMI] = (u64)ist_nmi_stack + IST_STACK_SIZE;
    tss.ist[IST_PF] = (u64)ist_pf_stack + IST_STACK_SIZE;

    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base = (u64)gdt;

    __asm__ volatile (
        "lgdt %0\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%ss\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "movabs $1f, %%rax\n"
        "pushq $0x08\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        "mov $0x28, %%ax\n"
        "ltr %%ax\n"
        : : "m"(gdtr) : "rax", "memory");
}

/* ---------------- IDT ---------------- */

static struct {
    u16 off_lo;
    u16 sel;
    u8 ist;
    u8 flags;
    u16 off_mid;
    u32 off_hi;
    u32 rsvd;
} __attribute__((packed)) idt[256];

static struct {
    u16 limit;
    u64 base;
} __attribute__((packed)) idtr;

static const char *exc_name[32] = {
    "division by zero", "debug", "NMI", "breakpoint",
    "overflow", "bound range", "invalid opcode", "device not available",
    "double fault", "coproc seg overrun", "invalid TSS", "segment not present",
    "stack fault", "general protection", "page fault", "reserved",
    "x87 FP exception", "alignment check", "machine check", "SIMD FP",
    "virtualization", "control protection", "reserved", "reserved",
    "reserved", "reserved", "reserved", "reserved",
    "HV injection", "VMM communication", "security", "reserved"
};

void idt_init(void)
{
    memset(idt, 0, sizeof(idt));

    for (int i = 0; i < 256; i++) {
        u64 addr = (u64)isr_stub_table[i];
        idt[i].off_lo = addr & 0xFFFF;
        idt[i].off_mid = (addr >> 16) & 0xFFFF;
        idt[i].off_hi = (u32)(addr >> 32);
        idt[i].sel = 0x08;
        idt[i].ist = 0;
        idt[i].flags = 0x8E; /* present, ring0, interrupt gate */
    }
    /* double fault on IST1 so a blown kernel stack still prints */
    idt[8].ist = IST_DF + 1;

    idtr.limit = sizeof(idt) - 1;
    idtr.base = (u64)idt;
    __asm__ volatile ("lidt %0" : : "m"(idtr));
}

/* make a gate callable from ring 3 (interrupt/trap gate, DPL 3) */
void idt_gate_set_dpl3(u8 vec)
{
    idt[vec].flags = 0xEE; /* present, DPL 3, interrupt gate */
}

/* ---------------- dispatch ---------------- */

static void dump_frame(struct iframe *fr)
{
    kprintf("RIP=%p CS=%p RFLAGS=%p RSP=%p\n",
            (void *)fr->rip, (void *)fr->cs, (void *)fr->rflags, (void *)fr->rsp);
    kprintf("RAX=%p RBX=%p RCX=%p RDX=%p\n",
            (void *)fr->rax, (void *)fr->rbx, (void *)fr->rcx, (void *)fr->rdx);
    kprintf("RSI=%p RDI=%p RBP=%p ERR=%p\n",
            (void *)fr->rsi, (void *)fr->rdi, (void *)fr->rbp, (void *)fr->err);
}

void isr_dispatch(struct iframe *fr)
{
    if (fr->int_no < 32) {
        if (fr->int_no == 14) {
            u64 cr2;
            __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
            extern int vmm_page_fault(u64, u64);
            if (vmm_page_fault(cr2, fr->err) == 0)
                return; /* handled (COW): retry */
        }
        kprintf(KLOG_ERR "exception %u (%s) in ring %u\n",
                (u32)fr->int_no,
                exc_name[fr->int_no] ? exc_name[fr->int_no] : "?",
                (u32)(fr->cs & 3));
        dump_frame(fr);
        panic("unhandled exception");
    }

    extern void apic_eoi(void);
    extern void keyboard_irq(void);

    switch (fr->int_no) {
    case 32: /* LAPIC timer: EOI first, because schedule() may switch
              * away from this interrupt frame and never return to it */
        apic_eoi();
        extern void apic_timer_tick(void);
        extern void sched_tick(void);
        apic_timer_tick(); /* jiffies++ and rearm (TSC-deadline) */
        sched_tick();
        return; /* do not EOI twice */
    case 33: /* PS/2 keyboard */
        keyboard_irq();
        break;
    case 128: { /* syscall (int 0x80) */
        extern u64 syscall_dispatch(struct iframe *);
        fr->rax = syscall_dispatch(fr);
        break;
    }
    default:
        break;
    }

    apic_eoi();
}
