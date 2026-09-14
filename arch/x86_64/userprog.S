/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX embedded userspace program (flat binary, built from user/program.S). */

.section .rodata
.global user_program_start
user_program_start:
    .incbin "build/user/program.bin"
.global user_program_end
user_program_end:
