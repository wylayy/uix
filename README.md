# UIX

Experimental OS: Mach-like microkernel core + POSIX/BSD personality,
x86_64, [Limine](https://github.com/limine-bootloader/limine) boot protocol.

Licensed under the GNU GPL v3 (or later): see [LICENSE](LICENSE).

## Status: M0 (hello world)

- Boots (BIOS + UEFI) via Limine, higher-half kernel at `0xffffffff80100000`
- Serial console (16550 UART) + framebuffer console (8x8 font, double-scanned)
- `kprintf`/`panic` (subset: `%c %s %d %u %x %X %p %%`, field width)
- Limine memmap + HHDM parsed, freestanding lib

```
uix v0.1: microkernel + POSIX personality
[info] usable RAM: 2046 MiB
```

## Roadmap

| M | Scope |
|---|-------|
| M1 | our own GDT/TSS, IDT, exceptions, APIC timer, PS/2 keyboard |
| M2 | PMM (bitmap from Limine memmap), VMM (4-level paging, COW-ready), heap |
| M3 | scheduler, context switch, kernel tasks |
| M4 | IPC ports, ring 3, ELF loader, syscalls |
| M5 | POSIX personality: VFS, ramfs/devfs, fd table, fork/execve (static ELF) |
| M6 | drivers: PCI enum, virtio-blk, NVMe/USB later |

## Build & run

Requirements: gcc, binutils, xorriso, qemu-system-x86_64, make.
A fresh clone builds as-is: `limine-install` is compiled from source automatically.

```
make        # build build/uix.iso
make run    # BIOS boot in QEMU, serial on stdio
make run-uefi  # UEFI boot (needs OVMF)
make debug  # QEMU -s -S + gdb hint
make clean
```

## Layout

```
boot/        Limine protocol parsing + vendored bootloader binaries
core/        layer 1: kernel core (lib, console, kprintf, kmain → cpu/memory/ipc)
personality/ layer 2: POSIX/BSD (M5+)
drivers/     layer 3: I/O (serial now; pci/storage later)
arch/x86_64/ entry.S, ISA-specific code
include/     uix/ headers, vendored limine.h + freestanding C headers
tools/       linker.ld, limine.conf
```

Third-party (kept under their own licenses, not relicensed):
`boot/limine/*` and `include/limine.h` (BSD-2-Clause, Limine project),
`include/std*.h` (freestanding-c-hdrs), font8x8 (public domain).

## License

Copyright (C) 2026 UIX contributors.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
[LICENSE](LICENSE) for more details.
