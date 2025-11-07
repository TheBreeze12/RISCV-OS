// 用户程序入口点 - Shell程序
#include "syscall.h"
#include "printf.h"
#include "scanf.h"
#include "shell.h"
// 用户程序入口点

void test_scanf(void);
void test_syscall(void);

void main(void) {
   shell_main();
//    test_syscall();
}



void test_scanf(void){
    int num;
    char str[32];
    unsigned int hex_num;
    
    printf("请输入一个整数: ");
    if (scanf("%d", &num) == 1) {
        printf("您输入的整数是: %d\n", num);
    } else {
        printf("输入错误\n");
    }
    
    printf("请输入一个字符串: ");
    if (scanf("%s", str) == 1) {
        printf("您输入的字符串是: %s\n", str);
    } else {
        printf("输入错误\n");
    }
    
    printf("请输入一个十六进制数: ");
    if (scanf("%x", &hex_num) == 1) {
        printf("您输入的十六进制数是: 0x%x\n", hex_num);
    } else {
        printf("输入错误\n");
    }
    
    sys_exit(0);
}

void test_syscall(){
    printf("Hello from user space!\n");
    int pid=sys_fork();
    if(pid==0){
        sys_sleep(3);
        printf("child process\n");
        sys_exit(0);

    }else{
        printf("parent process\n");
    }
    printf("after fork\n");
    // sys_exit(0);
    int ppid=sys_wait();
    printf("ppid=%d\n", ppid);
    sys_exit(0);
}