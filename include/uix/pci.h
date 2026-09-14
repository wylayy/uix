/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX PCI bus enumeration: config space via port I/O (CF8/CFC). */

#ifndef UIX_PCI_H
#define UIX_PCI_H

#include <uix/types.h>

#define PCI_ADDR 0xCF8
#define PCI_DATA 0xCFC

struct pci_dev {
    u16 vendor, device;
    u8 bus, dev, fn;
    u8 class_, subclass, prog_if;
    u8 irq;
    u64 bars[6];
    u8 bar_types[6]; /* 0 = mmio, 1 = io */
};

/* scan the bus and print what is found; returns the number of devices */
int pci_scan(void);

/* find a device by class code (e.g. 2/0 = network, 1/0 = storage other) */
int pci_find_by_class(u8 class_, u8 subclass, struct pci_dev *out);

/* find a device by vendor:device ids */
int pci_find_by_id(u16 vendor, u16 device, struct pci_dev *out);

/* enable I/O + memory + bus mastering (needed before DMA) */
void pci_enable_bus_master(struct pci_dev *d);

#endif /* UIX_PCI_H */
