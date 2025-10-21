#include "../def.h"

// 具体的系统调用实现函数
uint64 sys_exit(void) {
    printf("sys_exit called\n");
    // TODO: 实现进程退出逻辑
    return 0;
}

uint64 sys_getpid(void) {
    printf("sys_getpid called\n");
    // TODO: 返回实际的进程ID
    return 1;
}

uint64 sys_fork(void) {
    printf("sys_fork called\n");
    // TODO: 实现进程创建逻辑
    return 2;
}

uint64 sys_wait(void) {
    printf("sys_wait called\n");
    // TODO: 实现进程等待逻辑
    return 3;
}

uint64 sys_read(void) {
    printf("sys_read called\n");
    // TODO: 实现读取逻辑
    return 4;
}

uint64 sys_write(void) {
    printf("sys_write called\n");
    // TODO: 实现写入逻辑
    return 5;
}

uint64 sys_open(void) {
    printf("sys_open called\n");
    // TODO: 实现文件打开逻辑
    return 6;
}

uint64 sys_close(void) {
    printf("sys_close called\n");
    // TODO: 实现文件关闭逻辑
    return 7;
}

uint64 sys_exec(void) {
    printf("sys_exec called\n");
    // TODO: 实现程序执行逻辑
    return 8;
}

uint64 sys_sbrk(void) {
    printf("sys_sbrk called\n");
    // TODO: 实现内存分配逻辑
    return 9;
}