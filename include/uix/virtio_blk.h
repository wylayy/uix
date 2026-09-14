/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX virtio-blk driver (legacy interface, virtio-pci 1af4:1001).
 * Polling (no IRQ yet); single request at a time. */

#ifndef UIX_VIRTIO_BLK_H
#define UIX_VIRTIO_BLK_H

#include <uix/types.h>

/* returns 0 on success */
int virtio_blk_init(void);

/* read one 512-byte sector into buf (must be 512-aligned memory) */
int virtio_blk_read(u64 sector, void *buf);

/* write one 512-byte sector from buf */
int virtio_blk_write(u64 sector, const void *buf);

u64 virtio_blk_sectors(void);

#endif /* UIX_VIRTIO_BLK_H */
