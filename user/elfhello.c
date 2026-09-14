/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UIX ELF test: static ELF64 built on the host, no libc, int 0x80. */

typedef unsigned long u64;

static inline long sys(long n, long a, long b, long c)
{
    long ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "a"(n), "D"(a), "S"(b), "d"(c) : "memory");
    return ret;
}

static void puts_(const char *s)
{
    long n = 0;
    while (s[n])
        n++;
    sys(1, 1, (long)s, n);
}

void _start(void)
{
    /* read argc+argv[0] from the initial stack (rsp -> argc, argv) */
    u64 argc = 0;
    const char *arg0 = 0;
    __asm__ volatile ("mov (%%rsp), %0" : "=r"(argc));
    __asm__ volatile ("mov 8(%%rsp), %0" : "=r"(arg0));

    puts_("[ELF] hello from a static ELF binary!\n");
    if (argc >= 1 && arg0) {
        puts_("[ELF] argv0: ");
        puts_(arg0);
        puts_("\n");
    }

    /* exit(9) */
    sys(0, 9, 0, 0);
    for (;;)
        ;
}
