#include "file.h"
#include "../def.h"

struct {
  struct file file[NFILE];
} ftable;

struct file*
filealloc(void)
{
  struct file *f;
  for(f = ftable.file; f < ftable.file + NFILE; f++){
    if(f->ref == 0){
      f->ref = 1;
      return f;
    }
  }
  return 0;
}

void
fileclose(struct file *f)
{
  if(f->ref < 1)
    panic("fileclose");
  f->ref--;
  if(f->ref == 0){
    if(f->type == FD_INODE && f->data) {
      kfree(f->data);
      f->data = 0;
    }
  }
}

struct file*
filedup(struct file *f)
{
  if(f->ref < 1)
    panic("filedup");
  f->ref++;
  return f;
}

// 从文件读取数据到地址 addr，支持从偏移量 offset 开始读取
int
fileread(struct file *f, uint64 addr, uint offset, int n)
{
  if(f->readable == 0)
    return -1;
  if(f->type == FD_INODE && f->data) {
    // 检查偏移量是否有效
    if(offset >= f->size)
      return 0;
    // 确保不会读取超出文件大小
    if(offset + n > f->size)
      n = f->size - offset;
    if(n > 0) {
      memmove((void*)addr, f->data + offset, n);
      return n;
    }
  }
  return 0;
}