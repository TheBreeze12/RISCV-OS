# RISC-V操作系统 Makefile
# 标准构建配置文件

# 工具链配置
CROSS_COMPILE = riscv64-unknown-elf-
CC = $(CROSS_COMPILE)gcc
AS = $(CROSS_COMPILE)as
LD = $(CROSS_COMPILE)ld
OBJDUMP = $(CROSS_COMPILE)objdump
OBJCOPY = $(CROSS_COMPILE)objcopy
NM = $(CROSS_COMPILE)nm

# 编译选项
CFLAGS = -Wall -Werror -O -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -MD -mcmodel=medany -ffreestanding -fno-common -nostdlib
CFLAGS += -mno-relax -fno-stack-protector -fno-pie -no-pie

ASFLAGS = -gdwarf-2

# 链接选项
LDFLAGS = -z max-page-size=4096

# 目标文件
OBJS = \
kernel/trampoline.o \
kernel/boot/entry.o \
kernel/boot/start.o \
kernel/main.o \
kernel/utils/printf.o \
kernel/utils/uart.o \
kernel/utils/console.o \
kernel/mm/kalloc.o \
kernel/utils/string.o \
kernel/mm/vm.o \
kernel/trap/trap.o \
kernel/trap/kernelvec.o \
kernel/trap/syscall.o \
kernel/proc/proc.o \
kernel/proc/swtch.o \
kernel/proc/proc_test.o \
kernel/fs/file.o \
kernel/fs/namei.o \
kernel/proc/exec.o

# 用户程序构建（最小C到二进制再转头文件）
USER_CC = $(CROSS_COMPILE)gcc
USER_OBJCOPY = $(CROSS_COMPILE)objcopy
USER_OBJDUMP = $(CROSS_COMPILE)objdump
USER_CFLAGS = -march=rv64gc -mabi=lp64 -Wall -O2 -fno-builtin -nostdlib -ffreestanding
USER_LDFLAGS = -T user/user.ld -nostdlib -static -n --gc-sections

# 用户程序列表（每个程序一个.c文件，生成对应的.elf文件）
USER_PROGRAMS = init hello
USER_COMMON_SRCS = user/utils/printf.c user/utils/scanf.c user/utils/shell.c
USER_COMMON_OBJS = user/utils/printf.o user/utils/scanf.o user/utils/shell.o
USER_INCS = user/utils/syscall.h user/user.ld

# 生成所有ELF文件列表
USER_ELFS = $(addprefix user/,$(addsuffix .elf,$(USER_PROGRAMS)))

# 主程序（用于生成initcode.h）
USER_ELF = user/init.elf
USER_HDR = user/initcode.h

# 默认目标
all: $(USER_HDR)  kernel.elf

# 编译汇编文件
kernel/boot/entry.o: kernel/boot/entry.S
	$(CC) $(CFLAGS) -c $< -o $@

kernel/trampoline.o: kernel/trampoline.S
	$(CC) $(CFLAGS) -c $< -o $@

kernel/trap/kernelvec.o: kernel/trap/kernelvec.S
	$(CC) $(CFLAGS) -c $< -o $@

kernel/proc/swtch.o: kernel/proc/swtch.S
	$(CC) $(CFLAGS) -c $< -o $@

# 编译C文件
kernel/%.o: kernel/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# proc.c 依赖用户程序头文件
kernel/proc/proc.o: kernel/proc/proc.c $(USER_HDR)
	$(CC) $(CFLAGS) -c $< -o $@

# 链接生成内核镜像
kernel.elf: $(OBJS) kernel/kernel.ld $(USER_HDR)
	$(LD) $(LDFLAGS) -T kernel/kernel.ld -o $@ $(OBJS)

# 生成反汇编文件用于调试
kernel.asm: kernel.elf
	$(OBJDUMP) -S $< > $@

# 生成符号表
kernel.sym: kernel.elf
	$(NM) $< | sort > $@

# 验证内存布局
check-layout: kernel.elf
	@echo "=== 内存段布局 ==="
	$(OBJDUMP) -h $<
	@echo ""
	@echo "=== 关键符号地址 ==="
	$(NM) $< | grep -E "(start|end|text|bss|etext|edata)"


# 调试模式运行
debug: kernel.elf
	@echo "=== 启动QEMU调试模式 ==="
	@echo "QEMU将在端口1234等待GDB连接..."
	@echo "请在另一个终端运行: make gdb"
	@echo "或手动运行: gdb-multiarch -ex 'target remote :1234' -ex 'symbol-file kernel.elf' kernel.elf"
	qemu-system-riscv64 -machine virt -bios none -kernel $< -m 128M -smp 1 -nographic -s -S

# GDB调试连接
gdb: kernel.elf
	@echo "=== 连接到QEMU调试会话 ==="
	gdb-multiarch -ex "target remote :1234" -ex "symbol-file kernel.elf" kernel.elf

# 一键调试（同时启动QEMU和GDB）
debug-all: kernel.elf
	@echo "=== 启动完整调试环境 ==="
	@echo "将在后台启动QEMU，然后启动GDB..."
	@(qemu-system-riscv64 -machine virt -bios none -kernel $< -m 128M -smp 1 -nographic -s -S &) && sleep 2 && gdb-multiarch -ex "target remote :1234" -ex "symbol-file kernel.elf" kernel.elf


clean-all:
	rm -f kernel/*/*.o kernel/*/*.d kernel/*.o kernel/*.d kernel.elf kernel.asm kernel.sym \
		user/*.o user/*.elf $(USER_HDR) 

clean:
	rm -f kernel/*/*.o kernel/*/*.d kernel/*.o kernel/*.d 


# 在QEMU中运行（需要安装qemu-system-riscv64）
run: kernel.elf
	qemu-system-riscv64 -machine virt -bios none -kernel $< -m 128M -smp 1 -nographic

qemu: run clean

# 完整构建和验证
test: clean all check-layout kernel.asm kernel.sym 
	@echo ""
	@echo "=== 构建完成！==="
	@echo "运行: make run"
	@echo "调试: make debug (然后在另一终端运行 gdb-multiarch kernel.elf)"

.PHONY: all clean run debug debug-all gdb test check-layout user-programs fsimg

# 包含依赖文件
-include kernel/*/*.d 


# 为每个用户程序构建ELF文件
# init程序需要链接shell等库
user/init.elf: user/init.c $(USER_COMMON_OBJS) $(USER_INCS)
	$(USER_CC) $(USER_CFLAGS) -c user/init.c -o user/init.o
	$(LD) $(USER_LDFLAGS) -o $@ user/init.o $(USER_COMMON_OBJS)




$(USER_HDR): $(USER_ELF)
	@echo "// generated from user/init.elf" > $(USER_HDR)
	@echo "#ifndef INITCODE_H" >> $(USER_HDR)
	@echo "#define INITCODE_H" >> $(USER_HDR)
	@echo "static const unsigned char initcode[] = {" >> $(USER_HDR)
	@hexdump -v -e '1/1 " 0x%02x,"' $(USER_ELF) | sed 's/$$/ /' >> $(USER_HDR)
	@echo "" >> $(USER_HDR)
	@echo "};" >> $(USER_HDR)
	@echo "static const unsigned int initcode_size = sizeof(initcode);" >> $(USER_HDR)
	@echo "#endif" >> $(USER_HDR)

