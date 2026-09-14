/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX embedded second userspace program (flat binary). */

.section .rodata
.global user_hello2_start
user_hello2_start:
    .incbin "build/user/hello2.bin"
.global user_hello2_end
user_hello2_end:
