#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H

#include <stdint.h>

// 系统调用号（需与内核保持一致）
#define SYS_EXIT    1
#define SYS_GETPID  2
#define SYS_FORK    3
#define SYS_WRITE   6

static inline long do_syscall(long n, long a0, long a1, long a2) {
    register long x10 asm("a0") = a0;
    register long x11 asm("a1") = a1;
    register long x12 asm("a2") = a2;
    register long x17 asm("a7") = n;
    asm volatile ("ecall" : "+r"(x10) : "r"(x11), "r"(x12), "r"(x17) : "memory");
    return x10;
}

static inline int sys_getpid(void) {
    return (int)do_syscall(SYS_GETPID, 0, 0, 0);
}

static inline void sys_exit(int status) {
    (void)do_syscall(SYS_EXIT, status, 0, 0);
    while (1) {}
}

static inline int sys_write(int fd, const void *buf, int n) {
    return (int)do_syscall(SYS_WRITE, fd, (long)buf, n);
}

static inline int sys_fork(void) {
    return (int)do_syscall(SYS_FORK, 0, 0, 0);
}

#endif


