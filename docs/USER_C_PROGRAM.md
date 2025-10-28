# 用户 C 程序支持实现

## 概述

成功实现了最小用户空间，支持运行 C 程序而不是手写机器码。用户现在可以编写 C 代码，编译后在内核中运行。

## 实现方案

### 1. 用户程序构建链

#### 文件结构
```
user/
├── init.c          # 用户主程序
├── syscall.h       # 系统调用接口
├── user.ld         # 用户程序链接脚本
├── init.elf        # 编译后的 ELF 文件
├── init.bin        # 二进制文件
└── initcode.h      # 生成的 C 头文件
```

#### 构建流程
1. **C 源码编译**：`user/init.c` → `user/init.o`
2. **链接**：`user/init.o` → `user/init.elf` (使用 `user/user.ld`)
3. **提取二进制**：`user/init.elf` → `user/init.bin`
4. **生成头文件**：`user/init.bin` → `user/initcode.h`
5. **内核编译**：包含 `initcode.h` 并加载用户程序

### 2. 用户程序接口

#### 系统调用封装 (`user/syscall.h`)
```c
// 系统调用号
#define SYS_EXIT    1
#define SYS_GETPID  2
#define SYS_WRITE   6

// 通用系统调用函数
static inline long do_syscall(long n, long a0, long a1, long a2);

// 具体系统调用包装
static inline int sys_getpid(void);
static inline void sys_exit(int status);
static inline int sys_write(int fd, const void *buf, int n);
```

#### 用户程序示例 (`user/init.c`)
```c
void _start(void) {
    int pid = sys_getpid();
    
    // 多次系统调用验证
    for (int i = 0; i < 4; i++) {
        sys_getpid();
    }
    
    sys_exit(0);
}
```

### 3. 内核修改

#### 用户程序加载 (`kernel/proc/proc.c`)
```c
// 引入生成的用户程序镜像
#include "/home/thebreeze/riscv-os/user/initcode.h"

void userinit(void) {
    // 分配用户内存
    p->sz = uvmalloc(p->pagetable, 0, PGROUNDUP(initcode_size));
    
    // 复制用户程序到用户内存
    uint64 pa = PTE2PA(*pte);
    memmove((void*)pa, initcode, initcode_size);
    
    // 设置用户程序入口点
    p->trapframe->epc = 0;
    p->trapframe->sp = PGSIZE;
}
```

#### 系统调用修复 (`kernel/trap/syscall.c`)
```c
uint64 sys_exit(void) {
    // 关闭中断，然后调用exit
    intr_off();
    exit(status);
}
```

### 4. 构建系统

#### Makefile 规则
```makefile
# 用户程序构建
$(USER_ELF): $(USER_SRCS) $(USER_INCS)
	$(USER_CC) $(USER_CFLAGS) -c user/init.c -o user/init.o
	$(LD) $(USER_LDFLAGS) -o $(USER_ELF) user/init.o

$(USER_BIN): $(USER_ELF)
	$(USER_OBJCOPY) -O binary $(USER_ELF) $(USER_BIN)

$(USER_HDR): $(USER_BIN)
	@hexdump -v -e '1/1 " 0x%02x,"' $(USER_BIN) | sed 's/$$/ /' >> $(USER_HDR)

# 内核依赖用户程序
kernel.elf: $(OBJS) kernel/kernel.ld $(USER_HDR)
```

## 运行结果

### 成功输出
```
S
B
S
start
My RISC-V OS Starting...
System initialization complete!
Entering scheduler...
第一个进程初始化，准备切换到用户态...
[RET] usertrapret: pid=1, switching to user mode...
[RET] satp=87f98
[TRAP] ============ usertrap called! ============
[TRAP] sepc=8, sstatus=20, scause=8
[TRAP] epc=8
[TRAP] usertrap: pid=1, scause=8, epc=8
[U->K] sys_getpid() pid=1
[RET] usertrapret: pid=1, switching to user mode...
[RET] satp=87f98
[TRAP] ============ usertrap called! ============
[TRAP] sepc=16, sstatus=20, scause=8
[TRAP] epc=16
[TRAP] usertrap: pid=1, scause=8, epc=16
[U->K] sys_getpid() pid=1
[RET] usertrapret: pid=1, switching to user mode...
[RET] satp=87f98
[TRAP] ============ usertrap called! ============
[TRAP] sepc=16, sstatus=20, scause=8
[TRAP] epc=16
[TRAP] usertrap: pid=1, scause=8, epc=16
[U->K] sys_getpid() pid=1
[RET] usertrapret: pid=1, switching to user mode...
[RET] satp=87f98
[TRAP] ============ usertrap called! ============
[TRAP] sepc=16, sstatus=20, scause=8
[TRAP] epc=16
[TRAP] usertrap: pid=1, scause=8, epc=16
[U->K] sys_getpid() pid=1
[RET] usertrapret: pid=1, switching to user mode...
[RET] satp=87f98
[TRAP] ============ usertrap called! ============
[TRAP] sepc=16, sstatus=20, scause=8
[TRAP] epc=16
[TRAP] usertrap: pid=1, scause=8, epc=16
[U->K] sys_getpid() pid=1
[RET] usertrapret: pid=1, switching to user mode...
[RET] satp=87f98
[TRAP] ============ usertrap called! ============
[TRAP] sepc=22, sstatus=20, scause=8
[TRAP] epc=22
[TRAP] usertrap: pid=1, scause=8, epc=22
[U->K] sys_exit(0) pid=1
```

## 关键特性

### ✅ 已实现
1. **C 程序编译**：用户可以用 C 编写程序
2. **系统调用接口**：提供 `sys_getpid()`, `sys_exit()` 等
3. **自动构建**：Makefile 自动处理编译链
4. **用户态执行**：程序在用户态正常运行
5. **系统调用**：用户态与内核态正确切换
6. **程序退出**：`sys_exit()` 正常工作

### 🔧 可扩展
1. **更多系统调用**：`sys_write()`, `sys_read()` 等
2. **用户库**：标准 C 库函数
3. **动态加载**：ELF 文件加载器
4. **多进程**：fork, exec 等

## 使用方法

### 编写用户程序
1. 编辑 `user/init.c`
2. 添加需要的系统调用到 `user/syscall.h`
3. 在内核中实现对应的系统调用

### 编译和运行
```bash
make clean && make
make run
```

## 总结

成功实现了最小用户空间，支持运行 C 程序。用户现在可以：
- 编写 C 代码而不是手写机器码
- 使用系统调用与内核交互
- 享受完整的构建系统支持

这为后续实现更复杂的用户程序（如 shell、文件系统等）奠定了基础。
