// echo - 写入内容到文件
#include "./utils/syscall.h"
#include "./utils/printf.h"

static int strlen(const char *s) {
    int n = 0;
    while (*s++)
        n++;
    return n;
}

void main(int argc, char *argv[]) {
    int fd;
    
    if(argc < 3) {
        printf("用法: echo <内容> <文件名>\n");
        printf("示例: echo \"Hello World\" test.txt\n");
        sys_exit(1);
    }
    
    // 打开文件，如果不存在则创建，清空原内容
    fd = sys_open(argv[2], O_CREATE | O_WRONLY | O_TRUNC);
    if(fd < 0) {
        printf("echo: 无法打开文件 '%s'\n", argv[2]);
        sys_exit(1);
    }
    
    // 写入内容
    const char *content = argv[1];
    int len = strlen(content);
    if(sys_write(fd, content, len) != len) {
        printf("echo: 写入失败\n");
        sys_close(fd);
        sys_exit(1);
    }
    
    // 写入换行符
    sys_write(fd, "\n", 1);
    
    sys_close(fd);
    sys_exit(0);
}

