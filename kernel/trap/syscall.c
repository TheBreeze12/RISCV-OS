
#include "../proc/proc.h"
#include "../utils/console.h"
#include "../include/param.h"

// 声明exec函数
extern int exec(char *path, char **argv);

#define BACKSPACE 0x100

// 具体的系统调用实现函数
uint64 sys_exit(void) {
    int status;
    struct proc *p = myproc();
    // 从trapframe中获取参数
    status = p->trapframe->a0;
    // printf("[U->K] sys_exit(%d) pid=%d\n", status, p->pid);
    
    // 关闭中断，然后调用exit
    intr_off();
    exit(status);
    return 0;  // 不会到达
}

uint64 sys_getpid(void) {
    struct proc *p = myproc();
    // printf("[U->K] sys_getpid() pid=%d\n", p->pid);
    return p->pid;
}

uint64 sys_fork(void) {
    // printf("[U->K] sys_fork()\n");
    return fork();
}

uint64 sys_wait(void) {
    struct proc *p = myproc();
    uint64 addr = p->trapframe->a0;
    // printf("[U->K] sys_wait()\n");
    return wait(addr);
}

uint64 sys_write(void) {
    struct proc *p = myproc();
    int fd = p->trapframe->a0;
    uint64 buf = p->trapframe->a1;
    int n = p->trapframe->a2;
    
    // 参数验证
    if(n < 0)
        return -1;
    
    // printf("[U->K] sys_write(fd=%d, buf=0x%x, n=%d) pid=%d\n", fd, (uint64)buf, n, p->pid);
    
    if(fd == 1) { // stdout
        char kbuf[PGSIZE];  // 内核缓冲区
        int total_written = 0;
        uint64 srcva = buf;
        
        // 分段复制，处理跨页情况
        while(n > 0) {
            int to_copy = n > PGSIZE ? PGSIZE : n;
            
            // 从用户空间复制到内核缓冲区
            if(copyin(p->pagetable, kbuf, srcva, to_copy) < 0) {
                // 复制失败，返回已写入的字节数，或 -1 如果什么都没写
                return total_written > 0 ? total_written : -1;
            }
            
            // 输出到控制台
            for(int i = 0; i < to_copy; i++) {
                cons_putc(kbuf[i]);
            }
            
            total_written += to_copy;
            srcva += to_copy;
            n -= to_copy;
        }
        
        return total_written;
    }
    
    return -1; // 不支持的文件描述符
}

uint64 sys_read(void) {
    struct proc *p = myproc();
    int fd = p->trapframe->a0;
    uint64 buf = p->trapframe->a1;
    int n = p->trapframe->a2;
    
    // 参数验证
    if(n < 0)
        return -1;
    
    // printf("[U->K] sys_read(fd=%d, buf=0x%x, n=%d) pid=%d\n", fd, (uint64)buf, n, p->pid);
    
    if(fd == 0) { // stdin
        char kbuf[PGSIZE];  // 内核缓冲区
        int total_read = 0;
        uint64 dstva = buf;
        int c;
        
        // 读取字符直到达到n个字符或遇到换行符
        while(total_read < n) {
            // 从控制台读取一个字符
            c = cons_getc();
            
            // 如果遇到换行符，也将其包含在结果中
            if(c == '\n') {
                kbuf[total_read++] = '\n';
                cons_putc('\n');  // 回显换行
                break;
            }
            
            // 处理退格键
            if(c == BACKSPACE) {
                if(total_read > 0) {
                    total_read--;
                    // 回显退格效果
                    cons_putc(BACKSPACE);
                }
                continue;
            }
            
            // 普通字符，回显并保存
            kbuf[total_read++] = (char)c;
            cons_putc(c);  // 回显输入
            
            // 注意：不退格时，我们不在循环中复制到用户空间
            // 因为退格可能会影响已经复制的内容
        }
        
        // 将数据复制到用户空间（一次性复制，避免退格问题）
        if(total_read > 0) {
            if(copyout(p->pagetable, dstva, kbuf, total_read) < 0) {
                return total_read > 0 ? total_read : -1;
            }
        }
        
        return total_read;
    }
    
    return -1; // 不支持的文件描述符
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
    struct proc *p = myproc();
    uint64 path_addr = p->trapframe->a0;  // 程序路径
    uint64 argv_addr = p->trapframe->a1;  // 参数数组地址
    
    char path[MAXPATH];
    char *argv[MAXARG];
    char kargv[MAXARG][MAXPATH];
    
    // 从用户空间读取路径
    if(copyin_str(p->pagetable, path, path_addr, MAXPATH) < 0) {
        printf("[U->K] sys_exec: 无法读取路径\n");
        return -1;
    }
    
    // 从用户空间读取参数数组
    // argv 是一个指针数组，每个元素指向一个字符串
    int argc = 0;
    for(int i = 0; i < MAXARG; i++) {
        uint64 arg_ptr;
        // 读取 argv[i] 的值（一个指针）
        if(copyin(p->pagetable, (char*)&arg_ptr, argv_addr + i * sizeof(uint64), sizeof(uint64)) < 0) {
            break;
        }
        
        // 如果指针为NULL，说明参数列表结束
        if(arg_ptr == 0) {
            break;
        }
        
        // 读取参数字符串
        if(copyin_str(p->pagetable, kargv[i], arg_ptr, MAXPATH) < 0) {
            printf("[U->K] sys_exec: 无法读取参数 %d\n", i);
            return -1;
        }
        
        argv[i] = kargv[i];
        argc++;
    }
    argv[argc] = 0;  // 参数列表以NULL结尾
    
    printf("[U->K] sys_exec(path='%s', argc=%d)\n", path, argc);
    
    // 调用exec函数执行程序
    int ret = exec(path, argv);
    if(ret < 0) {
        printf("[U->K] sys_exec: 执行失败\n");
        return -1;
    }
    
    // exec成功不会返回，但如果返回了，说明出错
    return ret;
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

uint64 sys_sleep(void) {
    printf("[U->K] sys_sleep()\n");
    struct proc *p = myproc();
    int n;  // 睡眠秒数
    uint64 ticks;  // 时钟周期数
    
    // 从 trapframe 获取参数（秒数）
    n = p->trapframe->a0;
    
    if(n < 0) {
      return -1;
    }
    
    // 将秒数转换为时钟周期数
    // 假设 CPU 频率为 1GHz（1秒 = 1,000,000,000 时钟周期）
    // 或者使用定时器频率：如果定时器每 1ms 中断一次，则 1秒 = 1000 * 1000000 周期
    ticks = (uint64)n * 10000000;  // n 秒 = n * 10^9 周期
    // 如果 n == 0，直接返回
    if(n == 0) {
      return 0;
    }
    
    
    // 关闭中断，然后睡眠
    intr_off();
    sleep_ticks(ticks);
    intr_on();
    
    return 0;
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
        [SYS_SLEEP]  = sys_sleep,  
    };

    if(num > 0 && num < sizeof(syscalls)/sizeof(syscalls[0]) && syscalls[num]) {
        // 执行系统调用并将返回值放入a0寄存器
        p->trapframe->a0 = syscalls[num]();
    } else {
        printf("Unknown syscall %d from pid=%d\n", num, p->pid);
        p->trapframe->a0 = -1;
    }
}