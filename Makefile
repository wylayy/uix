# UIX kernel: main build script.
# Targets: all (ISO), run, run-uefi, debug, clean
# Toolchain: host gcc (freestanding), GNU as, xorriso, QEMU.

NAME     := uix
ARCH     := x86_64
# SPDX-License-Identifier: GPL-3.0-or-later

CC       := gcc
HOSTCC   := gcc
AS       := as
LD       := ld
CFLAGS   := -std=gnu11 -Wall -Wextra -ffreestanding -mno-red-zone -mno-mmx \
            -mno-sse -mno-sse2 -mcmodel=kernel -O2 -g \
            -fno-stack-protector -fno-omit-frame-pointer \
            -fno-pie -fno-pic -nostdinc \
            -Iinclude -Iarch/$(ARCH)
ASFLAGS  := -c
LDFLAGS  := -nostdlib -static -z max-page-size=0x1000 \
            --no-dynamic-linker -no-pie

DIRS     := build build/iso build/iso/boot build/iso/uix

SRCS_C   := $(shell find boot core personality drivers -name '*.c' \
            -not -path 'boot/limine/*' 2>/dev/null)
SRCS_S   := $(shell find arch -name '*.S' 2>/dev/null)

OBJS     := $(patsubst %.c,build/%.o,$(SRCS_C)) \
            $(patsubst %.S,build/%.o,$(SRCS_S))

DEPS     := $(OBJS:.o=.d)

.PHONY: all run run-uefi debug clean iso
all: iso

# ---------------- compile rules ----------------

build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# userprog.o embeds program.bin via .incbin: force it to recompile
# whenever the binary changes (order-only prerequisite + explicit rule)
build/arch/x86_64/userprog.o: arch/x86_64/userprog.S build/user/program.bin
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

build/user/program.bin: user/program.S
	@mkdir -p $(dir $@)
	$(CC) -c user/program.S -o build/user/program.o
	$(LD) -Ttext=0 -o build/user/program.elf build/user/program.o
	objcopy -O binary build/user/program.elf $@

build/$(NAME).elf: $(OBJS) tools/linker.ld
	$(LD) $(LDFLAGS) -T tools/linker.ld $(OBJS) -o $@

# rebuild any other .S through the pattern rule below
build/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

# host tool, built with plain host gcc (not the freestanding kernel flags)
boot/limine/limine-install: boot/limine/limine-install.c boot/limine/limine-bios-hdd.h
	$(HOSTCC) -O2 -Wall $< -o $@

# ---------------- ISO ----------------

iso: build/$(NAME).elf boot/limine/limine-install
	@mkdir -p build/iso/boot build/iso/uix build/iso/EFI/BOOT
	cp build/$(NAME).elf build/iso/boot/$(NAME).elf
	cp boot/limine/limine-bios-cd.bin build/iso/boot/limine-bios-cd.bin
	cp boot/limine/limine-bios.sys build/iso/boot/limine-bios.sys
	cp boot/limine/limine-uefi-cd.bin build/iso/boot/limine-uefi-cd.bin
	cp boot/limine/BOOTX64.EFI build/iso/EFI/BOOT/BOOTX64.EFI
	cp tools/limine.conf build/iso/limine.conf
	xorriso -as mkisofs -b boot/limine-bios-cd.bin -no-emul-boot \
	    -boot-load-size 4 -boot-info-table --efi-boot boot/limine-uefi-cd.bin \
	    -efi-boot-part --efi-boot-image --protective-msdos-label \
	    build/iso -o build/$(NAME).iso
	./boot/limine/limine-install bios-install build/$(NAME).iso

# ---------------- run ----------------

QEMU      := qemu-system-x86_64
QEMUFLAGS := -m 2G -serial stdio -no-reboot -no-shutdown

run: iso
	$(QEMU) $(QEMUFLAGS) -cdrom build/$(NAME).iso

run-uefi: iso
	$(QEMU) $(QEMUFLAGS) -bios /usr/share/OVMF/OVMF_CODE.fd \
	    -cdrom build/$(NAME).iso

debug: iso
	$(QEMU) $(QEMUFLAGS) -cdrom build/$(NAME).iso -s -S &
	@echo "gdb build/uix.elf  ->  target remote localhost:1234"

# ---------------- housekeeping ----------------

clean:
	rm -rf build boot/limine/limine-install

-include $(DEPS)
