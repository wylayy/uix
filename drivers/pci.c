/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX PCI enumeration: port-I/O config space, single bus group scan. */

#include <uix/pci.h>
#include <uix/io.h>
#include <uix/kprintf.h>

#define MAX_DEVS 32
static struct pci_dev found[MAX_DEVS];
static int found_count;

static u32 pci_read32(u8 bus, u8 dev, u8 fn, u8 off)
{
    u32 addr = (1u << 31) | ((u32)bus << 16) | ((u32)dev << 11)
             | ((u32)fn << 8) | (off & 0xFC);
    outl(PCI_ADDR, addr);
    return inl(PCI_DATA);
}

static void pci_write32(u8 bus, u8 dev, u8 fn, u8 off, u32 val)
{
    u32 addr = (1u << 31) | ((u32)bus << 16) | ((u32)dev << 11)
             | ((u32)fn << 8) | (off & 0xFC);
    outl(PCI_ADDR, addr);
    outl(PCI_DATA, val);
}

static u16 pci_read16(u8 bus, u8 dev, u8 fn, u8 off)
{
    u32 v = pci_read32(bus, dev, fn, off & ~3);
    return (u16)(v >> ((off & 2) * 8));
}

static u64 bar_size(u8 bus, u8 dev, u8 fn, int bar)
{
    u8 off = 0x10 + bar * 4;
    u32 orig = pci_read32(bus, dev, fn, off);
    pci_write32(bus, dev, fn, off, 0xFFFFFFFF);
    u32 sz = pci_read32(bus, dev, fn, off);
    pci_write32(bus, dev, fn, off, orig);
    if (orig & 1) /* I/O space BAR */
        return (u64)(~(sz & ~0x3) + 1) & 0xFFFF;
    return (u64)(~(sz & ~0xF) + 1);
}

static void probe(u8 bus, u8 dev, u8 fn)
{
    u16 vendor = pci_read16(bus, dev, fn, 0);
    if (vendor == 0xFFFF)
        return;
    if (found_count >= MAX_DEVS)
        return;

    struct pci_dev *d = &found[found_count++];
    d->vendor = vendor;
    d->device = pci_read16(bus, dev, fn, 2);
    d->bus = bus;
    d->dev = dev;
    d->fn = fn;

    u32 cl = pci_read32(bus, dev, fn, 8);
    d->prog_if = (cl >> 16) & 0xFF;
    d->subclass = (cl >> 24) & 0xFF;
    d->class_ = (cl >> 16) >> 8; /* same 32-bit read, bits 31:24 */
    d->irq = pci_read16(bus, dev, fn, 0x3C) & 0xFF;

    for (int b = 0; b < 6; b++) {
        u32 bar = pci_read32(bus, dev, fn, 0x10 + b * 4);
        d->bar_types[b] = bar & 1;
        if (bar & 1)
            d->bars[b] = bar & ~0x3;
        else if ((bar & 6) == 4 && b < 5) { /* 64-bit MMIO */
            u32 hi = pci_read32(bus, dev, fn, 0x10 + (b + 1) * 4);
            d->bars[b] = ((u64)hi << 32) | (bar & ~0xF);
            b++; /* skip the high word */
        } else {
            d->bars[b] = bar & ~0xF;
        }
    }

    kprintf(KLOG_INFO "pci %02x:%02x.%u %04x:%04x class %02x%02x%02x"
            " irq %u bar0 %p (size %pK)\n",
            bus, dev, fn, d->vendor, d->device,
            d->class_, d->subclass, d->prog_if, d->irq,
            (void *)d->bars[0], (void *)bar_size(bus, dev, fn, 0));
}

int pci_scan(void)
{
    found_count = 0;
    /* single PCI bus group (bus 0 only; QEMU exposes everything there) */
    for (u8 dev = 0; dev < 32; dev++) {
        u16 vendor = pci_read16(0, dev, 0, 0);
        if (vendor == 0xFFFF)
            continue;
        probe(0, dev, 0);
        /* multifunction? */
        u8 hdr = pci_read16(0, dev, 0, 14) & 0xFF;
        if (hdr & 0x80)
            for (u8 fn = 1; fn < 8; fn++)
                probe(0, dev, fn);
    }
    return found_count;
}

int pci_find_by_class(u8 class_, u8 subclass, struct pci_dev *out)
{
    for (int i = 0; i < found_count; i++)
        if (found[i].class_ == class_ && found[i].subclass == subclass) {
            *out = found[i];
            return 0;
        }
    return -1;
}

int pci_find_by_id(u16 vendor, u16 device, struct pci_dev *out)
{
    for (int i = 0; i < found_count; i++)
        if (found[i].vendor == vendor && found[i].device == device) {
            *out = found[i];
            return 0;
        }
    return -1;
}

void pci_enable_bus_master(struct pci_dev *d)
{
    u32 cmd = pci_read32(d->bus, d->dev, d->fn, 0x04);
    cmd |= 0x7; /* I/O + memory + bus master */
    pci_write32(d->bus, d->dev, d->fn, 0x04, cmd);
}
