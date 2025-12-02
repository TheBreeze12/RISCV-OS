// cat - 读取并显示文件内容
#include "./utils/syscall.h"
#include "./utils/printf.h"

#define BUF_SIZE 512

void main(int argc, char *argv[]) {
    int fd, n;
    char buf[BUF_SIZE];
    
    if(argc < 2) {
        printf("用法: cat <文件名>\n");
        sys_exit(1);
    }
    
    // 打开文件
    fd = sys_open(argv[1], O_RDONLY);
    if(fd < 0) {
        printf("cat: 无法打开文件 '%s'\n", argv[1]);
        sys_exit(1);
    }
    
    // 读取并输出内容
    while((n = sys_read(fd, buf, BUF_SIZE)) > 0) {
        sys_write(1, buf, n);  // 写到标准输出
    }
    
    if(n < 0) {
        printf("cat: 读取错误\n");
        sys_close(fd);
        sys_exit(1);
    }
    
    sys_close(fd);
    sys_exit(0);
}

