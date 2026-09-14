#!/usr/bin/env python3
# UIXFS mkfs: pack flat binaries into build/rootfs.img (8 MiB).
# Layout: LBA0 superblock, then file entries, then data.

import struct, sys

SECT = 512
NFILES_MAX = 32

def main(out_path, files):
    img = bytearray(8 * 1024 * 1024)
    n = len(files)
    struct.pack_into("<4sI", img, 0, b"UIXF", n)

    entry_off = SECT
    data_off = SECT + NFILES_MAX * 32
    data_off = (data_off + SECT - 1) // SECT * SECT

    cur = data_off
    for i, (name, path) in enumerate(files):
        blob = open(path, "rb").read()
        name = name.encode()[:23]
        struct.pack_into("<24sII", img, entry_off + i * 32,
                         name, len(blob), cur // SECT)
        img[cur:cur + len(blob)] = blob
        cur += (len(blob) + SECT - 1) // SECT * SECT
        print(f"+ {name} ({len(blob)} bytes @ LBA {data_off//SECT if False else (cur - ((len(blob)+SECT-1)//SECT*SECT))//SECT})")

    open(out_path, "wb").write(img)
    print(f"UIXFS: {n} files -> {out_path}")

if __name__ == "__main__":
    out = sys.argv[1]
    files = [(a.split("=")[0], a.split("=")[1]) for a in sys.argv[2:]]
    main(out, files)
