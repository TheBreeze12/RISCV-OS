#ifndef FSIMG_H
#define FSIMG_H

#include "../type.h"

// 文件系统镜像格式定义
#define FS_MAGIC "RISCVFS"
#define FS_MAGIC_SIZE 8
#define FS_HEADER_SIZE 64
#define DIRENT_SIZE 64
#define MAX_FILENAME 55

// 文件系统头
struct fs_header {
    char magic[8];
    uint32 nfiles;
    char reserved[52];
};

// 目录项
struct fs_dirent {
    char name[56];
    uint32 offset;
    uint32 size;
};

// 文件系统镜像操作
int fsimg_init(void *image_base, uint64 image_size);
struct file* fsimg_namei(char *path);

// 外部变量（在fsimg.c中定义）
extern void *fs_image;

#endif // FSIMG_H

