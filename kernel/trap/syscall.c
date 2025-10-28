
#include "../proc/proc.h"

// 具体的系统调用实现函数
uint64 sys_exit(void) {
    int status;
    struct proc *p = myproc();
    // 从trapframe中获取参数
    status = p->trapframe->a0;
    printf("[U->K] sys_exit(%d) pid=%d\n", status, p->pid);
    
    // 关闭中断，然后调用exit
    intr_off();
    exit(status);
    return 0;  // 不会到达
}

uint64 sys_getpid(void) {
    struct proc *p = myproc();
    printf("[U->K] sys_getpid() pid=%d\n", p->pid);
    return p->pid;
}

uint64 sys_fork(void) {
    printf("[U->K] sys_fork()\n");
    return fork();
}

uint64 sys_wait(void) {
    struct proc *p = myproc();
    uint64 addr = p->trapframe->a0;
    printf("[U->K] sys_wait()\n");
    return wait(addr);
}

uint64 sys_write(void) {
    struct proc *p = myproc();
    int fd = p->trapframe->a0;
    uint64 buf = p->trapframe->a1;
    int n = p->trapframe->a2;

    // buf=walkaddr(p->pagetable, buf);
    
    printf("[U->K] sys_write(fd=%d, buf=0x%x, n=%d) pid=%d\n", fd, (uint64)buf, n, p->pid);
    buf=walkaddr(p->pagetable, buf);
    
    if(fd == 1) { // stdout
        char *p_buf = (char*)buf;
        for(int i = 0; i < n; i++) {
            cons_putc(p_buf[i]);
        }
        return n;
    }
    
    return -1; // 不支持的文件描述符
}

uint64 sys_read(void) {
    printf("[U->K] sys_read()\n");
    // TODO: 实现读取逻辑
    return 0;
}


uint64 sys_open(void) {
    printf("[U->K] sys_open()\n");
    // TODO: 实现文件打开逻辑
    return 0;
}

uint64 sys_close(void) {
    printf("[U->K] sys_close()\n");
    // TODO: 实现文件关闭逻辑
    return 0;
}

uint64 sys_exec(void) {
    printf("[U->K] sys_exec()\n");
    // TODO: 实现程序执行逻辑
    return 0;
}

uint64 sys_sbrk(void) {
    struct proc *p = myproc();
    int n = p->trapframe->a0;
    uint64 addr = p->sz;
    printf("[U->K] sys_sbrk(%d)\n", n);
    if(growproc(n) < 0)
        return -1;
    return addr;
}

// 系统调用分发函数
void
syscall(void)
{
    int num;
    struct proc *p = myproc();

    // 从trapframe获取系统调用号（a7寄存器）
    num = p->trapframe->a7;

    // 系统调用函数表
    static uint64 (*syscalls[])(void) = {
        [SYS_EXIT]   = sys_exit,
        [SYS_GETPID] = sys_getpid,
        [SYS_FORK]   = sys_fork,
        [SYS_WAIT]   = sys_wait,
        [SYS_READ]   = sys_read,
        [SYS_WRITE]  = sys_write,
        [SYS_OPEN]   = sys_open,
        [SYS_CLOSE]  = sys_close,
        [SYS_EXEC]   = sys_exec,
        [SYS_SBRK]   = sys_sbrk,
    };

    if(num > 0 && num < sizeof(syscalls)/sizeof(syscalls[0]) && syscalls[num]) {
        // 执行系统调用并将返回值放入a0寄存器
        p->trapframe->a0 = syscalls[num]();
    } else {
        printf("Unknown syscall %d from pid=%d\n", num, p->pid);
        p->trapframe->a0 = -1;
    }
}