// ELF 文件格式定义
#define ELF_MAGIC 0x464C457FU  // "\x7FELF" in little endian

// ELF 文件头
struct elfhdr {
  uint magic;  // ELF magic number
  uchar elf[12];
  ushort type;
  ushort machine;
  uint version;
  uint64 entry;  // 程序入口点
  uint64 phoff;  // 程序头表偏移
  uint64 shoff;  // 段头表偏移
  uint flags;
  ushort ehsize;
  ushort phentsize;
  ushort phnum;  // 程序头数量
  ushort shentsize;
  ushort shnum;
  ushort shstrndx;
};

// 程序头（Program Header）
struct proghdr {
  uint32 type;   // 段类型
  uint32 flags;  // 段标志
  uint64 off;    // 段在文件中的偏移
  uint64 vaddr;  // 段在虚拟地址空间中的地址
  uint64 paddr;  // 段在物理地址空间中的地址（通常与vaddr相同）
  uint64 filesz; // 段在文件中的大小
  uint64 memsz;  // 段在内存中的大小
  uint64 align;  // 对齐要求
};

#define PT_LOAD 1