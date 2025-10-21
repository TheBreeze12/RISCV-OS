#include "proc.h"

struct cpu cpus[NCPU];
struct proc proc[NPROC];
static void freeproc(struct proc *p);
extern void forkret(void);
extern char trampoline[]; // trampoline.S
struct proc *initproc;

int nextpid = 1;

int
cpuid()
{
  int id = r_tp();
  return id;
}

struct cpu*
mycpu(void)
{
  int id = cpuid();
  return &cpus[id];
}

void
procinit(void)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
      p->state = UNUSED;
      p->kstack = KSTACK((int) (p - proc));
  }
}

struct proc*
myproc(void)
{
  intr_off();
  // push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  // pop_off();
  intr_on();
  return p;
}


int
allocpid()
{
  int pid;
  
  pid = nextpid;
  nextpid = nextpid + 1;

  return pid;
}


static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p->state == UNUSED) {
      goto found;
  }
  return 0;
}

found:
  p->pid = allocpid();
  p->state = USED;

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = create_pagetable();
  if(pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// void
// forkret(void)
// {
//   extern char userret[];
//   static int first = 1;
//   struct proc *p = myproc();

//   // Still holding p->lock from scheduler.

//   if (first) {
//     // File system initialization must be run in the context of a
//     // regular process (e.g., because it calls sleep), and thus cannot
//     // be run from main().
//     // fsinit(ROOTDEV);

//     first = 0;
//     // ensure other cores see first=0.
//     __sync_synchronize();

//     // We can invoke kexec() now that file system is initialized.
//     // Put the return value (argc) of kexec into a0.
//     p->trapframe->a0 = kexec("/init", (char *[]){ "/init", 0 });
//     if (p->trapframe->a0 == -1) {
//       panic("exec");
//     }
//   }

//   // return to user space, mimicing usertrap()'s return.
//   prepare_return();
//   uint64 satp = MAKE_SATP(p->pagetable);
//   uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
//   ((void (*)(uint64))trampoline_userret)(satp);
// }

static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}

void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;
  
  // p->cwd = namei("/");

  p->state = RUNNABLE;

}

int
kfork(void)
{
  int pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  // if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
  //   freeproc(np);
  //   return -1;
  // }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  // for(i = 0; i < NOFILE; i++)
  //   if(p->ofile[i])
  //     np->ofile[i] = filedup(p->ofile[i]);
  // np->cwd = idup(p->cwd);

  // safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;


  np->parent = p;

  np->state = RUNNABLE;

  return pid;
}
void
kexit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  // for(int fd = 0; fd < NOFILE; fd++){
  //   if(p->ofile[fd]){
  //     struct file *f = p->ofile[fd];
  //     fileclose(f);
  //     p->ofile[fd] = 0;
  //   }
  // }

  // begin_op();
  // iput(p->cwd);
  // end_op();
  p->cwd = 0;

  // acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  // wakeup(p->parent);
  
  // acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  // release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

int
kwait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  // acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(pp = proc; pp < &proc[NPROC]; pp++){
      if(pp->parent == p){
        // make sure the child isn't still in exit() or swtch().
        // acquire(&pp->lock);

        havekids = 1;
        if(pp->state == ZOMBIE){
          // Found one.
          pid = pp->pid;
          // if(addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
          //                         sizeof(pp->xstate)) < 0) {
          //   release(&pp->lock);
          //   release(&wait_lock);
          //   return -1;
          // }
          freeproc(pp);
          // release(&pp->lock);
          // release(&wait_lock);
          return pid;
        }
        // release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || killed(p)){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}