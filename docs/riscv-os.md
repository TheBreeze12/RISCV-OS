# 一、entry.s
### 关于stack0的定义和用途
1. 系统启动时的临时栈，用于：多核系统初始化。Machine模式到Supervisor模式的过渡。早期内核初始化：在内存管理和进程调度系统建立之前使用，需要一个临时的栈来执行内核的初始化代码
2. 位置：kernel/start.c 中定义的全局数组。生命周期：只在系统启动初期使用。分配方式：静态分配，编译时确定大小
### 为什么第一条指令是设置栈指针？
QEMU启动时，栈指针（sp）是未定义的，必须手动设置。为后去分配临时栈空间作准备
### 为什么要清零BSS段？
### 如何从汇编跳转到C函数？
+ call start  //jal ra, start
+ entry->start->main->开始调度进程

### BSS相关知识点
+ 是程序内存中用于存储未初始化的全局变量和静态变量的段。
+ 编译时：BSS段不占用可执行文件的磁盘空间
+ 运行时：BSS段占用内存空间，且被自动初始化为0

# 二、kernel.ld
### ENTRY(_entry) 的作用是什么？
指定程序入口点：告诉链接器程序的第一条执行指令位置2. 设置ELF文件头：在生成的ELF可执行文件中设置入口地址3. 引导加载：当QEMU加载内核时，会从这个地址开始执行
### 为什么代码段要放在0x80000000？
RISC-V架构约定：
0x80000000 是RISC-V平台常用的内核加载地址
这个地址位于物理内存的高位，避免与低地址的设备寄存器冲突

### etext 、 edata 、 end 符号有什么用途？
标记结束位置

# 编译内核
make

# 运行内核
make run

# 调试内核
make debug

# 完整测试
make test

# 清理构建文件
make clean

倒入设备树
qemu-system-riscv64 -machine virt -machine dumpdtb=virt_new.dtb -nographic
反编译设备树查看源码
dtc -I dtb -O dts virt_new.dtb > virt.dts