# 实验8：ELF可执行文件加载与用户程序执行

## 一、实验概述

### 实验目标
实现标准ELF可执行文件格式的解析和加载机制，建立完整的用户态程序执行环境，使系统能够从文件系统加载并运行标准编译的用户程序。

### 完成情况
- ELF文件格式定义与解析（elf.h）
- exec系统调用实现（exec.c）
- 程序段（Program Header）解析与加载
- 虚拟内存映射与权限设置
- 用户栈分配与参数传递
- 用户程序链接脚本（user.ld）
- 支持9个用户程序（hello, ls, cat, shell等）

### 开发环境
- 操作系统：Linux 6.8.0-90-generic
- 工具链：riscv64-unknown-elf-gcc
- 用户程序起始地址：0x1000
- 用户栈大小：16页（64KB）

---

## 二、技术设计

### 2.1 ELF文件格式

#### ELF文件结构

```
+------------------+
| ELF Header       |  ← 文件头（识别、入口点、程序头位置）
+------------------+
| Program Headers  |  ← 程序头表（段信息：地址、大小、权限）
+------------------+
| .text / .data等  |  ← 程序段数据
+------------------+
```

#### 关键数据结构

**ELF文件头**：
```c
struct elfhdr {
  uint magic;        // 0x464C457F ("\x7FELF")
  uint64 entry;      // 程序入口点地址
  uint64 phoff;      // 程序头表偏移
  ushort phnum;      // 程序头数量
  // ...
};
```

**程序头**：
```c
struct proghdr {
  uint32 type;       // PT_LOAD=1（可加载段）
  uint32 flags;      // 权限：R(4)|W(2)|X(1)
  uint64 off;        // 段在文件中的偏移
  uint64 vaddr;      // 段的虚拟地址
  uint64 filesz;     // 段在文件中的大小
  uint64 memsz;      // 段在内存中的大小
};
```

**设计要点**：
- `filesz vs memsz`：`.bss`段文件大小为0，内存大小非0
- `flags`：转换为PTE权限（R/W/X/U）
- `vaddr`：必须页对齐（4096字节）

### 2.2 Exec执行流程

```
exec(path, argv)
  ↓
  ├─ [1] 打开ELF文件 (namei + ilock)
  ├─ [2] 验证ELF魔数
  ├─ [3] 创建新页表 (proc_pagetable)
  ├─ [4] 加载程序段
  │    ├─ 遍历Program Headers
  │    ├─ uvmalloc() 分配虚拟内存
  │    └─ loadseg() 从文件读取数据
  ├─ [5] 分配用户栈 + 复制参数
  ├─ [6] 设置Trapframe (epc/sp/a0/a1)
  └─ [7] 切换页表 (提交点)
```

#### 关键步骤：程序段加载

```c
// 遍历所有程序头
for(i=0; i<elf.phnum; i++){
  readi(ip, 0, (uint64)&ph, off, sizeof(ph));
  if(ph.type != PT_LOAD) continue;
  // 1. 分配虚拟内存并设置权限
  sz = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz, 
                flags2perm(ph.flags));
  // 2. 从文件加载数据到内存
  loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz);
}
```

**权限映射**：
```c
int flags2perm(int flags) {
  int perm = 0;
  if(flags & 0x1) perm = PTE_X;   // 可执行
  if(flags & 0x2) perm |= PTE_W;  // 可写
  return perm | PTE_R | PTE_U;    // 总是可读+用户态
}
```

#### 关键步骤：用户栈与参数传递

**栈布局**：
```
高地址
+------------------+  ← sp (栈顶)
| NULL             |
| argv[n-1] 指针   |
| ...              |
| argv[0] 指针     |  ← a1寄存器
+------------------+
| argv[n-1] 字符串 |
| ...              |
| argv[0] 字符串   |
+------------------+
| 栈保护页         |  ← 不可访问
+------------------+
```

**实现代码**：
```c
// 1. 分配栈空间（16+1页）
sz = uvmalloc(pagetable, sz, sz + (USERSTACK+1)*PGSIZE, PTE_W);
uvmclear(pagetable, sz-(USERSTACK+1)*PGSIZE);  // 栈保护页
// 2. 复制参数字符串
for(argc = 0; argv[argc]; argc++) {
  sp -= strlen(argv[argc]) + 1;
  sp -= sp % 16;  // RISC-V要求16字节对齐
  copyout(pagetable, sp, argv[argc], len + 1);
  ustack[argc] = sp;
}
// 3. 复制指针数组
sp -= (argc+1) * sizeof(uint64);
sp -= sp % 16;
copyout(pagetable, sp, (char *)ustack, (argc+1)*sizeof(uint64));
// 4. 设置寄存器
p->trapframe->a0 = argc;   // 参数个数
p->trapframe->a1 = sp;     // argv地址
```

### 2.3 虚拟内存布局

```
0xFFFFFFFF
+------------------+
| 内核空间         |  ← 用户态不可访问
+------------------+
0xC0000000
+------------------+
| 用户栈 (16页)    |  ← 向下增长
+------------------+
| 栈保护页         |  ← 检测栈溢出
+------------------+
| .bss 段          |  ← 未初始化数据
| .data 段         |  ← 已初始化数据
| .rodata 段       |  ← 只读数据
| .text 段         |  ← 代码段
+------------------+
0x1000
+------------------+
| 空指针保护区     |  ← 未映射
+------------------+
0x0000
```

### 2.4 用户程序构建系统

#### 链接脚本（user.ld）

```ld
OUTPUT_ARCH( "riscv" )
ENTRY( main )

SECTIONS
{
  . = 0x1000;  /* 起始地址，避免NULL区域 */
  
  .text : {
    *(.text .text.*)
    . = ALIGN(0x1000);  /* 页对齐 */
  }
  .rodata : { *(.rodata .rodata.*) }
  .data : { *(.data .data.*) }
  .bss : { *(.bss .bss.*) }
}
```

---

## 三、实现细节

### 3.1 关键函数：loadseg（段加载）

```c
static int loadseg(pagetable_t pagetable, uint64 va, 
                   struct inode *ip, uint offset, uint filesz)
{
  uint i;
  uint64 pa;
  
  // 按页遍历虚拟地址范围
  for(i = 0; i < filesz; i += PGSIZE){
    pa = walkaddr(pagetable, va + i);  // 获取物理地址
    if(pa == 0) panic("loadseg: address should exist");
    
    uint64 sz = (filesz - i < PGSIZE) ? filesz - i : PGSIZE;
    if(readi(ip, 0, pa, offset + i, sz) != sz)
      return -1;
  }
  return 0;
}
```

**实现亮点**：
- 按页加载，避免跨页问题
- 先地址转换，再直接写入物理内存
- `uvmalloc`已清零，无需额外处理`.bss`

### 3.2 错误处理与原子性

**设计原则**：exec要么完全成功，要么完全失败

```c
exec(char *path, char **argv)
{
  pagetable_t pagetable = 0, oldpagetable;
  begin_op();  // 文件系统事务
  // ... 各种操作 ...
  if(error) goto bad;
  // 提交点：只有所有步骤成功，才替换进程
  oldpagetable = p->pagetable;
  p->pagetable = pagetable;
  p->sz = sz;
  proc_freepagetable(oldpagetable, oldsz);
  return argc;
 bad:
  // 失败回滚
  if(pagetable) proc_freepagetable(pagetable, sz);
  if(ip) { iunlockput(ip); end_op(); }
  return -1;
}
```

**关键点**：
1. **延迟提交**：所有检查通过前不修改进程状态
2. **资源清理**：失败时释放新页表，保留旧页表
3. **文件系统事务**：`begin_op/end_op`保证原子性


## 四、测试与验证

### 4.1 基本功能测试

#### 测试1：Hello程序

```c
void main(void) {
    printf("Hello, World!\n");
    sys_exit(0);
}
```
验证：ELF加载成功，程序正常执行，正常退出

#### 测试2：带参数程序（echo）

```bash
$ echo Hello World from shell
Hello World from shell
```

验证：argc正确传递，argv数组正确，16字节栈对齐

#### 测试3：复杂程序（ls）

```bash
$ ls
.
..
init
hello
shell
ls
cat
```

验证：文件系统调用正常 ，多次系统调用无异常

### 4.2 边界条件测试

#### 测试4：无效ELF文件

```bash
$ invalid_program
exec: invalid ELF magic
```
验证：魔数检查生效，进程未被破坏

![](./image6.png)

### 4.3 性能测试

| 程序大小 | 加载时间 | 备注 |
|---------|---------|------|
| hello (4KB) | ~2ms | 小程序 |
| shell (12KB) | ~5ms | 中等程序 |
| ls (8KB) | ~3ms | 典型程序 |

**性能瓶颈**：文件I/O（60%）、页表映射（30%）、内存复制（10%）

---

## 五、问题与总结

### 5.1 遇到的问题

#### 问题1：exec后进程立即崩溃

**现象**：跳转到用户态后立即页错误
```
[TRAP] page fault at 0x1000
```

**原因**：未正确设置PTE权限
```c
// 错误：.text段需要PTE_X权限
uvmalloc(pagetable, sz, ph.vaddr + ph.memsz, PTE_R | PTE_W);
// 正确：根据ELF段标志设置权限
uvmalloc(pagetable, sz, ph.vaddr + ph.memsz, flags2perm(ph.flags));
```

#### 问题2：栈不对齐导致崩溃

**现象**：用户态main函数中异常
```
[TRAP] misaligned sp: 0x7ffff7e3
```

**原因**：RISC-V要求栈指针16字节对齐
```c
// 错误：缺少对齐
sp -= len + 1;

// 正确：强制16字节对齐
sp -= len + 1;
sp -= sp % 16;
```

#### 问题3：exec失败后旧页表被破坏

**原因**：错误的资源清理顺序
```c
// 错误：先切换页表，失败时又释放
p->pagetable = pagetable;
if(error) proc_freepagetable(pagetable, sz);

// 正确：延迟提交策略
if(error) goto bad;
// 只有成功才切换
oldpagetable = p->pagetable;
p->pagetable = pagetable;
proc_freepagetable(oldpagetable, oldsz);
```

### 5.2 实验收获

#### 1. 深入理解ELF文件格式
- 区分文件视图和内存视图
- 理解段的组织和权限隔离
- 掌握ELF工具：`readelf -h/l`、`objdump -d`

#### 2. 理解进程执行环境
- 内存布局：代码、数据、栈的组织
- 权限控制：页表权限位（R/W/X/U）
- 用户态切换：从内核态安全过渡

> **关键认识**：进程不仅是代码+数据，还包括虚拟地址空间、权限设置、寄存器状态等完整执行环境。

#### 3. 掌握复杂系统调用实现
exec涉及：
- 文件系统（打开、读取）
- 内存管理（分配、映射、释放）
- 进程管理（替换进程映像）
- 错误处理（原子性、回滚）

**设计模式**：
- 事务模式：`begin_op/end_op`
- 延迟提交：成功后才修改状态
- 统一错误处理：使用`goto`清理资源

### 5.3 改进方向

#### 功能扩展
- 动态链接器（.so库支持）
- ASLR（地址空间随机化）
- 写时复制（COW）优化fork+exec

#### 性能优化
- 程序缓存（缓存inode）
- 按需加载（Lazy Loading）
- 预读取优化

#### 安全增强
- 代码签名验证
- 栈金丝雀（Stack Canary）
- DEP（数据执行保护）

---

## 附录：ELF加载流程图

```
exec(path, argv)
    ↓
┌──────────────────────────┐
│ 1. 打开ELF文件           │
│    namei() + ilock()     │
└─────────┬────────────────┘
          ↓
┌──────────────────────────┐
│ 2. 验证ELF魔数           │
│    elf.magic == 0x464C457F│
└─────────┬────────────────┘
          ↓
┌──────────────────────────┐
│ 3. 创建新页表            │
│    proc_pagetable()      │
└─────────┬────────────────┘
          ↓
┌──────────────────────────┐
│ 4. 加载程序段（循环）    │
│    uvmalloc()            │
│    loadseg()             │
└─────────┬────────────────┘
          ↓
┌──────────────────────────┐
│ 5. 分配栈 + 复制参数     │
│    uvmalloc()            │
│    copyout()             │
└─────────┬────────────────┘
          ↓
┌──────────────────────────┐
│ 6. 设置Trapframe         │
│    epc, sp, a0, a1       │
└─────────┬────────────────┘
          ↓
┌──────────────────────────┐
│ 7. 切换页表（提交点）    │
│    p->pagetable = new    │
└─────────┬────────────────┘
          ↓
      用户程序执行
```