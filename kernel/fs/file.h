#include "../type.h"
// 文件结构（简化版，暂时不使用 inode）
struct file {
    enum { FD_NONE, FD_PIPE, FD_INODE } type;
    int ref;  // reference count
    char readable;
    char writable;
    // 暂时直接存储文件内容
    uint64 size;
    char *data;
  };
  
  // 文件系统操作（简化版）
  struct file* filealloc(void);
  void fileclose(struct file*);
  struct file* filedup(struct file*);
  int fileread(struct file*, uint64 addr, uint offset, int n);
  int filestat(struct file*, uint64 addr);