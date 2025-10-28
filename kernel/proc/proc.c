#include "proc.h"
extern char trampoline[];

// 全局进程表和CPU数组
struct cpu cpus[NCPU];
struct proc proc[NPROC];

// PID分配
static int nextpid = 1;

// 进程表查找
struct proc* myproc(void);

// ============================================================================
// 任务1：PID分配策略
// 设计：简单递增策略，考虑回绕
// ============================================================================

int
allocpid(void)
{
  int pid;
  
  pid = nextpid;
  nextpid = nextpid + 1;

  return pid;
}

// ============================================================================
// 任务2：进程查找和管理
// 设计：使用数组实现进程表，简单但高效
// 优点：O(1)索引访问，内存连续，缓存友好
// 缺点：固定大小，查找特定PID需要O(n)
// ============================================================================

// 返回当前CPU上运行的进程
struct proc*
myproc(void)
{
  // 在单CPU系统中，直接返回cpus[0].proc
  // push_off();
  struct cpu *c = &cpus[0];
  struct proc *p = c->proc;
  // pop_off();
  return p;
}

// 获取当前CPU
struct cpu*
mycpu(void)
{
  // 单CPU系统
  return &cpus[0];
}

// 通过PID查找进程 - O(n)复杂度
// 优化方案：可以使用哈希表将复杂度降至O(1)
struct proc*
find_proc_by_pid(int pid)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    if(p->pid == pid) {
      return p;
    }
  }
  return 0;
}

// ============================================================================
// 任务3：进程创建 - allocproc()
// 核心功能：
// 1. 在进程表中查找UNUSED槽位
// 2. 分配PID
// 3. 分配trapframe
// 4. 分配用户页表
// 5. 分配内核栈
// ============================================================================

struct proc*
allocproc(void)
{
  struct proc *p;

  // 遍历进程表查找空闲槽位
  for(p = proc; p < &proc[NPROC]; p++) {
    if(p->state == UNUSED) {
      goto found;
    }
  }
  return 0;  // 进程表已满

found:
  p->pid = allocpid();
  p->state = USED;

  // 分配trapframe页
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    return 0;
  }

  // 创建空的用户页表
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    return 0;
  }

  // 分配内核栈
  // 注意：这里假设已经在虚拟内存中映射了内核栈区域
  if((p->kstack = (uint64)kalloc()) == 0) {
    freeproc(p);
    return 0;
  }

  // 初始化上下文，准备第一次调度
  // 设置返回地址指向forkret，这样第一次调度时会跳转到forkret
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;
  strcpy(p->name, "allocproc");

  return p;
}

// ============================================================================
// 任务4：进程资源释放
// 关键：确保所有资源都被正确释放，防止内存泄漏
// ============================================================================

void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  
  if(p->kstack)
    kfree((void*)p->kstack);
  p->kstack = 0;
  
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}

// ============================================================================
// 任务5：用户页表管理
// ============================================================================

pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // 创建空页表
  pagetable = create_pagetable();
  if(pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
    (uint64)trampoline, PTE_R | PTE_X) < 0){
  uvmfree(pagetable, 0);
  return 0;
}

// map the trapframe page just below the trampoline page, for
// trampoline.S.
if(mappages(pagetable, TRAPFRAME, PGSIZE,
    (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
uvmunmap(pagetable, TRAMPOLINE, 1, 0);
uvmfree(pagetable, 0);
return 0;
}


  return pagetable;
}

// 释放用户页表
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// ============================================================================
// 任务6：进程初始化
// 在系统启动时调用，初始化进程表
// ============================================================================

void
procinit(void)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    p->state = UNUSED;
    p->kstack = 0;
  }
}

// ============================================================================
// 任务7：第一个用户进程 - userinit()
// 创建第一个用户进程，执行init程序
// ============================================================================

// 引入外部生成的用户程序镜像
#include "/home/thebreeze/riscv-os/user/initcode.h"

void
userinit(void)
{
  struct proc *p;
  pte_t *pte;

  p = allocproc();
  if(p == 0)
    panic("userinit: allocproc failed");

  // 分配足够的用户内存并复制initcode
  p->sz = uvmalloc(p->pagetable, 0, PGROUNDUP(initcode_size));
  if(p->sz == 0)
    panic("userinit: uvmalloc failed");
  
  // 获取物理地址并转换为内核虚拟地址
  pte = walk(p->pagetable, 0, 0);
  if(pte == 0)
    panic("userinit: walk failed");
  
  uint64 pa = PTE2PA(*pte);
  memmove((void*)pa, initcode, initcode_size);
  
  
  
  // 设置trapframe，准备返回用户态
  memset(p->trapframe, 0, sizeof(*p->trapframe));
  p->trapframe->epc = 0;  // 用户程序从地址0开始
  p->trapframe->sp = PGSIZE;  // 栈指针设置在页的顶部（简单起见用一页栈）
  
  strcpy(p->name, "initcode");
  p->state = RUNNABLE;
  
}

// ============================================================================
// 任务8：进程调度 - scheduler()
// 实现轮转调度算法（Round-Robin）
// 设计考虑：
// 1. 简单公平：每个进程轮流执行
// 2. 避免忙等：开启中断，允许设备工作
// 3. 多核支持：每个CPU独立调度
// ============================================================================

void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  
  c->proc = 0;
  
  for(;;){
    // 开启中断，允许设备中断
    // 这很关键：避免调度器独占CPU，让设备可以工作
    intr_on();

    // 遍历进程表，查找RUNNABLE进程
    for(p = proc; p < &proc[NPROC]; p++) {
      
      if(p->state != RUNNABLE)
        continue;

      // 找到可运行进程，切换到该进程
      p->state = RUNNING;
      c->proc = p;
      
      // 上下文切换：从调度器切换到进程
      // 关键：swtch保存当前上下文到c->context
      //       恢复p->context中的上下文
      swtch(&c->context, &p->context);

      // 当进程再次调度回调度器时，会从这里继续执行
      c->proc = 0;
    }
  }
}

// ============================================================================
// 任务9：主动让出CPU - yield()
// 进程主动放弃CPU，触发重新调度
// ============================================================================

void
yield(void)
{
  struct proc *p = myproc();
  if(p == 0)
    return;
  // printf("yield: %d\n", p->state);
    
  p->state = RUNNABLE;
  intr_off();
  sched();
  intr_on();

}

// ============================================================================
// 任务10：调度到调度器 - sched()
// 从进程上下文切换回调度器
// 安全检查：
// 1. 确保持有进程锁
// 2. 确保中断已关闭
// 3. 确保不是在中断中调度
// ============================================================================

void
sched(void)
{
  struct proc *p = myproc();
  struct cpu *c = mycpu();

  if(p == 0)
    panic("sched: no proc");

  // 安全检查
  if(p->state == RUNNING)
  {
    printf("sched: process running %s\n", p->name);
    panic("sched: process running");
  }
  if(intr_get())
    panic("sched: interruptible");

  // 切换回调度器上下文
  swtch(&p->context, &c->context);
}

// ============================================================================
// 任务11：进程同步 - sleep/wakeup
// 实现条件变量机制，解决生产者-消费者问题
// 
// sleep设计：
// 1. 原子地释放锁并睡眠
// 2. 避免lost wakeup问题
// 3. 被唤醒后重新获取锁
// ============================================================================

void
sleep(void *chan)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep: no proc");

  // 必须持有进程锁才能改变状态
  p->chan = chan;
  p->state = SLEEPING;

  // 切换到调度器
  sched();

  // 被唤醒后，清除chan
  p->chan = 0;
}

// 唤醒所有等待chan的进程
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;
      }
    }
  }
}

// ============================================================================
// 任务12：进程退出 - exit()
// 1. 关闭所有打开的文件
// 2. 唤醒等待的父进程
// 3. 将子进程交给init进程
// 4. 进入ZOMBIE状态
// ============================================================================

void
exit(int status)
{
  struct proc *p = myproc();
  struct proc *pp;

  if(p == 0)
    panic("exit: no proc");

  // 关闭所有打开的文件
  // TODO: close all open files

  p->xstate = status;
  p->state = ZOMBIE;

  // 将子进程交给init
  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = &proc[0];  // init进程
      wakeup(&proc[0]);
    }
  }

  // 唤醒父进程
  if(p->parent)
    wakeup(p->parent);

  // 跳转到调度器，不再返回
  sched();
  panic("exit: zombie exit");
}

// ============================================================================
// 任务13：等待子进程 - wait()
// 1. 查找ZOMBIE子进程
// 2. 如果有，清理并返回
// 3. 如果没有，睡眠等待
// ============================================================================

int
wait(uint64 addr)
{
  struct proc *pp;
  int havekids;
  int pid;
  struct proc *p = myproc();

  for(;;){
    havekids = 0;
    for(pp = proc; pp < &proc[NPROC]; pp++){
      if(pp->parent == p){
        havekids = 1;
        
        if(pp->state == ZOMBIE){
          // 找到僵尸子进程
          pid = pp->pid;
          if(addr != 0) {
            // 复制退出状态到用户空间
            // TODO: copyout to user space
          }
          freeproc(pp);
          return pid;
        }
      }
    }

    // 没有子进程
    if(!havekids){
      return -1;
    }

    // 等待子进程退出
    sleep(p);
  }
}

// ============================================================================
// 任务14：fork系统调用
// 创建当前进程的副本
// ============================================================================

int
fork(void)
{
  int pid;
  struct proc *np;
  struct proc *p = myproc();

  // 分配新进程
  np = allocproc();
  if(np == 0){
    return -1;
  }

  // 复制用户内存
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    return -1;
  }
  np->sz = p->sz;

  // 复制trapframe
  *(np->trapframe) = *(p->trapframe);

  // fork返回0给子进程
  np->trapframe->a0 = 0;

  // 复制打开的文件描述符
  // TODO: copy open files

  np->parent = p;
  strcpy(np->name, p->name);

  pid = np->pid;

  np->state = RUNNABLE;

  return pid;
}

// ============================================================================
// 任务15：forkret - 新进程第一次运行
// allocproc将ra设置为forkret，所以新进程第一次运行会进入这里
// ============================================================================

void
forkret(void)
{
  static int first = 1;

  // 第一个进程需要初始化文件系统
  if (first) {
    first = 0;
    // TODO: fsinit(ROOTDEV);
    printf("第一个进程初始化，准备切换到用户态...\n");
  }

  // 返回到用户空间
  usertrapret();
}

// ============================================================================
// 任务16：进程增长 - growproc()
// 增长或缩小进程的用户内存
// ============================================================================

int
growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// ============================================================================
// 任务17：杀死进程 - kill()
// 设置killed标志，让进程在合适的时候退出
// ============================================================================

int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // 唤醒进程，让它检查killed标志
        p->state = RUNNABLE;
      }
      return 0;
    }
  }
  return -1;
}

// 检查当前进程是否被杀死
int
killed(struct proc *p)
{
  return p->killed;
}

void
setkilled(struct proc *p)
{
  p->killed = 1;
}

// ============================================================================
// 任务18：CPU ID - cpuid()
// 返回当前CPU的ID
// ============================================================================

int
cpuid()
{
  // 单CPU系统，总是返回0
  return 0;
}

