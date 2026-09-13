/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX interrupt controller: legacy PIC disable, Local APIC + IOAPIC,
 * periodic timer calibrated against the PIT. */

#ifndef UIX_APIC_H
#define UIX_APIC_H

#include <uix/types.h>

void apic_init(void);

/* calibrated LAPIC timer ticks per second (set during init) */
extern u64 apic_timer_hz;
extern volatile u64 jiffies; /* incremented every timer tick */

#endif /* UIX_APIC_H */
