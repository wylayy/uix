/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX userspace task launch: build a ring-3 context and drop into it. */

#ifndef UIX_USER_H
#define UIX_USER_H

#include <uix/types.h>

/* spawn a ring-3 task running code (raw bytes) of length len at
 * USER_CODE_BASE with USER_STACK_TOP below it; returns the task */
struct task;
struct task *user_task_create(const char *name, const void *code, u64 len);

#define USER_CODE_BASE  0x400000ULL
#define USER_STACK_TOP  0x800000ULL
#define USER_STACK_SIZE 0x40000ULL /* 256 KiB */

#endif /* UIX_USER_H */
