# UIX

Experimental OS: Mach-like microkernel core + POSIX/BSD personality,
x86_64, [Limine](https://github.com/limine-bootloader/limine) boot protocol.

Licensed under the GNU GPL v3 (or later): see [LICENSE](LICENSE).

## Status: M3 (scheduler)

- Boots (BIOS + UEFI) via Limine, higher-half kernel at `0xffffffff80100000`
- Serial console (16550 UART) + framebuffer console (8x8 font, double-scanned)
- `kprintf`/`panic` (subset: `%c %s %d %u %x %X %p %%`, field width)
- Own GDT/TSS (IST for double fault/NMI), 256-entry IDT, exception dump
- Local APIC + IOAPIC (keyboard routed), legacy PIC disabled
- Timer at 100 Hz via TSC-deadline (calibrated against PIT channel 2)
- PS/2 keyboard (scancode set 1) echoing to console
- PMM: bitmap allocator over the Limine memmap
- VMM: page tables cloned from Limine then switched (own CR3),
  map/unmap/translate (4KiB and 2MiB leaves), page fault decode
- Kernel heap: kmalloc/kfree/kzalloc, first-fit with coalescing
- Preemptive round-robin scheduler, context switch, kernel tasks
  with per-task stacks, zombie reaping, boot self-test

```
uix v0.1: microkernel + POSIX personality
[info] usable RAM: 2046 MiB
```

## Roadmap

| M | Scope |
|---|-------|
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
`boot/limine/*` binaries and `limine-install.c` (BSD-2-Clause),
`include/limine.h` and `include/std*.h` (0BSD, Limine project / osdev0),
font8x8 (Public Domain, Daniel Hepper / Marcel Sondaar).

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
