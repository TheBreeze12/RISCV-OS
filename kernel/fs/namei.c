#include "file.h"
#include "../include/def.h"
#include "/home/thebreeze/riscv-os/user/initcode.h"

// 简化的文件查找：直接根据程序名返回预编译的 ELF 数据
// 将来可以扩展为真正的文件系统查找
struct file*
namei(char *path)
{
  struct file *f = filealloc();
  if(f == 0)
    return 0;
  
  f->type = FD_INODE;
  f->readable = 1;
  f->writable = 0;
  
  // 暂时：根据路径名返回对应的 ELF 数据
  // 这里可以扩展为从文件系统读取
  if(strcmp(path, "/init") == 0 || strcmp(path, "init") == 0) {
    // extern const unsigned char initcode[];
    // extern const unsigned int initcode_size;
    
    // 分配内存并复制文件内容
    f->data = kalloc();
    if(f->data == 0) {
      fileclose(f);
      return 0;
    }
    memmove(f->data, (void*)initcode, initcode_size);
    f->size = initcode_size;
    return f;
  }
  
  // 未找到文件
  fileclose(f);
  return 0;
}
