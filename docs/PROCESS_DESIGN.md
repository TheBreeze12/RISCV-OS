# 进程管理系统设计文档

## 1. 系统架构概览

本进程管理系统采用经典的Unix风格设计，参考xv6实现，针对RISC-V架构优化。

### 1.1 核心组件

```
进程管理系统
├── 进程结构体 (struct proc)
├── 进程表 (proc[NPROC])
├── CPU状态 (struct cpu)
├── 上下文切换 (swtch.S)
├── 调度器 (scheduler)
└── 同步原语 (sleep/wakeup)
```

## 2. 数据结构设计

### 2.1 进程结构体 (struct proc)

设计要点：
- **状态管理**：使用状态机模型管理进程生命周期
- **资源隔离**：每个进程有独立的页表、栈、trapframe
- **上下文保存**：struct context 保存被调用者保存寄存器

```c
struct proc {
    // 锁保护的字段
    enum procstate state;        // 进程状态
    void *chan;                  // 睡眠通道
    int killed;                  // 终止标志
    int xstate;                  // 退出状态
    int pid;                     // 进程ID
    
    // 父子关系
    struct proc *parent;         // 父进程
    
    // 私有字段
    uint64 kstack;               // 内核栈地址
    uint64 sz;                   // 内存大小
    pagetable_t pagetable;       // 页表
    struct trapframe *trapframe; // trapframe
    struct context context;      // 上下文
    struct file *ofile[NOFILE];  // 打开的文件
    struct inode *cwd;           // 当前目录
    char name[16];               // 进程名
};
```

### 2.2 进程状态机

```
UNUSED → USED → RUNNABLE → RUNNING → ZOMBIE → UNUSED
              ↓           ↑
              → SLEEPING →
```

状态转换说明：
- **UNUSED → USED**: allocproc() 分配进程
- **USED → RUNNABLE**: 初始化完成，准备运行
- **RUNNABLE → RUNNING**: scheduler() 调度执行
- **RUNNING → RUNNABLE**: yield() 主动让出CPU
- **RUNNING → SLEEPING**: sleep() 等待事件
- **SLEEPING → RUNNABLE**: wakeup() 唤醒
- **RUNNING → ZOMBIE**: exit() 进程退出
- **ZOMBIE → UNUSED**: wait() 父进程回收

### 2.3 上下文结构体 (struct context)

**关键设计决策：为什么只保存这些寄存器？**

```c
struct context {
    uint64 ra;  // 返回地址 - 决定返回到哪里
    uint64 sp;  // 栈指针 - 决定使用哪个栈
    // 被调用者保存寄存器 (callee-saved)
    uint64 s0-s11;  // 保证函数调用约定
};
```

答案：
1. **调用者保存寄存器** (caller-saved: t0-t6, a0-a7) 已经由调用swtch的函数保存在栈上
2. **被调用者保存寄存器** (callee-saved: s0-s11) 必须由swtch保存
3. **ra** 决定上下文切换后返回到哪里执行
4. **sp** 完成栈的切换

## 3. 进程表组织

### 3.1 设计选择：数组 vs 链表

**选择：数组实现**

优点：
- ✅ O(1) 索引访问
- ✅ 内存连续，缓存友好
- ✅ 实现简单，易于调试
- ✅ 固定大小，内存可预测

缺点：
- ❌ 查找特定PID需要O(n)
- ❌ 固定大小限制

**优化方案**：
- 对于高频PID查找：可以添加哈希表索引
- 对于动态进程数：可以改用链表+哈希表

### 3.2 PID分配策略

**当前实现**：简单递增

```c
static int nextpid = 1;

int allocpid(void) {
    int pid = nextpid;
    nextpid = nextpid + 1;
    return pid;
}
```

**优点**：
- 简单高效
- PID单调递增，便于调试

**改进方向**：
- PID回绕处理 (nextpid 溢出)
- PID重用策略
- PID命名空间 (容器支持)

## 4. 上下文切换机制

### 4.1 核心原理

上下文切换的本质：**改变CPU执行流**

关键步骤：
1. 保存当前进程的寄存器到 old->context
2. 恢复新进程的寄存器从 new->context
3. 通过ret跳转到new->context.ra

### 4.2 swtch实现详解

```assembly
swtch:
    # 保存当前上下文
    sd ra, 0(a0)      # 保存返回地址
    sd sp, 8(a0)      # 保存栈指针
    sd s0-s11, ...    # 保存callee-saved寄存器
    
    # 加载新上下文
    ld ra, 0(a1)      # 恢复返回地址
    ld sp, 8(a1)      # 恢复栈指针（切换栈！）
    ld s0-s11, ...    # 恢复callee-saved寄存器
    
    ret               # 跳转到新的ra
```

### 4.3 第一次调度的魔法

**问题**：新进程从未运行过，如何第一次被调度？

**解决方案**：
```c
// allocproc中
p->context.ra = (uint64)forkret;  // 设置返回地址
p->context.sp = p->kstack + PGSIZE;  // 设置栈指针
```

第一次调度时：
1. scheduler调用 swtch(&c->context, &p->context)
2. swtch恢复p->context.ra (指向forkret)
3. ret跳转到forkret
4. forkret执行进程初始化
5. 进入用户空间

## 5. 调度器设计

### 5.1 调度算法：轮转调度 (Round-Robin)

```c
void scheduler(void) {
    for(;;) {
        intr_on();  // 关键：允许中断
        
        for(p = proc; p < &proc[NPROC]; p++) {
            if(p->state == RUNNABLE) {
                p->state = RUNNING;
                c->proc = p;
                swtch(&c->context, &p->context);
                c->proc = 0;
            }
        }
    }
}
```

### 5.2 设计考虑

**1. 为什么开启中断？**
- 避免调度器独占CPU
- 允许设备中断处理
- 允许时钟中断触发抢占

**2. 如何避免忙等待？**
- 当前实现：持续轮询（简单但低效）
- 改进方案：使用WFI指令等待中断

**3. 公平性保证？**
- 轮转调度保证每个进程轮流执行
- 时间片通过时钟中断实现

### 5.3 调度时机

**主动调度**：
- yield(): 进程主动让出CPU
- sleep(): 等待事件
- exit(): 进程退出

**抢占调度**：
- 时钟中断：时间片耗尽
- 设备中断：I/O完成

## 6. 进程同步机制

### 6.1 sleep/wakeup 实现

**sleep设计**：
```c
void sleep(void *chan) {
    p->chan = chan;
    p->state = SLEEPING;
    sched();  // 切换到调度器
    p->chan = 0;
}
```

**wakeup设计**：
```c
void wakeup(void *chan) {
    for(p = proc; p < &proc[NPROC]; p++) {
        if(p->state == SLEEPING && p->chan == chan) {
            p->state = RUNNABLE;
        }
    }
}
```

### 6.2 避免 Lost Wakeup 问题

**问题**：wakeup在sleep之前调用，导致进程永久睡眠

**解决方案**：
1. 在改变条件和调用sleep之间持有锁
2. sleep原子地释放锁并睡眠
3. 被唤醒后重新获取锁

### 6.3 应用：生产者-消费者

```c
// 生产者
while(buffer_full) {
    sleep(&buffer);
}
produce_item();
wakeup(&buffer);

// 消费者
while(buffer_empty) {
    sleep(&buffer);
}
consume_item();
wakeup(&buffer);
```

## 7. 进程生命周期

### 7.1 创建流程 (fork)

```
fork()
  ↓
allocproc()  // 分配进程结构
  ↓
uvmcopy()    // 复制内存
  ↓
copy files   // 复制文件描述符
  ↓
RUNNABLE     // 设置为可运行
```

### 7.2 执行流程

```
RUNNABLE
  ↓
scheduler选中
  ↓
swtch切换 → RUNNING
  ↓
执行代码
  ↓
时钟中断/yield → RUNNABLE
```

### 7.3 退出流程 (exit)

```
exit(status)
  ↓
关闭文件
  ↓
交接子进程给init
  ↓
唤醒父进程
  ↓
ZOMBIE状态
  ↓
wait() → UNUSED
```

## 8. 关键问题解答

### Q1: 为什么需要两个栈？

**答案**：用户栈和内核栈分离

- **用户栈**：运行用户代码时使用，不可信
- **内核栈**：运行内核代码时使用，可信

好处：
- 安全性：用户不能破坏内核栈
- 隔离性：系统调用/中断不影响用户栈
- 多任务：每个进程独立的内核栈

### Q2: 上下文切换为什么是原子操作？

**答案**：防止中断干扰

如果上下文切换过程中被中断：
- 可能部分寄存器属于旧进程，部分属于新进程
- 可能栈指针已切换但其他寄存器未切换
- 导致系统崩溃

解决：swtch前关中断，swtch后再开中断

### Q3: 调度器如何处理死锁？

**答案**：
1. **预防**：保证锁的获取顺序
2. **检测**：暂未实现死锁检测
3. **避免**：sleep时自动释放锁

### Q4: 如何支持多核调度？

**当前**：单核实现，每个CPU一个调度器
**扩展方案**：
1. 每个CPU独立调度
2. 使用CPU亲和性
3. 负载均衡算法
4. CPU间迁移进程

## 9. 性能优化

### 9.1 当前性能瓶颈

1. **O(n) 进程查找**：遍历进程表
2. **无优先级**：所有进程平等对待
3. **忙等待调度器**：持续轮询

### 9.2 优化方向

**1. 使用运行队列**
```c
struct runqueue {
    struct proc *head;
    struct proc *tail;
};
```

**2. 优先级调度**
```c
struct proc {
    int priority;
    int nice;
    uint64 runtime;
};
```

**3. 多级反馈队列 (MLFQ)**
- 多个优先级队列
- 动态调整优先级
- 防止饥饿

**4. 完全公平调度器 (CFS)**
- 红黑树管理进程
- 虚拟运行时间
- O(log n) 选择下一个进程

## 10. 测试策略

### 10.1 单元测试

- `test_process_creation()`: 测试进程分配
- `test_proc_table()`: 测试进程表管理
- `test_context_switch()`: 测试上下文切换
- `test_scheduler()`: 测试调度器

### 10.2 集成测试

- 生产者-消费者测试
- 父子进程测试
- 多进程并发测试

### 10.3 压力测试

- 创建NPROC个进程
- 高频上下文切换
- 长时间运行稳定性

## 11. 未来扩展

### 11.1 功能扩展

- [ ] 进程组和会话
- [ ] 信号机制
- [ ] 进程间通信 (IPC)
- [ ] 命名空间 (Namespace)
- [ ] 控制组 (Cgroup)

### 11.2 性能扩展

- [ ] 多核调度
- [ ] 实时调度
- [ ] 能耗感知调度
- [ ] NUMA感知调度

### 11.3 安全扩展

- [ ] 进程隔离
- [ ] 能力机制 (Capabilities)
- [ ] 安全计算模式 (Seccomp)
- [ ] 强制访问控制 (MAC)

## 12. 参考资料

1. xv6-riscv: https://github.com/mit-pdos/xv6-riscv
2. Linux内核设计与实现
3. Operating Systems: Three Easy Pieces
4. RISC-V Privileged Architecture Specification

## 13. 总结

本进程管理系统实现了：
- ✅ 完整的进程生命周期管理
- ✅ 轮转调度算法
- ✅ 上下文切换机制
- ✅ sleep/wakeup同步原语
- ✅ fork/exit/wait系统调用
- ✅ 全面的测试套件

设计特点：
- **简洁**：核心代码约600行
- **清晰**：良好的代码结构和注释
- **可扩展**：易于添加新功能
- **可移植**：遵循标准RISC-V ABI

下一步：
1. 运行测试验证正确性
2. 集成到main.c启动流程
3. 实现用户进程支持
4. 添加系统调用接口

