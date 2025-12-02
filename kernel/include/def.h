#ifndef DEF_H
#define DEF_H

#include "riscv.h"
#include "type.h"
#include "param.h"
#include "../mm/memlayout.h"

// Forward declarations
struct proc;
struct cpu;
struct context;
struct k_trapframe;
struct spinlock;
struct sleeplock;
struct buf;
struct inode;
struct superblock;
struct file;
struct stat;

// 自定义assert宏
#define assert(condition) \
    do { \
        if (!(condition)) { \
            printf("Assertion failed: %s, file %s, line %d\n", \
                   #condition, __FILE__, __LINE__); \
            panic("Assertion failed"); \
        } \
    } while(0)

//printf.c
void printint(long long value, int base, int sgn);
void printf(const char *fmt, ...);
void panic(char *s);
void clear_screen(void);
void clear_line(void);
void goto_xy(int x, int y);
void set_text_color(const char *color);
void set_background_color(const char *color);
void reset_colors(void);
void printf_color(const char *color, const char *fmt, ...);

//uart.c
void uart_putc(char c);
char uart_getc(void);

//console.c
void cons_putc(int c);
void cons_puts(const char *s);
int cons_getc(void);

// kalloc.c
void*           kalloc(void);
void            kfree(void *);
void            kinit(void);

// string.c
void* memset(void *dst, int c, uint n);
char* strcpy(char *dst, const char *src);
int   strcmp(const char *p, const char *q);
int   strlen(const char *s);
char* safestrcpy(char *s, const char *t, int n);
int   sprintf(char *dst, const char *fmt, ...);
void* memmove(void *dst, const void *src, uint n);
int   strncmp(const char *p, const char *q, uint n);
char* strncpy(char *s, const char *t, int n);


// vm.c
void            kvminit(void);
void            kvminithart(void);
void            kvmmap(pagetable_t, uint64, uint64, uint64, int);

int             mappages(pagetable_t, uint64, uint64, uint64, int);
pagetable_t     create_pagetable(void);
void            free_pagetable(pagetable_t);
pte_t *         walk(pagetable_t, uint64, int);
uint64          walkaddr(pagetable_t, uint64);
int             ismapped(pagetable_t, uint64);
void            uvmunmap(pagetable_t, uint64, uint64, int);
void            uvmfree(pagetable_t, uint64);
int             uvmcopy(pagetable_t, pagetable_t, uint64);
uint64         uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm);
uint64          uvmdealloc(pagetable_t, uint64, uint64);
int             copyout(pagetable_t, uint64, char *, uint64);
int             copyin(pagetable_t, char *, uint64, uint64);
int             copyin_str(pagetable_t, char *, uint64, uint64);
void            uvmclear(pagetable_t, uint64);
// trap.c
void trapinithart(void);
void test_timer_interrupt(void);
void test_breakpoint(void);
void test_syscall(void);
void test_exception(void);
void usertrap(void);
void usertrapret(void);
void kerneltrap(struct k_trapframe *tf);
void sbi_set_timer(uint64 time);
// plic.c
void plicinit(void);
void plicinithart(void);
int plic_claim(void);
void plic_complete(int);

// 全局变量外部声明
extern volatile int global_interrupt_count;

// RISC-V异常和中断类型定义
#define CAUSE_INTERRUPT_FLAG    0x8000000000000000L

// 异常类型 (scause & ~CAUSE_INTERRUPT_FLAG)
#define CAUSE_INSTRUCTION_MISALIGNED    0 // 指令地址未对齐
#define CAUSE_INSTRUCTION_ACCESS_FAULT  1 // 指令访问故障
#define CAUSE_ILLEGAL_INSTRUCTION       2 // 非法指令
#define CAUSE_BREAKPOINT               3 // 断点指令
#define CAUSE_LOAD_MISALIGNED          4 // 加载地址未对齐
#define CAUSE_LOAD_ACCESS_FAULT        5 // 加载访问故障
#define CAUSE_STORE_MISALIGNED         6 // 存储地址未对齐
#define CAUSE_STORE_ACCESS_FAULT       7 // 存储访问故障
#define CAUSE_USER_ECALL               8 // 用户模式系统调用
#define CAUSE_SUPERVISOR_ECALL         9 // 特权模式系统调用
#define CAUSE_MACHINE_ECALL            11 // 机器模式系统调用
#define CAUSE_INSTRUCTION_PAGE_FAULT   12 // 指令页故障
#define CAUSE_LOAD_PAGE_FAULT          13 // 加载页故障
#define CAUSE_STORE_PAGE_FAULT         15 // 存储页故障

// 中断类型 (scause & ~CAUSE_INTERRUPT_FLAG)
#define CAUSE_SOFTWARE_INTERRUPT       1 // 软件中断
#define CAUSE_TIMER_INTERRUPT          5 // 定时器中断
#define CAUSE_EXTERNAL_INTERRUPT       9 // 外部中断

// 系统调用号定义
#define SYS_EXIT    1
#define SYS_GETPID  2
#define SYS_FORK    3
#define SYS_WAIT    4
#define SYS_READ    5
#define SYS_WRITE   6
#define SYS_OPEN    7
#define SYS_CLOSE   8
#define SYS_EXEC    9
#define SYS_SBRK    10
#define SYS_SLEEP   11
#define SYS_FSTAT   12
#define SYS_UNLINK  13
#define SYS_MKDIR   14
// 陷阱帧结构体定义
struct k_trapframe {
     /*   0 */ uint64 ra;
    /*   8 */ uint64 sp;
    /*  16 */ uint64 gp;
    /*  24 */ uint64 tp;
    /*  32 */ uint64 t0;
    /*  40 */ uint64 t1;
    /*  48 */ uint64 t2;
    /*  56 */ uint64 s0;
    /*  64 */ uint64 s1;
    /*  72 */ uint64 a0;
    /*  80 */ uint64 a1;
    /*  88 */ uint64 a2;
    /*  96 */ uint64 a3;
    /* 104 */ uint64 a4;
    /* 112 */ uint64 a5;
    /* 120 */ uint64 a6;
    /* 128 */ uint64 a7;
    /* 136 */ uint64 s2;
    /* 144 */ uint64 s3;
    /* 152 */ uint64 s4;
    /* 160 */ uint64 s5;
    /* 168 */ uint64 s6;
    /* 176 */ uint64 s7;
    /* 184 */ uint64 s8;
    /* 192 */ uint64 s9;
    /* 200 */ uint64 s10;
    /* 208 */ uint64 s11;
    /* 216 */ uint64 t3;
    /* 224 */ uint64 t4;
    /* 232 */ uint64 t5;
    /* 240 */ uint64 t6;
    /* 248 */ uint64 epc;    // 单独处理，不在栈上
};

// 异常处理函数声明
void handle_exception(struct k_trapframe *tf);
void handle_syscall(struct k_trapframe *tf);
void handle_instruction_page_fault(struct k_trapframe *tf);
void handle_load_page_fault(struct k_trapframe *tf);
void handle_store_page_fault(struct k_trapframe *tf);

// syscall.c
uint64 sys_exit(void);
uint64 sys_getpid(void);
uint64 sys_fork(void);
uint64 sys_wait(void);
uint64 sys_read(void);
uint64 sys_write(void);
uint64 sys_open(void);
uint64 sys_close(void);
uint64 sys_exec(void);
uint64 sys_sbrk(void);
uint64 sys_fstat(void);
uint64 sys_unlink(void);
uint64 sys_mkdir(void);
void syscall(void);

// proc.c
struct proc* allocproc(void);
 void freeproc(struct proc *p);

int             cpuid(void);
int             allocpid(void);
void            userinit(void);
struct proc*    find_proc_by_pid(int);
pagetable_t     proc_pagetable(struct proc *);
void            proc_freepagetable(pagetable_t, uint64);
struct cpu*     mycpu(void);
struct proc*    myproc(void);
void            procinit(void);
void            scheduler(void) __attribute__((noreturn));
void            yield(void);
void            sched(void);
void            sleep(void *chan);
void            sleep_lock(void *chan, struct spinlock *lk);
void            wakeup(void *chan);
void            wakeup_lock(void *chan);
void            exit(int status);
int             wait(uint64 addr);
int             fork(void);
void            forkret(void);
int             growproc(int n);
int             kill(int pid);
int             killed(struct proc *p);
void            setkilled(struct proc *p);
void            wakeup_timer(void);
void            sleep_ticks(uint64 ticks);
int             either_copyout(int user_dst, uint64 dst, void *src, uint64 len);
int             either_copyin(void *dst, int user_src, uint64 src, uint64 len);
// proc_test.c
void            run_all_proc_tests(void);
void            test_process_creation(void);
void            test_proc_table(void);
void            test_context_switch(void);
void            test_scheduler(void);
void            test_synchronization(void);
void            debug_proc_table(void);

// swtch.S
void            swtch(struct context *, struct context *);

// exec.c
int exec(char *path, char **argv);

// file.c
struct file* filealloc(void);
void fileclose(struct file*);
struct file* filedup(struct file*);
int fileread(struct file *f, uint64 addr, int n);
int filewrite(struct file *f, uint64 addr, int n);
int filestat(struct file *f, uint64 addr);
void fileinit(void);

// namei.c


// sleeplock.c
void initsleeplock(struct sleeplock *lk, char *name);
void acquiresleep(struct sleeplock *lk);
void releasesleep(struct sleeplock *lk);
int holdingsleep(struct sleeplock *lk);

// spinlock.c
void initlock(struct spinlock *lk, char *name);
void acquire(struct spinlock *lk);
void release(struct spinlock *lk);
int holding(struct spinlock *lk);
void push_off(void);
void pop_off(void);

// virtio_disk.c
void            virtio_disk_init(void);
void            virtio_disk_rw(struct buf *, int);
void            virtio_disk_intr(void);

// bio.c
void binit(void);
void bwrite(struct buf *b);
void brelse(struct buf *b);
void bpin(struct buf *b);
void bunpin(struct buf *b);
struct buf* bread(uint dev, uint blockno);
// fs.c
void fsinit(int dev);
void iinit(void);
// Note: iget is static in fs.c, so not declared here
struct inode* ialloc(uint dev, short type);
void ilock(struct inode *ip);
void iunlock(struct inode *ip);
void iupdate(struct inode *ip);
void iput(struct inode *ip);
void iunlockput(struct inode *ip);
void itrunc(struct inode *ip);
struct inode* idup(struct inode *ip);
int readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n);
int writei(struct inode *ip, int user_src, uint64 src, uint off, uint n);
void iclaim(int dev);
void ireclaim(int dev);
void stati(struct inode *ip, struct stat *st);
struct inode* namei(char *path);
struct inode* nameiparent(char *path, char *name);
struct inode* dirlookup(struct inode *dp, char *name, uint *poff);
int dirlink(struct inode *dp, char *name, uint inum);
struct inode* create(char *path, short type, short major, short minor);

// log.c
void initlog(int dev, struct superblock *sb);
void begin_op(void);
void end_op(void);
void log_write(struct buf *b);
void recover_from_log(void);
void log_stats(void);
int log_check(void);
// Note: commit, write_log, write_head, read_head, install_trans 
// are static in log.c, so not declared here

#endif // DEF_H
