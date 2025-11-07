#include "proc/proc.h"
#include "elf.h"
#include "fs/file.h"

static int loadseg(pagetable_t pagetable, uint64 va, struct file *f, uint offset, uint filesz);

int
exec(char *path, char **argv)
{
  char *s, *last;
  int i, off;
  uint64 argc,sz=0, sp, ustack[MAXARG], stackbase;
  struct elfhdr elf;
  struct proghdr ph;
  pagetable_t pagetable = 0, oldpagetable;
  struct file *f;
  struct proc *p = myproc();

  // 打开文件
  if((f = namei(path)) == 0){
    return -1;
  }

  // 读取 ELF 头
  if(fileread(f, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;
  if(elf.magic != ELF_MAGIC)
    goto bad;

  // 创建新页表
  if((pagetable = proc_pagetable(p)) == 0)
    goto bad;


  // 加载程序段

  for(i = 0, off = elf.phoff; i < elf.phnum; i++, off += sizeof(ph)){
    if(fileread(f, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != PT_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    uint64 sz1;
    if((sz1 = uvmalloc(pagetable, ph.vaddr, ph.vaddr + ph.memsz)) == 0)
      goto bad;
    if(sz1 < ph.vaddr + ph.memsz)
      sz1 = ph.vaddr + ph.memsz;
    sz=sz1;
    if(loadseg(pagetable, ph.vaddr, f, ph.off, ph.filesz) < 0)
      goto bad;
  }
  uint64 oldsz = p->sz;


  // 分配用户栈
  sz = PGROUNDUP(sz);
  uint64 sz1;
  if((sz1 = uvmalloc(pagetable, sz, sz + (USERSTACK+1)*PGSIZE)) == 0)
    goto bad;
  sz = sz1;
  uvmclear(pagetable, sz-(USERSTACK+1)*PGSIZE);
  sp = sz;
  stackbase = sp - USERSTACK*PGSIZE;



  // Copy argument strings into new stack, remember their
  // addresses in ustack[].
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp -= strlen(argv[argc]) + 1;
    sp -= sp % 16; // riscv sp must be 16-byte aligned
    if(sp < stackbase)
      goto bad;
    if(copyout(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[argc] = sp;
  }
  ustack[argc] = 0;

  // push a copy of ustack[], the array of argv[] pointers.
  sp -= (argc+1) * sizeof(uint64);
  sp -= sp % 16;
  if(sp < stackbase)
    goto bad;
  if(copyout(pagetable, sp, (char *)ustack, (argc+1)*sizeof(uint64)) < 0)
    goto bad;

  // a0 and a1 contain arguments to user main(argc, argv)
  // argc is returned via the system call return
  // value, which goes in a0.
  p->trapframe->a1 = sp;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(p->name, last, sizeof(p->name));
    
  // Commit to the user image.
  oldpagetable = p->pagetable;
  p->pagetable = pagetable;
  p->sz = sz;
  p->trapframe->epc = elf.entry;  // initial program counter = main
  p->trapframe->sp = sp; // initial stack pointer
  proc_freepagetable(oldpagetable, oldsz);

  return argc; // this ends up in a0, the first argument to main(argc, argv)

bad:
  if(pagetable)
    proc_freepagetable(pagetable, 0);
  if(f)
    fileclose(f);
  return -1;
}
//   // 准备参数
//   argc = 0;
//   if(argv != 0) {
//     for(argc = 0; argc < MAXARG && argv[argc]; argc++)
//       ;
//   }
//   ustack[argc] = 0;  // 结束标记
  
//   // 提取程序名
//   for(last=s=path; *s; s++)
//     if(*s == '/')
//       last = s+1;
//   safestrcpy(p->name, last, sizeof(p->name));

//   // 设置栈指针和参数
//   sp = stackbase;
//   sp -= sp % 16;  // riscv sp must be 16-byte aligned
//   printf("hello\n");
  
//   // 将参数字符串复制到栈上
//   for(i = argc-1; i >= 0; i--){
//     sp -= strlen(argv[i]) + 1;
//     sp -= sp % 16;
//     if(copyout(pagetable, sp, argv[i], strlen(argv[i]) + 1) < 0)
//       goto bad;
//     ustack[i] = sp;
//   }
//   printf("hello\n");

//   // 复制参数指针到栈
//   sp -= (argc+1) * sizeof(uint64);
//   sp -= sp % 16;
//   if(copyout(pagetable, sp, (char *)ustack, (argc+1)*sizeof(uint64)) < 0)
//     goto bad;

//   // 保存旧页表和大小
//   oldpagetable = p->pagetable;
//   p->pagetable = pagetable;
//   p->sz = sz1;

//   // 设置 trapframe
//   memset(p->trapframe, 0, sizeof(*p->trapframe));
//   p->trapframe->epc = elf.entry;
//   p->trapframe->sp = sp;

//   // 释放旧页表
//   proc_freepagetable(oldpagetable, oldsz);
//   fileclose(f);
//   return 0;
// 加载一个程序段到内存
static int
loadseg(pagetable_t pagetable, uint64 va, struct file *f, uint offset, uint filesz)
{
  uint i;
  uint64 pa;

  for(i = 0; i < filesz; i += PGSIZE){
    pa = walkaddr(pagetable, va + i);
    if(pa == 0)
      panic("loadseg: address should exist");
    uint64 sz = filesz - i;
    if(sz > PGSIZE)
      sz = PGSIZE;
    if(fileread(f, pa, offset + i, sz) != sz)
      return -1;
    if(va + i + sz < va + i)
      return -1;
  }
  return 0;
}