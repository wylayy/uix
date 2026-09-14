/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX syscall layer: int 0x80 from ring 3, dispatched here. */

#ifndef UIX_SYSCALL_H
#define UIX_SYSCALL_H

#include <uix/types.h>
#include <uix/idt.h>

#define SYS_exit   0
#define SYS_write  1
#define SYS_getpid 2
#define SYS_yield  3

void syscall_init(void);

/* called from isr_dispatch for vector 128; args in the iframe */
u64 syscall_dispatch(struct iframe *fr);

#endif /* UIX_SYSCALL_H */
