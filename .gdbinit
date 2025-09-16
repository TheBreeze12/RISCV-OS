# RISC-V内核调试配置
set architecture riscv:rv64
set disassemble-next-line on
set print pretty on

# 连接到QEMU
define connect
    target remote :1234
    symbol-file kernel.elf
end

# 常用断点
define setup-breakpoints
    # 在内核入口点设置断点
    break _entry
    # 在main函数设置断点  
    break main
    # 在start函数设置断点（如果存在）
    break start
end

# 显示寄存器状态
define show-regs
    info registers
    info registers pc sp ra
end

# 显示内存布局
define show-layout
    info target
    info files
end

# 帮助信息
define help-riscv
    echo Available commands:\n
    echo   connect          - 连接到QEMU调试会话\n
    echo   setup-breakpoints - 设置常用断点\n
    echo   show-regs        - 显示寄存器状态\n  
    echo   show-layout      - 显示内存布局\n
    echo   c                - 继续执行\n
    echo   n                - 下一行\n
    echo   s                - 单步执行\n
    echo   bt               - 显示调用栈\n
end

# 启动时显示帮助
echo RISC-V内核调试环境已加载\n
echo 输入 'help-riscv' 查看可用命令\n
echo 输入 'connect' 连接到QEMU\n 