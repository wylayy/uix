/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX kernel: x86_64 higher half, Limine protocol. */

OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

PHDRS
{
    limine_requests PT_LOAD FLAGS(0); /* marker+requests: R-- (Limine scans) */
    text            PT_LOAD FLAGS(5); /* R-X */
    rodata          PT_LOAD FLAGS(4); /* R-- */
    data            PT_LOAD FLAGS(6); /* RW- */
}

SECTIONS
{
    /* Topmost 2GiB (higher half), Limine-mandated region. */
    . = 0xffffffff80100000;

    .limine_requests : ALIGN(8)
    {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    } :limine_requests

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .text : ALIGN(4K)
    {
        KEEP(*(.text.boot))
        *(.text .text.*)
    } :text

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .rodata : ALIGN(4K)
    {
        *(.rodata .rodata.*)
    } :rodata

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .data : ALIGN(4K)
    {
        *(.data .data.*)
    } :data

    .bss : ALIGN(4K)
    {
        *(.bss .bss.*)
        *(COMMON)
    } :data

    /DISCARD/ :
    {
        *(.eh_frame*)
        *(.note .note.*)
        *(.comment)
    }
}
