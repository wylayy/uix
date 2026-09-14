/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX ELF64 static loader. */

#include <uix/elf.h>
#include <uix/pmm.h>
#include <uix/vmm.h>
#include <uix/kprintf.h>
#include <uix/lib.h>

u64 elf_load(const void *img, u64 len)
{
    const struct elf64_ehdr *eh = img;

    if (len < sizeof(*eh))
        return 0;
    if (eh->ident[0] != 0x7F || eh->ident[1] != 'E' ||
        eh->ident[2] != 'L' || eh->ident[3] != 'F')
        return 0;
    if (eh->ident[4] != 2) /* ELFCLASS64 */
        return 0;
    if (eh->machine != 0x3E) /* x86-64 */
        return 0;
    if (eh->phnum == 0 || eh->phentsize < sizeof(struct elf64_phdr))
        return 0;

    const struct elf64_phdr *ph =
        (const void *)((const u8 *)img + eh->phoff);

    for (u32 i = 0; i < eh->phnum; i++) {
        if (ph[i].type != PT_LOAD)
            continue;

        u64 start = ph[i].vaddr & ~(UIX_PAGE_SIZE - 1);
        u64 end = ph[i].vaddr + ph[i].memsz;
        end = (end + UIX_PAGE_SIZE - 1) & ~(UIX_PAGE_SIZE - 1);

        for (u64 va = start; va < end; va += UIX_PAGE_SIZE) {
            paddr_t pg = pmm_alloc();
            if (!pg)
                return 0;
            /* clear, then copy the file part covering this page */
            memset(pmm_phys_to_virt(pg), 0, UIX_PAGE_SIZE);
            vmm_map(va, pg, VMM_PRESENT | VMM_WRITE | VMM_USER);

            u64 file_off = ph[i].offset + (va - ph[i].vaddr);
            if (file_off >= ph[i].offset && va < ph[i].vaddr + ph[i].filesz) {
                u64 copy = ph[i].vaddr + ph[i].filesz - va;
                if (copy > UIX_PAGE_SIZE)
                    copy = UIX_PAGE_SIZE;
                if (file_off + copy > len)
                    copy = len - file_off;
                memcpy((void *)va, (const u8 *)img + file_off, copy);
            }
        }
    }

    return eh->entry;
}
