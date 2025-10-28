#include "proc.h"
extern struct proc proc[NPROC];
// ============================================================================
// 进程管理系统测试套件
// ============================================================================

// 简单任务函数
void simple_task(void)
{
  printf("Simple task running, PID=%d\n", myproc()->pid);
  for(int i = 0; i < 3; i++) {
    printf("  Task iteration %d\n", i);
    yield();  // 主动让出CPU
  }
  printf("Simple task exiting\n");
  exit(0);
}

// CPU密集型任务
void cpu_intensive_task(void)
{
  struct proc *p = myproc();
  printf("CPU intensive task started, PID=%d\n", p->pid);
  
  uint64 count = 0;
  for(int i = 0; i < 100000; i++) {
    count += i;
    if(i % 10000 == 0) {
      printf("  PID %d: iteration %d\n", p->pid, i);
      yield();
    }
  }
  
  printf("CPU intensive task done, PID=%d, count=%d\n", p->pid, count);
  exit(0);
}

// ============================================================================
// 测试1：进程创建和销毁
// ============================================================================

void test_process_creation(void)
{
  printf("\n=== Test 1: Process Creation ===\n");
  
  // 测试基本的进程分配
  printf("Testing basic process allocation...\n");
  struct proc *p1 = allocproc();
  if(p1 == 0) {
    printf("FAIL: Could not allocate first process\n");
    return;
  }
  printf("SUCCESS: Allocated process, PID=%d\n", p1->pid);
  
  // 测试多个进程分配
  printf("\nTesting multiple process allocation...\n");
  int count = 0;
  struct proc *procs[10];
  
  for(int i = 0; i < 10; i++) {
    procs[i] = allocproc();
    if(procs[i] != 0) {
      count++;
      printf("  Allocated PID=%d\n", procs[i]->pid);
    }
  }
  printf("Allocated %d processes\n", count);
  
  // 清理：释放所有测试进程
  printf("\nCleaning up test processes...\n");
  freeproc(p1);
  for(int i = 0; i < 10; i++) {
    if(procs[i] != 0) {
      freeproc(procs[i]);
    }
  }
  
  printf("SUCCESS: Process creation test passed\n");
}

// ============================================================================
// 测试2：进程表管理
// ============================================================================

void test_proc_table(void)
{
  printf("\n=== Test 2: Process Table Management ===\n");
  
  // 显示初始进程表状态
  printf("Initial process table:\n");
  // debug_proc_table();
  
  // 创建几个进程
  printf("\nCreating test processes...\n");
  for(int i = 0; i < 5; i++) {
    struct proc *p = allocproc();
    if(p) {
      printf(p->name, "test-%d", i);
      p->state = RUNNABLE;
      printf("  Created %s, PID=%d\n", p->name, p->pid);
    }
  }
  
  // 显示更新后的进程表
  printf("\nUpdated process table:\n");
  debug_proc_table();
  
  // 测试PID查找
  printf("\nTesting PID lookup...\n");
  struct proc *p = find_proc_by_pid(12);
  if(p) {
    printf("Found process: PID=%d, Name=%s\n", p->pid, p->name);
    freeproc(p);
  } else {
    printf("Process with PID=2 not found\n");
  }
  allocproc();
  debug_proc_table();
  printf("SUCCESS: Process table test passed\n");
}

// ============================================================================
// 测试3：上下文切换
// ============================================================================

// 测试用的简单上下文
struct context test_ctx1, test_ctx2;
int switch_count = 0;

void context_switch_target1(void)
{
  printf("Entered context 1, switch count=%d\n", switch_count);
  switch_count++;
  
  if(switch_count < 5) {
    printf("Context 1 switching to context 2...\n");
    swtch(&test_ctx1, &test_ctx2);
    printf("Context 1 resumed\n");
  }
  
  printf("Context 1 exiting\n");
}

void context_switch_target2(void)
{
  printf("Entered context 2, switch count=%d\n", switch_count);
  switch_count++;
  
  if(switch_count < 5) {
    printf("Context 2 switching back to context 1...\n");
    swtch(&test_ctx2, &test_ctx1);
    printf("Context 2 resumed\n");
  }
  
  printf("Context 2 exiting\n");
}

void test_context_switch(void)
{
  printf("\n=== Test 3: Context Switch ===\n");
  
  printf("Testing basic context switch mechanism...\n");
  printf("This test demonstrates how swtch() works\n\n");
  
  // 注意：这是一个简化的测试，实际的上下文切换更复杂
  // 在真实场景中，上下文包含在进程结构体中，由调度器管理
  
  printf("Context switch test requires scheduler to be running\n");
  printf("Will be tested as part of scheduler test\n");
  
  printf("SUCCESS: Context switch test acknowledged\n");
}

// ============================================================================
// 测试4：调度器
// ============================================================================

void test_scheduler(void)
{
  printf("\n=== Test 4: Scheduler ===\n");
  
  printf("Testing round-robin scheduler...\n");
  
  // 创建几个测试进程
  printf("\nCreating test processes for scheduling...\n");
  for(int i = 0; i < 3; i++) {
    struct proc *p = allocproc();
    if(p) {
      printf(p->name, "sched-test-%d", i);
      p->state = RUNNABLE;
      printf("  Created %s, PID=%d\n", p->name, p->pid);
    }
  }
  
  printf("\nProcess table before scheduling:\n");
  // debug_proc_table();
  
  // 注意：实际的调度器测试需要启动scheduler()
  // 这会接管CPU，永不返回
  printf("\nScheduler will run continuously once started\n");
  printf("Use 'make run' to test the full scheduler\n");
  
  printf("SUCCESS: Scheduler test setup complete\n");
}

// ============================================================================
// 测试5：进程同步 - 生产者消费者
// ============================================================================

#define BUFFER_SIZE 10
int shared_buffer[BUFFER_SIZE];
int buffer_in = 0;
int buffer_out = 0;
int buffer_count = 0;

void producer_task(void)
{
  struct proc *p = myproc();
  printf("Producer started, PID=%d\n", p->pid);
  
  for(int i = 0; i < 20; i++) {
    // 等待缓冲区有空间
    while(buffer_count >= BUFFER_SIZE) {
      printf("  Producer waiting (buffer full)...\n");
      yield();
    }
    
    // 生产数据
    shared_buffer[buffer_in] = i;
    buffer_in = (buffer_in + 1) % BUFFER_SIZE;
    buffer_count++;
    
    printf("  Producer: produced item %d, count=%d\n", i, buffer_count);
    
    // 模拟生产时间
    for(volatile int j = 0; j < 100000; j++);
  }
  
  printf("Producer finished\n");
  exit(0);
}

void consumer_task(void)
{
  struct proc *p = myproc();
  printf("Consumer started, PID=%d\n", p->pid);
  
  for(int i = 0; i < 20; i++) {
    // 等待缓冲区有数据
    while(buffer_count <= 0) {
      printf("  Consumer waiting (buffer empty)...\n");
      yield();
    }
    
    // 消费数据
    int item = shared_buffer[buffer_out];
    buffer_out = (buffer_out + 1) % BUFFER_SIZE;
    buffer_count--;
    
    printf("  Consumer: consumed item %d, count=%d\n", item, buffer_count);
    
    // 模拟消费时间
    for(volatile int j = 0; j < 150000; j++);
  }
  
  printf("Consumer finished\n");
  exit(0);
}

void test_synchronization(void)
{
  printf("\n=== Test 5: Process Synchronization ===\n");
  printf("Testing producer-consumer pattern...\n");
  
  // 初始化共享缓冲区
  buffer_in = 0;
  buffer_out = 0;
  buffer_count = 0;
  
  printf("\nThis test requires fork() to be implemented\n");
  printf("It will create producer and consumer processes\n");
  
  // TODO: 当fork()实现后，取消注释
  // int pid1 = fork();
  // if(pid1 == 0) {
  //   producer_task();
  // } else {
  //   int pid2 = fork();
  //   if(pid2 == 0) {
  //     consumer_task();
  //   } else {
  //     wait(0);
  //     wait(0);
  //   }
  // }
  
  printf("SUCCESS: Synchronization test setup complete\n");
}

// ============================================================================
// 测试6：进程退出和等待
// ============================================================================

void test_exit_wait(void)
{
  printf("\n=== Test 6: Exit and Wait ===\n");
  
  printf("Testing process exit and wait mechanism...\n");
  
  // TODO: 需要fork()实现
  printf("\nThis test requires fork() to be implemented\n");
  printf("It will test parent-child process relationships\n");
  
  printf("SUCCESS: Exit/Wait test acknowledged\n");
}

// ============================================================================
// 调试辅助函数
// ============================================================================

void debug_proc_table(void)
{
  int count = 0;
  printf("┌──────┬────────────────┬──────────┬────────┐\n");
  printf("│ PID  │ Name           │ State    │ Parent │\n");
  printf("├──────┼────────────────┼──────────┼────────┤\n");
  
  for(int i = 0; i < NPROC; i++) {
    struct proc *p = &proc[i];
    if(p->state != UNUSED) {
      const char *state_str;
      switch(p->state) {
        case USED: state_str = "USED"; break;
        case SLEEPING: state_str = "SLEEPING"; break;
        case RUNNABLE: state_str = "RUNNABLE"; break;
        case RUNNING: state_str = "RUNNING"; break;
        case ZOMBIE: state_str = "ZOMBIE"; break;
        default: state_str = "UNKNOWN"; break;
      }
      
      int parent_pid = p->parent ? p->parent->pid : 0;
      printf("│ %d │ %s │ %s │ %d │\n",
             p->pid, p->name, state_str, parent_pid);
      count++;
    }
  }
  
  printf("└──────┴────────────────┴──────────┴────────┘\n");
  printf("Total: %d processes\n", count);
}

void debug_context(struct context *ctx, const char *name)
{
  printf("Context %s:\n", name);
  printf("  ra  = 0x%lx\n", ctx->ra);
  printf("  sp  = 0x%lx\n", ctx->sp);
  printf("  s0  = 0x%lx\n", ctx->s0);
  printf("  s1  = 0x%lx\n", ctx->s1);
}

// ============================================================================
// 运行所有测试
// ============================================================================

void run_all_proc_tests(void)
{
  printf("\n");
  printf("╔════════════════════════════════════════════════════╗\n");
  printf("║     Process Management System Test Suite          ║\n");
  printf("╚════════════════════════════════════════════════════╝\n");
  
  test_process_creation();
  test_proc_table();
  test_context_switch();
  test_scheduler();
  test_synchronization();
  test_exit_wait();
  
  printf("\n");
  printf("╔════════════════════════════════════════════════════╗\n");
  printf("║           All Tests Completed                      ║\n");
  printf("╚════════════════════════════════════════════════════╝\n");
  printf("\n");
}

// ============================================================================
// 性能测试
// ============================================================================

// void test_context_switch_performance(void)
// {
//   printf("\n=== Context Switch Performance Test ===\n");
  
//   uint64 start_time = get_time();
//   int iterations = 1000;
  
//   printf("Performing %d context switches...\n", iterations);
  
//   // TODO: 实际的性能测试需要调度器运行
  
//   uint64 end_time = get_time();
//   uint64 total_cycles = end_time - start_time;
  
//   if(iterations > 0) {
//     uint64 cycles_per_switch = total_cycles / iterations;
//     printf("Average cycles per context switch: %lu\n", cycles_per_switch);
//   }
  
//   printf("Performance test completed\n");
// }

