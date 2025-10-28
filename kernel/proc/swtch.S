# Context switch
#
#   void swtch(struct context *old, struct context *new);
# 
# Save current registers in old. Load from new.
# 上下文切换是操作系统最核心的机制之一
# 
# 关键问题：为什么只保存被调用者保存寄存器(callee-saved)？
# 答案：因为调用者保存寄存器(caller-saved)已经由调用swtch的函数保存在栈上了
# 
# RISC-V寄存器约定：
# - 被调用者保存(callee-saved): s0-s11, sp, ra
# - 调用者保存(caller-saved): t0-t6, a0-a7
# 
# 当一个函数调用swtch时：
# 1. 编译器已经将需要保存的caller-saved寄存器压入栈
# 2. swtch只需要保存callee-saved寄存器
# 3. 切换到新上下文后，返回到新的调用点
# 4. 新的调用者会恢复它自己的caller-saved寄存器

.globl swtch
.align 4
swtch:
        # a0 = old context (保存当前上下文的地址)
        # a1 = new context (要切换到的上下文的地址)
        
        # 保存被调用者保存寄存器到old context
        # 注意：这里的偏移量必须与struct context定义匹配
        sd ra, 0(a0)      # 保存返回地址
        sd sp, 8(a0)      # 保存栈指针
        sd s0, 16(a0)     # 保存帧指针
        sd s1, 24(a0)
        sd s2, 32(a0)
        sd s3, 40(a0)
        sd s4, 48(a0)
        sd s5, 56(a0)
        sd s6, 64(a0)
        sd s7, 72(a0)
        sd s8, 80(a0)
        sd s9, 88(a0)
        sd s10, 96(a0)
        sd s11, 104(a0)

        # 从new context恢复被调用者保存寄存器
        ld ra, 0(a1)      # 恢复返回地址 - 这决定了返回到哪里！
        ld sp, 8(a1)      # 恢复栈指针 - 切换到新的栈！
        ld s0, 16(a1)
        ld s1, 24(a1)
        ld s2, 32(a1)
        ld s3, 40(a1)
        ld s4, 48(a1)
        ld s5, 56(a1)
        ld s6, 64(a1)
        ld s7, 72(a1)
        ld s8, 80(a1)
        ld s9, 88(a1)
        ld s10, 96(a1)
        ld s11, 104(a1)
        
        # 返回到新上下文的ra指向的地址
        # 第一次调度时，ra指向forkret
        # 后续调度时，ra指向上次swtch调用后的位置
        ret

# 上下文切换的魔法：
# 
# 假设进程A调用swtch切换到进程B：
# 1. A调用swtch(&A.context, &B.context)
# 2. swtch保存A的寄存器到A.context
# 3. swtch恢复B的寄存器从B.context
# 4. ret跳转到B.context.ra
# 5. 现在CPU在执行B的代码了！
# 
# 当B再次被切换走时：
# 1. B调用swtch(&B.context, &C.context)
# 2. swtch保存B的寄存器（包括ra，指向swtch返回后的位置）
# 3. ...
# 
# 当B再次被调度时：
# 1. 某个进程调用swtch(&X.context, &B.context)
# 2. swtch恢复B的寄存器（ra指向上次保存的位置）
# 3. ret返回到B上次离开的地方
# 4. B继续执行，就像从未离开过一样！

