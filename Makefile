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
kernel/utils/spinlock.o \
kernel/utils/sleeplock.o \
kernel/mm/vm.o \
kernel/trap/trap.o \
kernel/trap/kernelvec.o \
kernel/trap/syscall.o \
kernel/trap/plic.o \
kernel/proc/proc.o \
kernel/proc/swtch.o \
kernel/proc/proc_test.o \
kernel/fs/file.o \
kernel/proc/exec.o \
kernel/fs/bio.o \
kernel/fs/virtio_disk.o \
kernel/fs/fs.o \
kernel/fs/log.o 



# 用户程序构建配置
U = user
K = kernel

USER_CC = $(CROSS_COMPILE)gcc
USER_CFLAGS = -march=rv64gc -mabi=lp64 -Wall -O2 -fno-builtin -nostdlib -ffreestanding
USER_LDFLAGS = -T $(U)/user.ld -nostdlib -static -n --gc-sections

# 用户程序公共库
USER_COMMON_OBJS = $(U)/utils/printf.o $(U)/utils/scanf.o $(U)/utils/shell.o
USER_INCS = $(U)/utils/syscall.h $(U)/user.ld

# 用户程序库（类似 xv6 的 ULIB）
ULIB = $(U)/utils/printf.o $(U)/utils/scanf.o $(U)/utils/shell.o

# 主程序（用于生成initcode.h，现在使用 _init）
USER_ELF = $(U)/_init
USER_HDR = $(U)/initcode.h

# 用户程序列表（类似 xv6 的 UPROGS）
# 注意：程序名以 _ 开头，mkfs 会自动去掉下划线
UPROGS = \
	$(U)/_init \
	$(U)/_hello \
	$(U)/_shell \

# 通用用户程序构建规则（类似 xv6 的 _% 规则）
# 从 user/xxx.c 生成 user/_xxxwakeup(
$(U)/_%: $(U)/%.o $(ULIB) $(U)/user.ld
	$(LD) $(USER_LDFLAGS) -o $@ $< $(ULIB)
	$(OBJDUMP) -S $@ > $(U)/$*.asm
	$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(U)/$*.sym

# 编译用户程序 C 文件
$(U)/%.o: $(U)/%.c $(USER_INCS)
	$(USER_CC) $(USER_CFLAGS) -c $< -o $@




# 默认目标
all: $(USER_HDR) kernel.elf fs.img

# 编译汇编文件（统一规则）
kernel/%.o: kernel/%.S
	$(CC) $(CFLAGS) -c $< -o $@

kernel/boot/%.o: kernel/boot/%.S
	$(CC) $(CFLAGS) -c $< -o $@

kernel/trap/%.o: kernel/trap/%.S
	$(CC) $(CFLAGS) -c $< -o $@

kernel/proc/%.o: kernel/proc/%.S
	$(CC) $(CFLAGS) -c $< -o $@

# 编译内核C文件
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


clean: 
	rm -f kernel/*/*.o kernel/*/*.d kernel/*.o kernel/*.d 
	rm -f kernel.elf kernel.asm kernel.sym
	rm -f $(U)/*.o $(U)/*.elf $(U)/_* $(U)/*.asm $(U)/*.sym $(U)/utils/*.o $(USER_HDR)
	rm -f fs.img
QEMU = qemu-system-riscv64
MIN_QEMU_VERSION = 7.2
QEMU_VERSION := $(shell $(QEMU) --version | head -n 1 | sed -E 's/^QEMU emulator version ([0-9]+\.[0-9]+)\..*/\1/')
check-qemu-version:
	@if [ "$(shell echo "$(QEMU_VERSION) >= $(MIN_QEMU_VERSION)" | bc)" -eq 0 ]; then \
		echo "ERROR: Need qemu version >= $(MIN_QEMU_VERSION)"; \
		exit 1; \
	fi


QEMUOPTS = -machine virt -bios none -kernel kernel.elf -m 128M -smp 1 -nographic
QEMUOPTS += -global virtio-mmio.force-legacy=false
QEMUOPTS += -drive file=fs.img,if=none,format=raw,id=x0
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

qemu: check-qemu-version kernel.elf fs.img
	$(QEMU) $(QEMUOPTS)


# 在QEMU中运行（需要安装qemu-system-riscv64）
run: kernel.elf
	qemu-system-riscv64 -machine virt -bios none -kernel $< -m 128M -smp 1 -nographic

run-fs: kernel.elf fs.img
	qemu-system-riscv64 -machine virt -bios none -kernel $< -m 128M -smp 1 -nographic \
		-drive file=fs.img,if=none,format=raw,id=disk0 \
		-device virtio-blk-device,drive=disk0,bus=virtio-mmio-bus.0

run-clean: run clean

# 完整构建和验证
test: clean all check-layout kernel.asm kernel.sym 
	@echo ""
	@echo "=== 构建完成！==="
	@echo "运行: make run"
	@echo "调试: make debug (然后在另一终端运行 gdb-multiarch kernel.elf)"

# 编译用户程序公共库
$(U)/utils/%.o: $(U)/utils/%.c $(USER_INCS)
	$(USER_CC) $(USER_CFLAGS) -c $< -o $@

# 生成initcode.h头文件（从 _init 生成）
# 注意：如果需要 init.elf，可以通过复制 _init 来创建
$(USER_HDR): $(U)/_init
	@echo "// generated from user/_init" > $(USER_HDR)
	@echo "#ifndef INITCODE_H" >> $(USER_HDR)
	@echo "#define INITCODE_H" >> $(USER_HDR)
	@echo "static const unsigned char initcode[] = {" >> $(USER_HDR)
	@hexdump -v -e '1/1 " 0x%02x,"' $(U)/_init | sed 's/$$/ /' >> $(USER_HDR)
	@echo "" >> $(USER_HDR)
	@echo "};" >> $(USER_HDR)
	@echo "static const unsigned int initcode_size = sizeof(initcode);" >> $(USER_HDR)
	@echo "#endif" >> $(USER_HDR)

# 包含依赖文件
-include kernel/*/*.d

.PHONY: all clean clean-all run debug debug-all gdb test check-layout

# 编译 mkfs 工具（类似 xv6，包含必要的头文件）
tools/mkfs: tools/mkfs.c $(K)/fs/fs.h $(K)/include/param.h
	gcc -DMKFS -I. -Wall -o $@ $<

# 创建文件系统镜像（包含所有用户程序）
# 注意：如果不需要 README，可以去掉 README 依赖
fs.img: tools/mkfs $(UPROGS)
	tools/mkfs fs.img $(UPROGS)

# 防止删除中间文件（如 .o 文件），以便在首次构建后保持磁盘镜像的持久性
.PRECIOUS: %.o $(U)/%.o

