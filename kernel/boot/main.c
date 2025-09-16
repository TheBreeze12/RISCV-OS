#include "type.h"

/* 最小RISC-V操作系统主函数
 * 验证启动流程是否正确工作
 */

__attribute__ ((aligned (16))) char stack0[4096];

int test_bss_variable = 0;      
static int static_bss_var;      

int initialized_var = 42;       

void test_stack_function(void);
extern void printf(const char *fmt, ...);

void main(void) {
    printf("My RISC-V OS Starting...\r\n");

    printf("Testing BSS segment:");
    if (test_bss_variable == 0 && static_bss_var == 0) {
        printf("-----OK!\n");
    } else {
        printf("-----FAILED!\n");
    }
    
    printf("Testing DATA segment:");
    if (initialized_var == 42) {
        printf("-----OK!\n");
    } else {
        printf("-----FAILED!\n");
    }
    
    printf("Testing stack:");
    test_stack_function();
    
    printf("System initialization complete!\r\n");
    printf("Entering main loop...\r\n");
        
    while (1) {
        // 目前只是一个无限循环证明系统在运行
    }
}

void test_stack_function(void) {
    char local_array[64];  
    
    for (int i = 0; i < 64; i++) {
        local_array[i] = 'A' + (i % 26);
    }
    
    // 验证数组内容
    if (local_array[0] == 'A' && local_array[25] == 'Z') {
        printf("-----OK!\n");
    } else {
        printf("-----FAILED!\n");
    }
} 