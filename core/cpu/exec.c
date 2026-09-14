/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX execve: replace the calling task's user image with a program
 * from the embedded table or the UIXFS rootfs. Supports flat binaries
 * (legacy) and static ELF64 images. */

#include <uix/heap.h>
#include <uix/elf.h>
#include <uix/kprintf.h>
#include <uix/lib.h>
#include <uix/pmm.h>
#include <uix/task.h>
#include <uix/uixfs.h>
#include <uix/user.h>
#include <uix/vmm.h>

extern const u8 user_program_start[], user_program_end[];
extern const u8 user_hello2_start[], user_hello2_end[];

struct uxn {
    const char *name;
    const u8 *start;
    const u8 *end;
};

static const struct uxn programs[] = {
    { "/sh",     user_program_start, user_program_end },
    { "/hello2", user_hello2_start,  user_hello2_end },
};

/* free the page tables of the user half and drop page refs */
static void free_user_tables(u64 *table, int level)
{
    for (int i = 0; i < 512; i++) {
        u64 e = table[i];
        if (!(e & VMM_PRESENT))
            continue;
        if (level > 0) {
            free_user_tables(pmm_phys_to_virt(e & 0x000FFFFFFFFFF000ULL),
                             level - 1);
            pmm_free(e & 0x000FFFFFFFFFF000ULL);
        } else {
            pmm_unref(e & 0x000FFFFFFFFFF000ULL); /* shared COW-aware */
        }
    }
}

/* build the SysV initial stack: argc, argv[], NULL, envp[], NULL,
 * auxv pairs, NULL, NULL. Returns the user rsp (16-aligned region
 * with room below per ABI). Strings live near the stack top. */
static u64 build_user_stack(const char *path)
{
    u64 top = USER_STACK_TOP - 64; /* leave a small pad */

    /* argv[0] = path */
    u64 pathlen = strlen(path) + 1;
    top -= pathlen;
    u64 argv0_va = top;
    memcpy((void *)top, path, pathlen);

    /* 16 random bytes for AT_RANDOM */
    top -= 16;
    top &= ~0xFULL;
    u64 atrandom_va = top;
    for (int i = 0; i < 16; i++)
        *(u8 *)(atrandom_va + i) = (u8)(i * 7 + 0x5A);

    /* auxv: AT_PHDR-less minimal set */
    enum { AT_NULL = 0, AT_PAGESZ = 6, AT_RANDOM = 25, AT_ENTRY = 9 };
    u64 auxv[] = {
        AT_PAGESZ, UIX_PAGE_SIZE,
        AT_RANDOM, atrandom_va,
        AT_NULL, 0,
    };
    u64 aux_words = sizeof(auxv) / sizeof(u64);

    /* frame contents bottom-up: argc, argv[0], NULL, envp NULL,
     * auxv..., then 8-byte alignment pad so rsp % 16 == 0 at entry */
    u64 frame_words = 1 /*argc*/ + 2 /*argv*/ + 1 /*envp*/ + aux_words + 1;
    u64 frame_bytes = frame_words * 8;
    u64 rsp = (top - frame_bytes) & ~0xFULL;

    u64 *p = (u64 *)rsp;
    *p++ = 1;              /* argc */
    *p++ = argv0_va;       /* argv[0] */
    *p++ = 0;              /* argv NULL */
    *p++ = 0;              /* envp NULL (empty) */
    for (u64 i = 0; i < aux_words; i++)
        *p++ = auxv[i];

    return rsp;
}

/* called from the syscall dispatcher; on success the task never
 * returns to the old image (fresh user_launch trampoline) */
int do_execve(struct task *t, const char *upath)
{
    /* copy the user path first: it dies with the old address space */
    char path[64];
    for (u32 i = 0; i < 63 && upath[i]; i++)
        path[i] = upath[i];
    path[63] = 0;

    const u8 *img = NULL;
    u64 len = 0;

    /* embedded table first (leading '/') */
    for (u32 i = 0; i < sizeof(programs) / sizeof(programs[0]); i++)
        if (strcmp(path, programs[i].name) == 0) {
            img = programs[i].start;
            len = (u64)(programs[i].end - programs[i].start);
            break;
        }

    /* then the disk (bare name, e.g. "hello2") */
    void *disk_img = NULL;
    if (!img) {
        long dsize = uixfs_read(path, &disk_img);
        if (dsize < 0) {
            kprintf(KLOG_ERR "execve: no such program '%s'\n", path);
            return -2; /* -ENOENT */
        }
        img = disk_img;
        len = (u64)dsize;
    }

    /* free the old user tables (leaf pages unref'd COW-aware) */
    if (t->cr3) {
        vmm_switch(t->cr3);
        u64 *old = pmm_phys_to_virt(t->cr3);
        for (int i = 0; i < 256; i++)
            if (old[i] & VMM_PRESENT) {
                free_user_tables(
                    pmm_phys_to_virt(old[i] & 0x000FFFFFFFFFF000ULL), 2);
                pmm_free(old[i] & 0x000FFFFFFFFFF000ULL);
            }
        pmm_free(t->cr3);
    }

    /* fresh space */
    t->cr3 = vmm_create_space();
    vmm_switch(t->cr3);

    /* load the image: ELF or flat */
    u64 entry;
    int is_elf = len >= 4 && img[0] == 0x7F && img[1] == 'E';
    if (is_elf) {
        entry = elf_load(img, len);
        if (!entry) {
            kprintf(KLOG_ERR "execve: bad ELF '%s'\n", path);
            return -8; /* -ENOEXEC */
        }
    } else {
        entry = USER_CODE_BASE;
        u64 off = 0;
        while (off < len) {
            paddr_t pg = pmm_alloc();
            vmm_map(USER_CODE_BASE + off, pg,
                    VMM_PRESENT | VMM_WRITE | VMM_USER);
            u64 chunk = len - off;
            if (chunk > UIX_PAGE_SIZE)
                chunk = UIX_PAGE_SIZE;
            memcpy((void *)(USER_CODE_BASE + off), img + off, chunk);
            off += UIX_PAGE_SIZE;
        }
    }

    /* user stack */
    for (u64 va = USER_STACK_TOP - UIX_PAGE_SIZE;
         va >= USER_STACK_TOP - USER_STACK_SIZE; va -= UIX_PAGE_SIZE) {
        paddr_t pg = pmm_alloc();
        vmm_map(va, pg, VMM_PRESENT | VMM_WRITE | VMM_USER);
    }

    u64 user_rsp;
    if (is_elf) {
        user_rsp = build_user_stack(path);
    } else {
        user_rsp = USER_STACK_TOP - 16; /* flat binaries: simple sp */
    }

    if (disk_img) {
        extern void uixfs_release_buf(void);
        uixfs_release_buf(); /* PMM-backed, freed after the copy */
    }

    /* reset the kernel stack to the user_launch trampoline frame */
    extern void user_launch_trampoline(void);
    u64 sp = t->kstack + t->kstack_size;
    sp &= ~0xFULL;
    sp -= 8;  *(u64 *)sp = (u64)user_launch_trampoline;
    sp -= 8;  *(u64 *)sp = 0;
    sp -= 8;  *(u64 *)sp = 0;
    sp -= 8;  *(u64 *)sp = entry;                /* r12: rip */
    sp -= 8;  *(u64 *)sp = user_rsp;             /* r13: rsp */
    sp -= 8;  *(u64 *)sp = 0x202;                /* r14: rflags */
    sp -= 8;  *(u64 *)sp = 0;                    /* r15 */
    t->rsp = sp;

    /* hijack the syscall return: switch straight to the trampoline.
     * The old rsp value is dropped (never resumed). */
    {
        u64 dummy_rsp;
        extern void switch_to(u64 *prev_rsp, u64 next_rsp);
        switch_to(&dummy_rsp, t->rsp);
    }
    __builtin_unreachable();
}
