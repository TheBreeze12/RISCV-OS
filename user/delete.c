// delete - 删除一个文件
#include "./utils/syscall.h"
#include "./utils/printf.h"

void main(int argc, char *argv[]) {
    if(argc < 2) {
        printf("用法: delete <文件名>\n");
        sys_exit(1);
    }
    
    if(sys_unlink(argv[1]) < 0) {
        printf("delete: 无法删除文件 '%s'\n", argv[1]);
        sys_exit(1);
    }
    
    printf("已删除: %s\n", argv[1]);
    sys_exit(0);
}

