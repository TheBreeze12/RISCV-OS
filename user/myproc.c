#include "./utils/syscall.h"
#include "./utils/printf.h"
// 用户程序入口点

void main(void) {
    printf("这是文件系统中自己写的程序\n");
    sys_exit(0);
}