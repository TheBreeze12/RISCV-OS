// 文件系统日志系统（Write-Ahead Logging）
// 用于保证文件系统操作的原子性和崩溃恢复
#include "../include/type.h"
#include "../include/def.h"
#include "../include/param.h"
#include "../utils/spinlock.h"
#include "fs.h"
#include "buf.h"

// 磁盘上的日志头结构
struct logheader {
  int n;                      // 日志中的块数量
  int block[LOGBLOCKS];       // 记录每个日志块对应的实际磁盘块号
};

// 内存中的日志结构
struct log {
  struct spinlock lock;
  int start;                  // 日志区的起始块号
  int size;                   // 日志区的大小（块数）
  int outstanding;            // 当前正在进行的文件系统操作数量
  int committing;             // 是否正在提交
  int dev;                    // 设备号
  struct logheader lh;        // 日志头
};

struct log log;

// 初始化日志系统
void
initlog(int dev, struct superblock *sb)
{
  if (sizeof(struct logheader) >= BSIZE)
    panic("initlog: too big logheader");

  initlock(&log.lock, "log");
  log.start = sb->logstart;
  log.size = sb->nlog;
  log.dev = dev;
  
  // 从日志中恢复未完成的事务
  recover_from_log();
  
  printf("[LOG] Log initialized: start=%d, size=%d blocks\n", 
         log.start, log.size);
}

// 从磁盘读取日志头
static void
read_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *lh = (struct logheader *) (buf->data);
  int i;
  
  log.lh.n = lh->n;
  for (i = 0; i < log.lh.n; i++) {
    log.lh.block[i] = lh->block[i];
  }
  
  brelse(buf);
}

// 将日志头写入磁盘
static void
write_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *hb = (struct logheader *) (buf->data);
  int i;
  
  hb->n = log.lh.n;
  for (i = 0; i < log.lh.n; i++) {
    hb->block[i] = log.lh.block[i];
  }
  
  bwrite(buf);
  brelse(buf);
}

// 从日志恢复：将日志中的块复制到实际位置
static void
install_trans(int recovering)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    // 读取日志中的块
    struct buf *lbuf = bread(log.dev, log.start + tail + 1);
    // 读取目标块
    struct buf *dbuf = bread(log.dev, log.lh.block[tail]);
    // 复制数据
    memmove(dbuf->data, lbuf->data, BSIZE);
    // 写回目标块
    bwrite(dbuf);
    if (recovering == 0)
      bunpin(dbuf);
    brelse(lbuf);
    brelse(dbuf);
  }
}

// 将内存中缓存的块写入日志区
static void
write_log(void)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    // 读取要写入日志的块
    struct buf *to = bread(log.dev, log.start + tail + 1);
    struct buf *from = bread(log.dev, log.lh.block[tail]);
    // 复制数据到日志块
    memmove(to->data, from->data, BSIZE);
    // 写日志块
    bwrite(to);
    brelse(from);
    brelse(to);
  }
}

// 提交当前事务
static void
commit()
{
  if (log.lh.n > 0) {
    write_log();                // 1. 写日志内容到磁盘
    write_head();               // 2. 写日志头（提交点）
    install_trans(0);           // 3. 将日志内容写入实际位置
    log.lh.n = 0;
    write_head();               // 4. 清空日志头（事务完成）
  }
}

// 从日志恢复（在系统启动时调用）
void
recover_from_log(void)
{
  read_head();
  if (log.lh.n > 0) {
    printf("[LOG] Recovering %d blocks from log...\n", log.lh.n);
    install_trans(1);           // 恢复日志中的数据
    log.lh.n = 0;
    write_head();               // 清空日志
    printf("[LOG] Recovery complete\n");
  }
}

// 开始一个文件系统操作
// 增加 outstanding 计数，表示有操作正在进行
void
begin_op(void)
{
  acquire(&log.lock);
  while(1) {
    // 等待其他事务提交完成
    if (log.committing) {
      sleep_lock(&log, &log.lock);
    }
    // 检查日志空间是否足够
    // 每个操作最多写 MAXOPBLOCKS 个块
    else if (log.lh.n + (log.outstanding + 1) * MAXOPBLOCKS > LOGBLOCKS) {
      // 日志空间不足，等待
      sleep_lock(&log, &log.lock);
    } else {
      log.outstanding += 1;
      release(&log.lock);
      break;
    }
  }
}

// 结束一个文件系统操作
// 减少 outstanding 计数，如果为0则提交事务
void
end_op(void)
{
  int do_commit = 0;

  acquire(&log.lock);
  log.outstanding -= 1;
  
  if (log.committing)
    panic("log.committing");
  
  // 如果没有正在进行的操作了，准备提交
  if (log.outstanding == 0) {
    do_commit = 1;
    log.committing = 1;
  } else {
    // 唤醒可能在等待的其他操作
    wakeup_lock(&log);
  }
  release(&log.lock);

  // 在锁外执行提交（避免长时间持有锁）
  if (do_commit) {
    commit();
    acquire(&log.lock);
    log.committing = 0;
    wakeup_lock(&log);
    release(&log.lock);
  }
}

// 记录一个块的写操作到日志
// 调用者必须已经调用了 begin_op()
void
log_write(struct buf *b)
{
  int i;

  acquire(&log.lock);
  
  if (log.lh.n >= LOGBLOCKS || log.lh.n >= log.size - 1)
    panic("log_write: too big a transaction");
  if (log.outstanding < 1)
    panic("log_write: outside of trans");

  // 检查这个块是否已经在日志中
  for (i = 0; i < log.lh.n; i++) {
    if (log.lh.block[i] == b->blockno) {
      // 已存在，不需要重复添加
      break;
    }
  }
  
  // 如果是新块，添加到日志
  if (i == log.lh.n) {
    bpin(b);  // 防止块被驱逐出缓存
    log.lh.block[i] = b->blockno;
    log.lh.n++;
  }
  
  release(&log.lock);
}

// 获取日志统计信息（用于调试）
void
log_stats(void)
{
  acquire(&log.lock);
  printf("[LOG] Stats: outstanding=%d, logged_blocks=%d/%d, committing=%d\n",
         log.outstanding, log.lh.n, log.size, log.committing);
  release(&log.lock);
}

// 检查日志是否健康
int
log_check(void)
{
  int ok = 1;
  acquire(&log.lock);
  
  if (log.lh.n > LOGBLOCKS) {
    printf("[LOG] ERROR: log.lh.n (%d) > LOGBLOCKS (%d)\n", 
           log.lh.n, LOGBLOCKS);
    ok = 0;
  }
  
  if (log.outstanding < 0) {
    printf("[LOG] ERROR: log.outstanding (%d) < 0\n", log.outstanding);
    ok = 0;
  }
  
  release(&log.lock);
  return ok;
}
