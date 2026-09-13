/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * UIX: microkernel + personalitas POSIX, x86_64, protokol boot Limine.
 * Lapisan 1: core/ (cpu, memori, ipc).  Lapisan 2: personality/ (POSIX/BSD).
 * Lapisan 3: drivers/.  Lapisan 4: ruang pengguna (M4+).
 */

#ifndef UIX_TYPES_H
#define UIX_TYPES_H

#include <stdint.h>
#include <stddef.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

typedef unsigned long uint;      /* word mesin */
typedef unsigned long paddr_t;   /* alamat fisik */
typedef unsigned long vaddr_t;   /* alamat virtual */

#define NULL ((void *)0)

#define UIX_PAGE_SIZE   4096UL
#define UIX_PAGE_SHIFT  12UL

/* konvensi errno sejak hari pertama (personalitas POSIX) */
#define EPERM  1
#define ENOENT 2
#define EIO    5
#define ENOMEM 12
#define EFAULT 14
#define EEXIST 17
#define EINVAL 22
#define ENOSPC 28
#define ENOSYS 38

/* pembantu pembulatan */
#define UIX_ROUND_UP(x, a)   (((x) + (a) - 1) & ~((a) - 1))
#define UIX_ROUND_DOWN(x, a) ((x) & ~((a) - 1))

#endif /* UIX_TYPES_H */
