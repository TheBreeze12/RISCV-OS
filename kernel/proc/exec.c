#include "proc.h"
#include "../include/elf.h"
#include "../fs/file.h"
#include "../fs/fs.h"

static int loadseg(pagetable_t pagetable, uint64 va, struct inode *f, uint offset, uint filesz);

int flags2perm(int flags)
{
    int perm = 0;
    if(flags & 0x1)
      perm = PTE_X;
    if(flags & 0x2)
      perm |= PTE_W;
    return perm;
}
int
exec(char *path, char **argv)
{
  char *s, *last;
  int i, off;
  uint64 argc, sz = 0, sp, ustack[MAXARG], stackbase;
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pagetable_t pagetable = 0, oldpagetable;
  struct proc *p = myproc();
  uint64 oldsz;

  begin_op();

  // Open the executable file.
  if((ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);

  // Read the ELF header.
  if(readi(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;

  // Is this really an ELF file?
  if(elf.magic != ELF_MAGIC)
    goto bad;
  if((pagetable = proc_pagetable(p)) == 0)
    goto bad;

  // Load program into memory.
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != PT_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    if(ph.vaddr % PGSIZE != 0)
      goto bad;
    uint64 sz1;
    if((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz, flags2perm(ph.flags))) == 0)
      goto bad;
    sz = sz1;
    if(loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  end_op();
  ip = 0;

  p = myproc();
  oldsz = p->sz;
  // printf("[DEBUG] exec: oldsz=%x, p->sz=%x\n", oldsz, p->sz);

  // Allocate some pages at the next page boundary.
  // Make the first inaccessible as a stack guard.
  // Use the rest as the user stack.
  sz = PGROUNDUP(sz);
  uint64 sz1;
  if((sz1 = uvmalloc(pagetable, sz, sz + (USERSTACK+1)*PGSIZE, PTE_W)) == 0)
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
    
  // 在切换页表之前设置 trapframe，避免访问已释放的页表
  // 注意：trapframe 是内核地址，但为了安全，在切换前设置
  p->trapframe->epc = elf.entry;
  p->trapframe->sp = sp;
  
  // Commit to the user image.
  oldpagetable = p->pagetable;
  // printf("[DEBUG] exec: before free, oldsz=%x, new sz=%x\n", oldsz, sz);
  p->pagetable = pagetable;  // 切换页表
  p->sz = sz;
  proc_freepagetable(oldpagetable, oldsz);  // 释放旧页表
  // printf("[DEBUG] exec: after proc_freepagetable\n");
  return argc; // this ends up in a0, the first argument to main(argc, argv)

 bad:
  if(pagetable)
  {
  printf("2\n");
    proc_freepagetable(pagetable, sz);
  }
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1;
}

static int
loadseg(pagetable_t pagetable, uint64 va, struct inode *ip, uint offset, uint filesz)
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
    if(readi(ip, 0, pa, offset + i, sz) != sz)
      return -1;
    if(va + i + sz < va + i)
      return -1;
  }
  return 0;
}