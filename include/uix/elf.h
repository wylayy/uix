/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX ELF64 static loader: map PT_LOAD segments, return the entry. */

#ifndef UIX_ELF_H
#define UIX_ELF_H

#include <uix/types.h>

#define EI_NIDENT 16
#define PT_LOAD 1

struct elf64_ehdr {
    u8 ident[EI_NIDENT];
    u16 type, machine;
    u32 version;
    u64 entry, phoff, shoff;
    u32 flags;
    u16 ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
} __attribute__((packed));

struct elf64_phdr {
    u32 type, flags;
    u64 offset, vaddr, paddr, filesz, memsz, align;
} __attribute__((packed));

/* load a static ELF image (kernel virtual buffer) into the current
 * user address space; returns the entry point or 0 on failure */
u64 elf_load(const void *img, u64 len);

#endif /* UIX_ELF_H */
