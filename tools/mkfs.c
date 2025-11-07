// 简单的文件系统镜像构建工具
// 格式：
// - 文件系统头（64字节）
//   - magic: "RISCVFS" (8字节)
//   - nfiles: 文件数量 (4字节)
//   - reserved: 保留 (52字节)
// - 目录项数组（每个64字节）
//   - name: 文件名 (56字节，null结尾)
//   - offset: 文件在镜像中的偏移 (4字节)
//   - size: 文件大小 (4字节)
// - 文件数据区

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

#define FS_MAGIC "RISCVFS"
#define FS_MAGIC_SIZE 8
#define FS_HEADER_SIZE 64
#define DIRENT_SIZE 64
#define MAX_FILENAME 55

struct fs_header {
    char magic[8];
    uint32_t nfiles;
    char reserved[52];
};

struct dirent {
    char name[56];
    uint32_t offset;
    uint32_t size;
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "用法: %s <输出镜像文件> <文件1> [文件2] ...\n", argv[0]);
        fprintf(stderr, "示例: %s fs.img /path/to/init.elf /path/to/program2.elf\n", argv[0]);
        return 1;
    }

    const char *output_file = argv[1];
    int nfiles = argc - 2;
    
    if (nfiles > 255) {
        fprintf(stderr, "错误: 文件数量过多（最多255个）\n");
        return 1;
    }

    // 打开输出文件
    FILE *out = fopen(output_file, "wb");
    if (!out) {
        perror("无法创建输出文件");
        return 1;
    }

    // 写入文件系统头（先占位，稍后更新）
    struct fs_header header;
    memset(&header, 0, sizeof(header));
    memcpy(header.magic, FS_MAGIC, FS_MAGIC_SIZE);
    header.nfiles = nfiles;
    fwrite(&header, sizeof(header), 1, out);

    // 目录项数组（先占位）
    struct dirent *dirs = calloc(nfiles, sizeof(struct dirent));
    if (!dirs) {
        fprintf(stderr, "内存分配失败\n");
        fclose(out);
        return 1;
    }

    // 计算数据区起始位置
    uint32_t data_offset = FS_HEADER_SIZE + nfiles * DIRENT_SIZE;

    // 处理每个输入文件
    for (int i = 0; i < nfiles; i++) {
        const char *input_file = argv[2 + i];
        
        // 打开输入文件
        FILE *in = fopen(input_file, "rb");
        if (!in) {
            fprintf(stderr, "错误: 无法打开文件 %s\n", input_file);
            free(dirs);
            fclose(out);
            return 1;
        }

        // 获取文件大小
        fseek(in, 0, SEEK_END);
        uint32_t file_size = ftell(in);
        fseek(in, 0, SEEK_SET);

        // 提取文件名（从完整路径中）
        const char *name = strrchr(input_file, '/');
        if (name) {
            name++;  // 跳过 '/'
        } else {
            name = input_file;
        }

        // 如果路径以 / 开头，保留完整路径
        if (input_file[0] == '/') {
            // 去掉前导斜杠，但保留路径结构
            strncpy(dirs[i].name, input_file + 1, MAX_FILENAME);
        } else {
            strncpy(dirs[i].name, name, MAX_FILENAME);
        }
        dirs[i].name[MAX_FILENAME] = '\0';

        // 设置目录项
        dirs[i].offset = data_offset;
        dirs[i].size = file_size;

        // 读取并写入文件数据
        char *buffer = malloc(file_size);
        if (!buffer) {
            fprintf(stderr, "内存分配失败\n");
            fclose(in);
            free(dirs);
            fclose(out);
            return 1;
        }

        if (fread(buffer, 1, file_size, in) != file_size) {
            fprintf(stderr, "错误: 读取文件 %s 失败\n", input_file);
            free(buffer);
            fclose(in);
            free(dirs);
            fclose(out);
            return 1;
        }

        // 写入文件数据
        fseek(out, data_offset, SEEK_SET);
        fwrite(buffer, 1, file_size, out);

        data_offset += file_size;
        
        // 对齐到4字节边界
        if (data_offset % 4 != 0) {
            uint32_t padding = 4 - (data_offset % 4);
            char zero = 0;
            for (uint32_t j = 0; j < padding; j++) {
                fwrite(&zero, 1, 1, out);
            }
            data_offset += padding;
        }

        free(buffer);
        fclose(in);

        printf("添加文件: %s (大小: %u 字节, 偏移: 0x%x)\n", 
               dirs[i].name, dirs[i].size, dirs[i].offset);
    }

    // 写入目录项数组
    fseek(out, FS_HEADER_SIZE, SEEK_SET);
    fwrite(dirs, sizeof(struct dirent), nfiles, out);

    free(dirs);
    fclose(out);

    printf("文件系统镜像创建成功: %s\n", output_file);
    printf("文件数量: %d\n", nfiles);
    printf("镜像大小: %u 字节\n", data_offset);

    return 0;
}

