// ls - 列出目录内容
#include "./utils/syscall.h"
#include "./utils/printf.h"

// 文件类型定义（与内核 stat.h 保持一致）
#define T_DIR     1   // Directory
#define T_FILE    2   // File
#define T_DEVICE  3   // Device

// 目录项结构（与内核 fs.h 保持一致）
#define DIRSIZ 14
struct dirent {
    unsigned short inum;
    char name[DIRSIZ];
};

// 文件状态结构（与内核 stat.h 保持一致）
struct stat {
    int dev;     // File system's disk device
    unsigned int ino;    // Inode number
    short type;  // Type of file
    short nlink; // Number of links to file
    unsigned long size; // Size of file in bytes
};

// 字符串处理函数
static int strlen(const char *s) {
    int n = 0;
    while (*s++)
        n++;
    return n;
}

static int strcmp(const char *p, const char *q) {
    while (*p && *p == *q)
        p++, q++;
    return (unsigned char)*p - (unsigned char)*q;
}

static void strcpy(char *dst, const char *src) {
    while ((*dst++ = *src++) != 0)
        ;
}

// 获取文件名（去掉路径）
static char* basename(char *path) {
    char *p;
    for (p = path + strlen(path); p >= path && *p != '/'; p--)
        ;
    return p + 1;
}

// 格式化输出文件信息
static void print_file(char *name, struct stat *st) {
    char *type_str;
    
    switch(st->type) {
        case T_DIR:
            type_str = "DIR ";
            break;
        case T_FILE:
            type_str = "FILE";
            break;
        case T_DEVICE:
            type_str = "DEV ";
            break;
        default:
            type_str = "??? ";
            break;
    }
    
    printf("%s %d %s\n", type_str, (int)st->size, name);
}

// 列出目录内容
static void ls_dir(char *path) {
    int fd;
    struct dirent de;
    struct stat st;
    char full_path[512];
    int path_len = strlen(path);
    
    // 打开目录
    if((fd = sys_open(path, O_RDONLY)) < 0) {
        printf("ls: 无法打开目录 %s\n", path);
        return;
    }
    
    // 读取目录项
    while(sys_read(fd, (char*)&de, sizeof(de)) == sizeof(de)) {
        if(de.inum == 0)
            continue;
        
        // 跳过 . 和 ..（可选）
        if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
            continue;
        
        // 提取文件名（处理非 null 结尾的情况）
        char name_buf[DIRSIZ + 1];
        int name_len = 0;
        while(name_len < DIRSIZ && de.name[name_len] != 0)
            name_len++;
        for(int i = 0; i < name_len; i++)
            name_buf[i] = de.name[i];
        name_buf[name_len] = 0;
        
        // 构建完整路径
        strcpy(full_path, path);
        char *p = full_path + path_len;
        if(path_len > 0 && full_path[path_len - 1] != '/')
            *p++ = '/';
        strcpy(p, name_buf);
        
        // 获取文件状态信息
        int file_fd = sys_open(full_path, O_RDONLY);
        if(file_fd < 0) {
            printf("%s\n", name_buf);  // 无法打开，仅显示文件名
            continue;
        }
        
        if(sys_fstat(file_fd, &st) < 0) {
            printf("%s\n", name_buf);  // 无法获取状态，仅显示文件名
            sys_close(file_fd);
            continue;
        }
        
        sys_close(file_fd);
        print_file(name_buf, &st);
    }
    
    sys_close(fd);
}

// 列出单个文件信息
static void ls_file(char *path) {
    struct stat st;
    int fd;
    
    if((fd = sys_open(path, O_RDONLY)) < 0) {
        printf("ls: 无法打开 %s\n", path);
        return;
    }
    
    if(sys_fstat(fd, &st) < 0) {
        printf("ls: 无法获取 %s 的状态\n", path);
        sys_close(fd);
        return;
    }
    
    sys_close(fd);
    print_file(basename(path), &st);
}

// ls 主函数
static void ls(char *path) {
    int fd;
    struct stat st;
    
    // 尝试打开
    if((fd = sys_open(path, O_RDONLY)) < 0) {
        printf("ls: 无法打开 %s\n", path);
        return;
    }
    
    // 使用 fstat 判断文件类型
    if(sys_fstat(fd, &st) < 0) {
        printf("ls: 无法获取 %s 的状态\n", path);
        sys_close(fd);
        return;
    }
    
    sys_close(fd);
    
    // 根据文件类型选择处理方式
    switch(st.type) {
        case T_FILE:
            ls_file(path);
            break;
        case T_DIR:
            ls_dir(path);
            break;
        default:
            printf("ls: %s 类型未知\n", path);
            break;
    }
}

void main(int argc, char *argv[]) {
    if(argc < 2) {
        // 没有参数，列出当前目录 "."
        ls(".");
    } else {
        // 列出所有指定的路径
        for(int i = 1; i < argc; i++) {
            if(argc > 2) {
                printf("%s:\n", argv[i]);
            }
            ls(argv[i]);
            if(i < argc - 1) {
                printf("\n");
            }
        }
    }
    
    sys_exit(0);
}

