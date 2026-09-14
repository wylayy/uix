/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIXFS: trivial read-only filesystem over virtio-blk.
 * LBA 0: "UIXFS1" + nfiles; then 32-byte entries
 * (name[24], size u32, start_lba u32); data follows. */

#ifndef UIX_UIXFS_H
#define UIX_UIXFS_H

#include <uix/types.h>

/* mount the disk; returns 0 on success */
int uixfs_mount(void);

/* read a file by name into a kmalloc'd buffer; returns size or
 * negative errno (-2 = not found). Caller frees. */
long uixfs_read(const char *name, void **out);

#endif /* UIX_UIXFS_H */
