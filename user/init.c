// 最小用户程序：演示用户空间 printf
#include "syscall.h"

// 用户程序入口点

void main(void) {
    // 测试基本的 printf 功能

        for(int i=0;;i++){
        if(i%1000000==0){
            sys_getpid();
        }}
}
