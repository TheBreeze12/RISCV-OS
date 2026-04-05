# sys_exec + exec 深度讲解（结合本项目代码）

本文只讲一件事：一个用户进程调用 exec 时，内核到底做了什么。

覆盖代码：

- kernel/trap/syscall.c 中的 sys_exec
- kernel/proc/exec.c 中的 exec 与 loadseg
- kernel/trap/trap.c 中 usertrap/usertrapret 的返回衔接
- user/shell.c 中外部命令执行入口

---

## 1. 先说结论：exec 的本质

exec 不是创建新进程，而是把当前进程的用户态镜像整体替换成新程序。

不变的东西：

- pid 不变
- 内核进程结构仍是同一个 struct proc
- 打开文件表 ofile 继续保留（本实现没有 FD_CLOEXEC）

变化的东西：

- 用户页表被新页表替换
- 用户内存内容（代码、数据、栈）被重建
- 用户态 PC 跳到 ELF entry
- 用户态 a0/a1 被设置成 main(argc, argv) 语义

---

## 2. 用户态入口：shell 如何触发 exec

在 user/shell.c 的 execute_external：

1. 先 fork。
2. 子进程构造 new_argv。
3. 调用 sys_exec(fullpath, new_argv)。
4. 如果 exec 成功，按语义不会返回到原代码。

这个模式是典型的 fork + exec。

---

## 3. syscall 层：sys_exec 做了什么

位置：kernel/trap/syscall.c

sys_exec 从 trapframe 读取参数：

- a0: path_addr（用户空间字符串地址）
- a1: argv_addr（用户空间 char* 数组地址）

核心步骤如下。

### 3.1 从用户空间拷贝 path

调用 copyin_str(p->pagetable, path, path_addr, MAXPATH)。

失败直接返回 -1，避免后续访问非法地址。

### 3.2 从用户空间拷贝 argv 指针数组

循环 i=0..MAXARG-1：

1. 先 copyin 一个 uint64 arg_ptr。
2. arg_ptr 为 0 表示参数结束。
3. 对每个参数，kalloc 一页临时内核缓冲。
4. 再 copyin_str 把用户字符串拷入该缓冲。
5. 把该内核字符串地址放入 argv[argc]。

这里非常关键：

- 使用了动态分配 arg_pages，而不是大栈数组。
- 这是为避免内核栈溢出（此前你们项目出现过这类问题）。

### 3.3 调用 exec(path, argv)

调用后无论成功失败，都会释放 arg_pages，避免泄漏。

- 成功：exec 返回 argc（随后写回 trapframe->a0）
- 失败：返回 -1

### 3.4 错误清理路径

bad 标签会遍历 arg_pages 全量释放，保证异常路径也不泄漏。

---

## 4. 内核主逻辑：exec 逐步拆解

位置：kernel/proc/exec.c

函数签名：

- int exec(char *path, char **argv)

可以把它看成 6 个阶段。

## 阶段 A：开启 FS 事务并读取 ELF

1. begin_op()
2. namei(path) 找到 inode
3. ilock(ip)
4. readi 读 elfhdr
5. 校验 elf.magic == ELF_MAGIC

失败就走 bad。

为什么要 begin_op/end_op：

- namei/readi 走文件系统路径，需要处于日志事务边界内。

## 阶段 B：创建新页表并装载程序段

1. proc_pagetable(p) 创建新用户页表。
2. 遍历程序头表 phdr：
   - 只处理 PT_LOAD
   - 校验 memsz/filesz/vaddr 对齐与溢出
   - uvmalloc 按段扩展用户地址空间
   - loadseg 把 ELF 文件内容装进对应 VA

loadseg 的本质：

- 对每页 walkaddr 找到物理页
- 从 inode 读文件偏移到该页

完成后 iunlockput(ip) + end_op()。

## 阶段 C：构造用户栈（含 guard page）

1. sz 向上页对齐。
2. 再分配 USERSTACK+1 页，权限 PTE_W。
3. 最低那页用 uvmclear 设为不可访问，作为 guard page。
4. sp 指向栈顶，stackbase 为可用栈底。

你的配置中 USERSTACK=1，所以可用用户栈只有 1 页。

## 阶段 D：把 argv 字符串和 argv 指针数组压入新栈

### D1 压字符串

对每个 argv[argc]：

1. 计算 len，限制 len < MAXPATH。
2. sp -= len+1，再做 16 字节对齐。
3. 检查 sp 不得越过 stackbase。
4. copyout 到新页表用户栈。
5. 记录字符串用户地址到 ustack[argc]。

### D2 压 argv 指针数组

1. ustack[argc]=0（NULL 结尾）
2. 再把整段 ustack 拷到用户栈
3. p->trapframe->a1 = sp（用户态 argv 指针）

这里配合返回值 a0=argc，正好满足 C 入口约定 main(argc, argv)。

## 阶段 E：提交新镜像（原子切换）

1. 校验 elf.entry 不能为 0。
2. 先写 trapframe：
   - epc = elf.entry
   - sp = 新用户栈顶
3. oldpagetable = p->pagetable
4. p->pagetable = 新页表
5. p->sz = 新地址空间大小
6. proc_freepagetable(oldpagetable, oldsz) 释放旧用户空间

这一步是 exec 的核心提交点。

## 阶段 F：返回 argc，供 syscall 层写入 a0

exec 返回 argc。

syscall 分发函数会把返回值写入 p->trapframe->a0。

---

## 5. 返回用户态：为何会跳到新程序入口

位置：kernel/trap/trap.c

当用户态发起 ecall：

1. usertrap 保存原 epc，并把 epc += 4（跳过 ecall 指令）。
2. 调用 syscall()。

对于 exec 场景，syscall 执行期间 exec 已把 trapframe 改成：

- epc = 新程序入口
- sp = 新栈
- a1 = argv 指针
- a0 = syscall 返回值 argc

最后 usertrapret：

1. w_sepc(p->trapframe->epc)
2. 切换 satp 到 p->pagetable
3. trampoline 返回用户态

因此 CPU 回到用户态时，看到的是全新的程序镜像与寄存器现场。

---

## 6. 这份实现的关键安全点

1. 用户指针全部通过 copyin/copyin_str/copyout，避免直接解引用用户地址。
2. argv 临时缓冲使用 kalloc，避免内核栈被大数组压爆。
3. 各阶段有边界检查：MAXARG、MAXPATH、栈越界、ELF 合法性。
4. 提交采用“先准备新页表，最后一次性切换”的策略，失败不会破坏旧镜像。

---

## 7. 你最需要关注的易错点

1. begin_op/end_op 配对：错误路径漏掉 end_op 会导致日志系统卡死。
2. bad 清理路径：任何新增资源都要加入统一释放。
3. sp 16 字节对齐：RISC-V ABI 要求，不对齐会导致隐蔽问题。
4. USERSTACK 很小（1 页）：参数过多或过长容易触发栈越界 bad。
5. 返回语义：exec 成功后不会回到旧用户代码；若你看到返回，多半是失败路径。

---

## 8. 从调试角度理解这两层

调试 exec 类问题时建议先分层定位：

1. sys_exec 层：参数搬运是否正确（path/argv 是否完整，是否提前失败）。
2. ELF 装载层：header/phdr 合法性与 loadseg 是否成功。
3. 栈构造层：sp 是否下穿 stackbase，a1 是否有效。
4. trap 返回层：epc/satp 是否与新程序一致。

只要这四层日志打通，exec 问题通常都能快速定位。

---

## 9. 一句话总结

sys_exec 负责把用户参数安全搬到内核，exec 负责构造并提交新用户镜像；两者通过 trapframe 对接，最终让同一个进程在下一次返回用户态时，从新程序入口以 main(argc, argv) 语义开始执行。
