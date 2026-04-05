# RISC-V OS 项目系统复习文档

> 面向目标：快速找回这个项目的实现脉络，同时把操作系统核心知识和项目代码一一对应。  
> 使用方式：先读第 1-3 节建立全局图，再按你想复习的模块跳读，第 10 节是本次关键 bug 的完整案例。

---

## 1. 项目是什么

这是一个在 RISC-V QEMU virt 平台上运行的教学型内核，完成了一个最小 Unix-like 系统的主干闭环：

1. 启动与特权级切换（M 模式进入 S 模式）
2. 虚拟内存和页表
3. 中断/异常/系统调用
4. 进程管理与调度
5. 文件系统与块设备
6. 用户态程序和 shell

你可以把它理解为：

- 内核态提供抽象：进程、文件、地址空间
- 用户态通过系统调用访问这些抽象
- shell 是用户接口，驱动 hello/ls/cat 等程序运行

---

## 2. 整体运行链路（从上电到 hello）

### 2.1 启动阶段

1. `_start` 在汇编入口执行，位于 `kernel/boot/entry.S`
2. 清 BSS、建立早期栈 `stack0`
3. 跳转到 `start()`，位于 `kernel/boot/start.c`
4. 在 `start()` 中配置委托、中断、PMP、定时器，`mret` 进入 S 模式
5. 跳到 `main()`，位于 `kernel/main.c`

### 2.2 内核初始化阶段

`main()` 里初始化顺序很关键：

1. 物理页分配器 `kinit`
2. 内核页表 `kvminit/kvminithart`
3. 进程系统 `procinit`
4. trap 和中断控制器 `trapinithart/plicinit/plicinithart`
5. 文件系统子系统 `binit/iinit/fileinit/virtio_disk_init`
6. 创建第一个进程 `userinit`
7. 进入 `scheduler()` 调度循环

### 2.3 进入用户空间阶段

1. 新进程第一次被调度时，从 `forkret()` 起步（`kernel/proc/proc.c`）
2. `forkret()` 中调用 `fsinit()`，再执行 `exec("/shell", ...)`
3. 返回用户态后，shell 运行
4. shell 输入 `hello`，通过 `fork + exec` 启动子进程

---

## 3. 代码目录速览

- `kernel/boot`：启动入口、模式切换、定时器初始配置
- `kernel/mm`：页表、地址映射、用户内存分配复制
- `kernel/trap`：中断、异常、系统调用分发
- `kernel/proc`：进程结构、创建退出、调度、上下文切换
- `kernel/fs`：缓存、日志、inode、目录、文件对象
- `kernel/utils`：锁、字符串、控制台、串口
- `user`：用户程序与 shell
- `tools/mkfs`：构建文件系统镜像

---

## 4. 启动与特权级知识（结合代码）

## 4.1 为什么要分 entry.S 和 start.c

在刚上电时，C 运行环境还不存在，先用汇编做最小准备：

1. 清 BSS（避免全局未初始化变量脏值）
2. 放置栈指针
3. 跳到 C 函数

这部分在 `kernel/boot/entry.S`。

## 4.2 M 模式到 S 模式做了什么

`kernel/boot/start.c` 的 `start()` 做了几件经典操作：

1. `mstatus.MPP` 设为 S
2. `mepc` 设为 `main`
3. 关闭分页（早期）
4. 委托异常和中断给 S 态
5. 配置 PMP 让 S 态可访问物理内存
6. 开启 timer 相关寄存器
7. `mret`

这就是“引导代码”与“内核主逻辑”的分界。

---

## 5. 内存管理知识（结合代码）

主要在 `kernel/mm/vm.c`。

## 5.1 你实现了哪些核心能力

1. 构建内核页表 `kvmmake`
2. 把 UART、PLIC、VIRTIO MMIO 设备映射进内核地址空间
3. 映射内核代码段和数据段
4. 映射 trampoline（用户态返回内核态关键跳板）

## 5.2 用户地址空间相关

1. `create_pagetable`
2. `mappages/walk/walkaddr`
3. `uvmalloc/uvmdealloc/uvmcopy`
4. `copyin/copyout/copyin_str`

可以把这些 API 归为两类：

1. 页表结构管理
2. 用户-内核数据搬运

后者在系统调用里非常高频。

---

## 6. Trap/系统调用知识（结合代码）

核心文件：`kernel/trap/trap.c`、`kernel/trap/syscall.c`、`kernel/trampoline.S`。

## 6.1 用户态陷入内核态

路径：

1. 用户态执行 `ecall` 或发生异常/中断
2. 进入 trampoline 的 `uservec`
3. 保存用户寄存器到 `trapframe`
4. 切换到内核页表和内核栈
5. 跳 `usertrap()`

## 6.2 系统调用处理

`usertrap()` 中对 `CAUSE_USER_ECALL`：

1. `epc += 4` 跳过 ecall 指令
2. 开中断
3. 调 `syscall()`

`syscall()` 再根据 `a7` 分发到：

1. `sys_read`
2. `sys_write`
3. `sys_exec`
4. `sys_fork`
5. `sys_wait`
6. 其他文件与进程相关调用

## 6.3 内核态 trap

`kerneltrap()` 处理内核态中断，关键点：

1. 保存并校验 `sepc/sstatus`
2. 通过 `devintr()` 判断设备中断/时钟中断
3. 时钟中断路径中触发 `wakeup_timer` 与条件 `yield`

---

## 7. 进程管理与调度知识（结合代码）

核心在 `kernel/proc/proc.c` 和 `kernel/proc/swtch.S`。

## 7.1 进程生命周期

典型状态流：

1. `UNUSED`
2. `USED`
3. `RUNNABLE`
4. `RUNNING`
5. `SLEEPING`（可往返）
6. `ZOMBIE`
7. 回收后回到 `UNUSED`

## 7.2 创建和切换

1. `allocproc()` 分配 `trapframe`、`pagetable`、`kstack`
2. 初始 `context.ra = forkret`
3. `scheduler()` 轮转扫描 `RUNNABLE`
4. `swtch(&cpu->context, &proc->context)`

`swtch.S` 只保存/恢复必要寄存器（ra/sp/s0-s11），符合 RISC-V 调用约定。

## 7.3 退出与回收

1. `exit()` 关闭文件、处理 cwd、改成 `ZOMBIE`、唤醒父进程
2. `wait()` 找 `ZOMBIE` 子进程并 `freeproc()`

这是父子进程资源回收闭环。

---

## 8. 文件系统知识（结合代码）

核心文件：`kernel/fs/fs.c`、`kernel/fs/file.c`、`kernel/fs/log.c`、`kernel/fs/bio.c`、`kernel/fs/virtio_disk.c`。

## 8.1 分层理解

1. 块设备层：virtio 磁盘中断和读写
2. 缓存层：`bread/bwrite/brelse`
3. 日志层：`begin_op/log_write/end_op/commit`
4. inode 层：`ialloc/ilock/iupdate/readi/writei`
5. 文件对象层：`filealloc/fileclose/fileread/filewrite`

## 8.2 为什么需要日志层

一个高层操作（比如创建文件）会改多个块：

1. inode 块
2. 目录数据块
3. bitmap 块

如果中途宕机会损坏一致性，所以要 WAL（先日志后落盘）。

---

## 9. 用户态与 shell（结合代码）

主要在 `user/shell.c`。

## 9.1 shell 的执行模型

1. 读取一行输入
2. 解析 argv
3. 内置命令直接执行
4. 外部命令走 `fork + exec + wait`

## 9.2 为什么外部命令普遍都能触发同类 bug

因为它们共享同一条内核路径：

1. shell `sys_exec`
2. 内核装载 ELF
3. 子进程退出 `sys_exit`
4. 父进程 `sys_wait`

所以 hello 只是最容易复现的代表，不是唯一问题点。

---

## 10. 关键 bug 深度复盘（本次修复）

## 10.1 现象

在 shell 中多次执行用户程序（如 hello），约数次后内核崩溃：

1. `kerneltrap`
2. `scause=0xc`（instruction page fault）
3. `sepc=0x0`

这说明 CPU 试图在地址 0 执行指令，通常是控制流（返回地址/上下文）被破坏。

## 10.2 错误是如何产生的

根因是“内核栈溢出导致上下文破坏”。

你这个内核给每个进程的内核栈是 4KB 量级，而系统调用里存在大栈对象：

1. `sys_write`/`sys_read` 里曾使用 `char kbuf[PGSIZE]`
2. `sys_exec` 里曾使用 `char kargv[MAXARG][MAXPATH]`

按默认参数估算：

$$
MAXARG \times MAXPATH = 32 \times 128 = 4096\ \text{bytes}
$$

再叠加函数栈帧、其他局部变量、调用深度，就非常容易越过栈边界。

一旦越界写，最危险的是覆盖：

1. 返回地址
2. 保存寄存器
3. 调度上下文区域

最终在某次 trap 返回或调度切换时跳到错误地址（例如 0x0），触发 panic。

## 10.3 为什么是“执行几次后”才崩

因为这类错误常不是每次都命中关键字节：

1. 覆盖位置与调用路径、输入长度、时序有关
2. 有时只是污染不敏感数据，暂时没挂
3. 累积到关键控制字段才爆炸

所以表现为“概率性 + 延迟触发”。

## 10.4 修复思路

修复原则：把大临时数据从内核栈拿走。

实际改动（`kernel/trap/syscall.c`）：

1. 引入 `KIO_CHUNK=256`，读写走小块分段缓冲
2. `sys_exec` 参数字符串改为 `kalloc` 动态分配
3. 增加统一清理分支，防止失败路径泄漏

额外加固（`kernel/trap/trap.c`）：

1. 时钟中断路径避免不安全的无条件让出
2. 更严格地在合适状态下 `yield`

## 10.5 修复后为什么有效

本质是把“固定大栈占用”变成“可控小栈 + 堆分配”：

1. 栈峰值显著下降，不再踩穿 4KB 内核栈
2. 生命周期清晰（`kalloc` 对应 `kfree`）
3. 失败路径可回收，避免新的泄漏问题

结果就是：重复执行 hello/ls/cat 这类用户程序时，调度上下文不再被随机破坏。

---

## 11. 面试可讲的 bug 故事模板

你可以按这 6 句话讲：

1. 我在 RISC-V 教学 OS 中遇到重复 exec 后随机 kerneltrap。
2. 表现为 instruction page fault 且 sepc=0，判断是控制流被污染。
3. 通过代码审计和复现实验，定位到系统调用内核栈超限。
4. `sys_exec` 曾在栈上放 4KB 级参数数组，叠加调用栈后越界。
5. 我把大对象改成小块分段和 kalloc 动态缓冲，并完善回收路径。
6. 修复后高频重复执行用户程序稳定通过，崩溃消失。

---

## 12. 建议的复习顺序（1 小时版）

1. 先看 `kernel/main.c`，理解初始化顺序
2. 看 `kernel/boot/entry.S + kernel/boot/start.c`，回忆特权级切换
3. 看 `kernel/trap/trap.c`，理解用户态/内核态 trap 双路径
4. 看 `kernel/proc/proc.c + kernel/proc/swtch.S`，串起进程调度
5. 看 `kernel/trap/syscall.c`，把 read/write/exec 三个重点过一遍
6. 最后看 `kernel/fs/file.c + kernel/fs/log.c + kernel/fs/fs.c`，补文件系统层次

---

## 13. 小结

这个项目最大的价值不只是“把功能做出来”，而是你已经走到了“能定位和修复内核级稳定性 bug”的阶段。  
对保研/面试来说，最加分的是这种能力：

1. 发现可复现问题
2. 还原运行链路
3. 给出正确抽象（栈溢出而非某个用户程序问题）
4. 实施低风险修复并验证

这就是从课程作业向工程能力跨越的关键点。
