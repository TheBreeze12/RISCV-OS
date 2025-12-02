// touch - 创建一个新文件
#include "./utils/syscall.h"
#include "./utils/printf.h"

void main(int argc, char *argv[]) {
    int fd;
    
    if(argc < 2) {
        printf("用法: touch <文件名>\n");
        sys_exit(1);
    }
    
    // 创建文件（如果不存在）
    fd = sys_open(argv[1], O_CREATE | O_WRONLY);
    if(fd < 0) {
        printf("touch: 无法创建文件 '%s'\n", argv[1]);
        sys_exit(1);
    }
    
    sys_close(fd);
    sys_exit(0);
}

