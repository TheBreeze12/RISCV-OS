#include "fsimg.h"
#include "file.h"
#include "../def.h"

// 文件系统镜像在内存中的位置（非static，供namei.c使用）
void *fs_image = 0;
static uint64 fs_image_size = 0;
static struct fs_header *fs_header = 0;
static struct fs_dirent *fs_dirents = 0;

// 初始化文件系统镜像
int fsimg_init(void *image_base, uint64 image_size) {
    if (image_base == 0 || image_size < FS_HEADER_SIZE) {
        return -1;
    }

    fs_image = image_base;
    fs_image_size = image_size;
    fs_header = (struct fs_header*)image_base;

    // 检查magic number
    int magic_match = 1;
    for (int i = 0; i < FS_MAGIC_SIZE; i++) {
        if (fs_header->magic[i] != FS_MAGIC[i]) {
            magic_match = 0;
            break;
        }
    }
    if (!magic_match) {
        printf("fsimg_init: 无效的文件系统镜像（magic不匹配）\n");
        return -1;
    }

    // 检查文件数量
    if (fs_header->nfiles == 0 || fs_header->nfiles > 255) {
        printf("fsimg_init: 无效的文件数量: %d\n", fs_header->nfiles);
        return -1;
    }

    // 计算目录项数组位置
    fs_dirents = (struct fs_dirent*)((char*)image_base + FS_HEADER_SIZE);

    printf("文件系统镜像加载成功: %d 个文件\n", fs_header->nfiles);
    return 0;
}

// 在文件系统镜像中查找文件
struct file* fsimg_namei(char *path) {
    if (fs_image == 0 || fs_header == 0) {
        return 0;
    }

    // 规范化路径（去掉前导斜杠）
    const char *name = path;
    if (*name == '/') {
        name++;
    }
    if (*name == '\0') {
        name = "init";  // 默认查找init
    }

    // 在目录项中查找
    for (uint32 i = 0; i < fs_header->nfiles; i++) {
        if (strcmp(fs_dirents[i].name, name) == 0) {
            // 找到文件，创建file结构
            struct file *f = filealloc();
            if (f == 0) {
                return 0;
            }

            f->type = FD_INODE;
            f->readable = 1;
            f->writable = 0;
            f->size = fs_dirents[i].size;

            // 检查偏移和大小是否有效
            uint32 offset = fs_dirents[i].offset;
            if (offset + fs_dirents[i].size > fs_image_size) {
                printf("fsimg_namei: 文件 %s 超出镜像范围\n", name);
                fileclose(f);
                return 0;
            }

            // 分配内存并复制文件数据
            f->data = kalloc();
            if (f->data == 0) {
                fileclose(f);
                return 0;
            }

            memmove(f->data, (char*)fs_image + offset, fs_dirents[i].size);
            return f;
        }
    }

    return 0;  // 未找到文件
}

