#include "proc.h"
extern char trampoline[];

// 全局进程表和CPU数组
struct cpu cpus[NCPU];
struct proc proc[NPROC];
struct proc *initproc;
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
  p->sz = 0;  // 初始化进程大小为0
  p->parent = 0;
  p->killed = 0;
  p->xstate = 0;
  p->chan = 0;
  // 分配trapframe页
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    return 0;
  }
  
  // 初始化trapframe为0，避免未初始化的值
  memset(p->trapframe, 0, sizeof(struct trapframe));

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
  p->wake_time = 0;
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
  p->wake_time = 0;
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
  // 先清除 TRAMPOLINE 和 TRAPFRAME 的映射（不释放物理页）
  // 必须在释放用户内存之前清除，因为 walk 需要访问页表
  pte_t *pte;
  if((pte = walk(pagetable, TRAMPOLINE, 0)) != 0 && (*pte & PTE_V))
    *pte = 0;
  
  if((pte = walk(pagetable, TRAPFRAME, 0)) != 0 && (*pte & PTE_V))
    *pte = 0;
  
  // 然后释放用户内存（这会清除所有用户页的映射并释放物理页）
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  
  // 现在可以安全地释放页表结构了
  free_pagetable(pagetable);
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
    // 初始化进程锁 
    memset(p, 0, sizeof(struct proc));
  }
}

// ============================================================================
// 任务7：第一个用户进程 - userinit()
// 创建第一个用户进程，执行init程序
// ============================================================================

// 引入外部生成的用户程序镜像


void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;
  
  if(p == 0)
  panic("userinit: allocproc failed");

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
    // The most recent process to run may have had interrupts
    // turned off; enable them to avoid a deadlock if all
    // processes are waiting. Then turn them back off
    // to avoid a possible race between an interrupt
    // and wfi.
    // intr_on();
    // printf("scheduler start\n");
    for(p = proc; p < &proc[NPROC]; p++) {
     
      if(p->state == RUNNABLE) {
        // printf("scheduler: process %d is runnable\n", p->pid);
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
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
  sched();

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
  intr_on();
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

  // printf("sched: proc %d\n", p->pid);

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

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;  // init进程
      if(initproc)
        wakeup(initproc);
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
            if(copyout(p->pagetable, addr, (char *)&pp->xstate, sizeof(pp->xstate)) < 0){
              return -1;
            }
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
  
  // 确保epc指向正确的地址（sys_fork的返回地址）
  // 注意：epc应该已经在usertrap中正确设置了

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
  struct proc *p = myproc();


  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    printf("第一个进程初始化，准备切换到用户态...\n");
    
    // 确保中断已启用，以便磁盘 I/O 可以完成
    if(!intr_get()) {
      printf("forkret: enabling interrupts...\n");
      intr_on();
    }
    
    // ensure other cores see first=0.
    printf("forkret: calling fsinit...\n");
    fsinit(ROOTDEV);
    printf("forkret: fsinit completed\n");

    first = 0;
    // ensure other cores see first=0.
    __sync_synchronize();
    // We can invoke kexec() now that file system is initialized.
    // Put the return value (argc) of kexec into a0.
    p->trapframe->a0 = exec("/shell", (char *[]){ "/shell", 0 });
    if (p->trapframe->a0 == -1) {
      panic("exec");
    }
  }

  // return to user space, mimicing usertrap()'s return.
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
    if((sz = uvmalloc(p->pagetable, sz, sz + n,PTE_W)) == 0) {
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



// ============================================================================
// 基于时间的睡眠 - sleep_ticks()
// 让进程睡眠指定的时钟周期数
// ============================================================================

void
sleep_ticks(uint64 ticks)
{
  struct proc *p = myproc();
  uint64 wake_time;
  
  if(p == 0)
    panic("sleep_ticks: no proc");
  
  // 计算唤醒时间（当前时间 + 睡眠时长）
  wake_time = r_time() + ticks;
  
  // 设置唤醒时间和睡眠通道（使用 wake_time 作为唯一标识）
  p->wake_time = wake_time;
  p->chan = (void*)wake_time;  // 使用 wake_time 作为 chan
  p->state = SLEEPING;
  
  // 切换到调度器
  sched();
  
  // 被唤醒后，清除 chan
  p->chan = 0;
  p->wake_time = 0;
}

// ============================================================================
// 唤醒所有到期的睡眠进程
// 在定时器中断中调用
// ============================================================================

void
wakeup_timer(void)
{
  struct proc *p;
  uint64 now = r_time();
  
  // 遍历所有进程，唤醒到期的进程
  for(p = proc; p < &proc[NPROC]; p++){
  
    if(p->state == SLEEPING && p->wake_time != 0) {
      if(now >= p->wake_time) {
        // 时间到了，唤醒进程
        p->state = RUNNABLE;
        p->wake_time = 0;
      }
    }
  }
}

void
sleep_lock(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep_lock: no proc");
  
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;
  
  // Release the lock before sleeping, so interrupt handler can acquire it
  release(lk);
  
  sched();
  
  // Tidy up.
  p->chan = 0;

  // Reacquire original lock after waking up
  acquire(lk);
}

// Wake up all processes sleeping on channel chan.
// Caller should hold the condition lock.
void
wakeup_lock(void *chan)
{
  struct proc *p;

  // Check all processes, including the current one (if any)
  // In interrupt context, myproc() may return the sleeping process
  for(p = proc; p < &proc[NPROC]; p++) {
    if(p->state == SLEEPING && p->chan == chan) {
      p->state = RUNNABLE;
    }
  }
}
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}
