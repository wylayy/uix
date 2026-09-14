# UIX

UIX is an experimental operating system: a **Mach-like microkernel core**
with a **POSIX/BSD personality**, in the spirit of XNU — one kernel, two
worlds: the core owns tasks, memory, and scheduling; the personality owns
file descriptors, processes, and UNIX semantics.

```
uix v0.1: microkernel + POSIX personality
[info] usable RAM: 2046 MiB
uix> run elfhello
[ELF] hello from a static ELF binary!
```

- **Arch:** x86_64 (higher-half kernel at `0xffffffff80100000`)
- **Boot:** [Limine](https://github.com/limine-bootloader/limine) protocol, BIOS + UEFI
- **Tested on:** QEMU (BIOS/UEFI), VirtualBox
- **License:** GPL-3.0-or-later — see [LICENSE](LICENSE)

## Status

| Subsystem | State |
|---|---|
| Boot | Limine protocol, memmap/HHDM/framebuffer parsing |
| Console | 16550 serial + framebuffer (8x8 font), `kprintf` subset, `panic` with register dump |
| Interrupts | Own GDT/TSS (IST for #DF/NMI), 256-gate IDT, exception decoding |
| Timers | LAPIC timer @ 100 Hz: TSC-deadline where supported, periodic LAPIC otherwise (CPUID-detected; VirtualBox lacks TSC-deadline) |
| Input | PS/2 keyboard -> tty line discipline (echo, backspace, CR-LF) |
| Memory | PMM bitmap + contiguous runs + page refcounts; VMM with cloned page tables, 4KiB/2MiB leaves, copy-on-write; kernel heap (first-fit, coalescing) |
| Scheduler | Preemptive round-robin, blocked state, zombie reaping, task registry |
| Userspace | Ring 3, per-task address spaces, `int 0x80` syscalls, ELF64 static loader + flat binaries, SysV initial stack (argc/argv/auxv) |
| POSIX personality | Per-proc fd table, tty, `open`/`close`, `fork` (COW), `execve`, `wait4` (blocking), exit statuses |
| Drivers | PCI enumeration (port I/O), virtio-blk (legacy virtio-pci, DMA queues, polling) |
| Filesystem | UIXFS: trivial read-only rootfs on the boot disk |
| Shell | Interactive: prompt, line editing, `hello2`, `run <file>` (fork + exec + wait) |

## System calls

`int 0x80` (rax = number, args in rdi/rsi/rdx, return in rax, errors as
`-errno`):

`exit` · `write` · `getpid` · `yield` · `read` · `close` · `open` ·
`fork` · `execve` · `wait4`

## Roadmap

| M | Scope | Status |
|---|-------|--------|
| M0 | boot, console, kprintf | done |
| M1 | GDT/IDT, exceptions, timer, keyboard | done |
| M2 | PMM, VMM, kernel heap | done |
| M3 | scheduler, context switch, kernel tasks | done |
| M4 | ring 3, syscalls, first user program | done |
| M5 | POSIX personality: tty, fds, fork/exec/wait | done |
| M6 | PCI, virtio-blk, rootfs, page refcounts | done |
| M7 | ELF64 loader, auxv (language runtime path) | done |
| M8 | `syscall` trap + Linux-x86_64 numbers, brk/mmap, musl | next |
| M9 | QuickJS (JavaScript), pipes, signals, mmap file-backed | planned |
| M10 | writeable rootfs, virtio-net, SMP | planned |

## Build & run

Requirements: `gcc`, `binutils`, `xorriso`, `qemu-system-x86_64`,
`make`, `python3`.

```
make          # build kernel + user programs + rootfs -> build/uix.iso
make run      # BIOS boot in QEMU with the rootfs disk, serial on stdio
make run-uefi # UEFI boot (needs OVMF)
make debug    # QEMU -s -S + gdb hint
make clean
```

`make run` attaches `build/rootfs.img` as a virtio disk containing the
UIXFS rootfs (`sh`, `hello2`, `elfhello`). A fresh clone builds as-is:
`limine-install` is compiled from the vendored source automatically.

To run elsewhere (VirtualBox, real hardware): attach `build/uix.iso` as
a CD-ROM; add `build/rootfs.img` as a raw disk for the root filesystem.

## Source layout

```
boot/          Limine protocol parsing + vendored bootloader binaries
core/
  cpu/         core layer: scheduler, tasks, syscalls, fork/exec/elf,
               user launch, interrupts (idt/apic)
  memory/      core layer: PMM, VMM (COW), kernel heap
  console.c    serial + framebuffer console
  kprintf.c    kernel printf + panic
  kmain.c      boot orchestration
personality/
  tty.c        BSD layer: line discipline
  fs/uixfs.c   BSD layer: read-only root filesystem
drivers/       serial, PS/2 keyboard, PCI, virtio-blk
arch/x86_64/   entry, interrupt stubs, context switch, embedded programs
user/          user programs (flat asm shell + host-built static ELF)
include/uix/   kernel headers
tools/         linker script, limine.conf, mkuixfs.py
```

## Design notes

- **Two ABI doors, XNU-style:** the UIX-native `int 0x80` dialect exists
  today; the standard `syscall` instruction with Linux-x86_64 numbering
  arrives in M8 so static musl binaries (and languages on top: C, C++,
  QuickJS, Lua) run unmodified.
- **Address spaces** are cloned from Limine's tables at boot (a clone
  cannot fault), then extended per-task; user halves are forked
  deep-copy with COW marking and refcounted pages.
- **DMA buffers** live in PMM pages mapped at fixed kernel scratch
  ranges, not the kernel heap.

## Third-party

Kept under their own licenses, not relicensed:

- `boot/limine/*` binaries and `limine-install.c` (BSD-2-Clause, Limine project)
- `include/limine.h` (0BSD, Limine project)
- `include/std*.h` (0BSD, osdev0 freestanding-c-hdrs)
- font8x8 (Public Domain, Daniel Hepper / Marcel Sondaar)

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
