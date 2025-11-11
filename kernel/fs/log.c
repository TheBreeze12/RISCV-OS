// Simplified logging - MVP version with no actual logging
#include "../include/type.h"
#include "../include/def.h"
#include "../include/param.h"
#include "../utils/spinlock.h"
#include "fs.h"
#include "buf.h"

// MVP: Minimal log structure (not actually used)
struct logheader {
  int n;
  int block[LOGBLOCKS];
};

struct fslog {
  struct spinlock lock;
  int dev;
  struct logheader lh;
};
static struct fslog fslog;

// Simplified: No logging, just empty functions
void
begin_op(void)
{
  // MVP: No logging, do nothing
}

void
end_op(void)
{
  // MVP: No logging, do nothing
}

void
initlog(int dev, struct superblock *sb)
{
  // MVP: No logging, just initialize lock
  initlock(&fslog.lock, "log");
  fslog.dev = dev;
}

// Simplified: log_write() directly writes to disk
void
log_write(struct buf *b)
{
  // MVP: No logging, write directly to disk
  bwrite(b);
}
