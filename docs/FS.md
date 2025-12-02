# RISC-V OS 文件系统实现报告

## 一、文件系统整体架构

文件系统采用分层设计，共分为五层：

```
┌─────────────────────────────────────┐
│  系统调用层 (syscall.c)              │  ← 用户接口
├─────────────────────────────────────┤
│  文件描述符层 (file.c)               │  ← 文件抽象
├─────────────────────────────────────┤
│  路径名解析层 (namei.c, fs.c)        │  ← 路径查找
├─────────────────────────────────────┤
│  Inode层 (fs.c)                      │  ← 文件元数据
├─────────────────────────────────────┤
│  日志层 (log.c)                      │  ← 事务保证
├─────────────────────────────────────┤
│  块缓存层 (bio.c)                    │  ← 缓存管理
├─────────────────────────────────────┤
│  磁盘驱动层 (virtio_disk.c)          │  ← 硬件接口
└─────────────────────────────────────┘
```

## 二、磁盘布局

磁盘布局（从低地址到高地址）：

1. **Boot Block** (块0)：引导代码
2. **Super Block** (块1)：文件系统元数据
3. **Log Blocks**：事务日志区
4. **Inode Blocks**：inode表
5. **Bitmap Blocks**：空闲块位图
6. **Data Blocks**：数据块

### Super Block 结构

```c
struct superblock {
  uint magic;        // Must be FSMAGIC (0x10203040)
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes
  uint nlog;         // Number of log blocks
  uint logstart;     // Block number of first log block
  uint inodestart;   // Block number of first inode block
  uint bmapstart;    // Block number of first free map block
};
```

## 三、各文件功能详解

### 1. `fs.h` - 文件系统格式定义

**功能：**
- 定义磁盘数据结构（superblock、dinode、dirent）
- 定义常量（块大小 BSIZE=1024、目录项大小 DIRSIZ=14 等）
- 提供磁盘布局宏定义

**关键结构：**

#### On-disk Inode 结构
```c
struct dinode {
  short type;           // File type (T_DIR, T_FILE, T_DEVICE)
  short major;          // Major device number (T_DEVICE only)
  short minor;          // Minor device number (T_DEVICE only)
  short nlink;          // Number of links to inode
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+1]; // Data block addresses (12 direct + 1 indirect)
};
```

#### 目录项结构
```c
struct dirent {
  ushort inum;          // Inode number
  char name[DIRSIZ];    // File name (最多14字符)
};
```

### 2. `buf.h` - 缓冲区结构定义

**功能：**
- 定义块缓存缓冲区结构
- 支持 LRU 缓存管理（通过 prev/next 指针）

**关键结构：**
```c
struct buf {
    int valid;          // 数据是否已从磁盘读取
    int disk;           // 磁盘是否"拥有"此缓冲区
    uint dev;           // 设备号
    uint blockno;       // 块号
    struct sleeplock lock; // 睡眠锁
    uint refcnt;        // 引用计数
    struct buf *prev;    // LRU 缓存列表前驱
    struct buf *next;    // LRU 缓存列表后继
    uchar data[BSIZE];  // 数据缓冲区 (1024字节)
};
```

### 3. `bio.c` - 块缓存层（Buffer Cache）

**功能：**
- 管理磁盘块的缓存，减少磁盘 I/O
- 提供统一的块读写接口
- 实现引用计数和锁机制
- 提供同步点，防止多进程同时修改同一块

**核心函数：**

#### `bread()` - 读取块
```c
struct buf* bread(uint dev, uint blockno)
```
- 从缓存中查找块
- 如果缓存未命中，从磁盘读取
- 返回锁定的缓冲区

#### `bwrite()` - 写回块
```c
void bwrite(struct buf *b)
```
- 将缓冲区内容写回磁盘
- 必须持有缓冲区锁

#### `brelse()` - 释放缓冲区
```c
void brelse(struct buf *b)
```
- 释放缓冲区锁
- 减少引用计数

#### `bpin()` / `bunpin()` - 引用计数管理
- 日志系统使用，防止块在事务期间被释放

### 4. `log.c` - 事务日志系统

**功能：**
- 实现 Write-Ahead Logging (WAL) 机制
- 保证文件系统操作的原子性
- 支持崩溃恢复

**核心结构：**
```c
struct log {
  struct spinlock lock;
  int start;                  // 日志区的起始块号
  int size;                   // 日志区的大小（块数）
  int outstanding;            // 当前正在进行的文件系统操作数量
  int committing;             // 是否正在提交
  int dev;                    // 设备号
  struct logheader lh;        // 日志头
};
```

**事务提交流程：**

1. **`begin_op()`** - 开始文件系统操作
   - 增加 `outstanding` 计数
   - 检查日志空间是否足够
   - 如果空间不足，等待其他事务完成

2. **`log_write()`** - 记录块的写操作
   - 将块添加到日志中
   - 调用 `bpin()` 防止块被释放
   - 如果块已在日志中，不重复添加

3. **`end_op()`** - 结束操作
   - 减少 `outstanding` 计数
   - 如果没有其他操作，调用 `commit()` 提交事务

4. **`commit()`** - 提交事务（4个步骤）
   ```c
   write_log();      // 1. 写日志内容到磁盘
   write_head();     // 2. 写日志头（提交点）
   install_trans();  // 3. 将日志内容写入实际位置
   write_head();     // 4. 清空日志头（事务完成）
   ```

5. **`recover_from_log()`** - 崩溃恢复
   - 系统启动时调用
   - 检查是否有未完成的事务
   - 如果有，恢复日志中的数据

**优势：**
- **原子性**：多步操作要么全部成功，要么全部回滚
- **崩溃恢复**：系统启动时自动恢复未完成的事务
- **性能优化**：批量提交减少磁盘 I/O
- **并发控制**：多个操作可以共享一个事务

### 5. `fs.c` - 文件系统核心实现

**功能：**
- Inode 管理（分配、释放、读写）
- 目录操作（查找、创建链接）
- 路径名解析
- 块分配与释放

#### 5.1 块分配器

**`balloc()`** - 分配数据块
```c
static uint balloc(uint dev)
```
- 扫描位图查找空闲块
- 标记块为已使用
- 调用 `log_write()` 记录到日志
- 清零新分配的块

**`bfree()`** - 释放数据块
```c
static void bfree(int dev, uint b)
```
- 在位图中标记块为空闲
- 调用 `log_write()` 记录到日志

#### 5.2 Inode 管理

**Inode 表结构：**
```c
struct {
  struct spinlock lock;
  struct inode inode[NINODE];  // 内存中的 inode 表
} itable;
```

**核心函数：**

- **`ialloc()`** - 分配 inode
  - 扫描 inode 块查找空闲 inode
  - 设置 inode 类型
  - 调用 `log_write()` 记录

- **`iget()`** - 获取 inode（引用计数管理）
  - 在 inode 表中查找
  - 如果找到，增加引用计数
  - 如果未找到，分配新槽位

- **`ilock()`** - 锁定 inode
  - 获取睡眠锁
  - 如果 inode 无效，从磁盘读取

- **`iunlock()`** - 解锁 inode
  - 释放睡眠锁

- **`iupdate()`** - 更新 inode 到磁盘
  - 将内存中的 inode 写回磁盘
  - 调用 `log_write()` 记录

- **`iput()`** - 释放 inode 引用
  - 减少引用计数
  - 如果引用为0且链接为0，释放 inode 和其数据块

#### 5.3 文件读写

**`readi()`** - 从 inode 读取数据
```c
int readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n)
```
- 使用 `bmap()` 获取数据块地址
- 调用 `bread()` 读取块
- 使用 `copyout()` 复制到用户空间

**`writei()`** - 向 inode 写入数据
```c
int writei(struct inode *ip, int user_src, uint64 src, uint off, uint n)
```
- 使用 `bmap()` 获取/分配数据块
- 调用 `bread()` 读取块
- 使用 `copyin()` 从用户空间复制数据
- 调用 `log_write()` 记录修改
- 更新 inode 大小

#### 5.4 目录操作

**`dirlookup()`** - 在目录中查找文件
```c
struct inode* dirlookup(struct inode *dp, char *name, uint *poff)
```
- 遍历目录项
- 比较文件名
- 返回对应的 inode

**`dirlink()`** - 在目录中创建链接
```c
int dirlink(struct inode *dp, char *name, uint inum)
```
- 查找空闲目录项
- 设置 inode 号和文件名
- 调用 `writei()` 写入目录

#### 5.5 路径名解析

**`namei()`** - 解析路径名
```c
struct inode* namei(char *path)
```
- 调用 `namex()` 进行实际解析

**`namex()`** - 路径名解析实现
```c
static struct inode* namex(char *path, int nameiparent, char *name)
```
- 从根目录或当前目录开始
- 逐级解析路径元素
- 使用 `dirlookup()` 查找每个组件
- 返回目标 inode

**`create()`** - 创建文件/目录
```c
struct inode* create(char *path, short type, short major, short minor)
```
- 解析父目录
- 分配新 inode
- 在父目录中创建链接
- 如果是目录，创建 `.` 和 `..` 条目

### 6. `file.c` - 文件描述符层

**功能：**
- 管理文件描述符表
- 提供文件读写接口
- 处理设备文件

**文件表结构：**
```c
struct {
  struct spinlock lock;
  struct file file[NFILE];  // 系统级文件表
} ftable;
```

**文件结构：**
```c
struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE } type;
  int ref;              // 引用计数
  char readable;        // 是否可读
  char writable;        // 是否可写
  struct pipe *pipe;    // FD_PIPE 类型
  struct inode *ip;     // FD_INODE 和 FD_DEVICE 类型
  uint off;             // FD_INODE 类型的文件偏移
  short major;          // FD_DEVICE 类型的主设备号
};
```

**核心函数：**

- **`filealloc()`** - 分配文件结构
  - 在文件表中查找空闲槽位
  - 初始化文件结构

- **`filedup()`** - 增加文件引用计数
  - 用于 `fork()` 和 `dup()`

- **`fileclose()`** - 关闭文件
  - 减少引用计数
  - 如果引用为0，释放 inode

- **`fileread()`** - 从文件读取
  - 处理设备文件和普通文件
  - 调用 `readi()` 或设备驱动

- **`filewrite()`** - 向文件写入
  - 分段写入，避免超过日志事务大小限制
  - 每次写入调用 `begin_op()` 和 `end_op()`
  - 调用 `writei()` 或设备驱动

- **`filestat()`** - 获取文件状态
  - 调用 `stati()` 获取 inode 信息
  - 复制到用户空间

### 7. `namei.c` - 路径名解析

**当前状态：** 文件为空（路径解析在 `fs.c` 的 `namex()` 中实现）

### 8. `virtio_disk.c` - 磁盘驱动

**功能：**
- 实现 VirtIO 块设备驱动
- 处理磁盘 I/O 请求
- 管理 DMA 描述符

**VirtIO 队列结构：**
```c
static struct disk {
  struct virtq_desc *desc;    // 描述符表
  struct virtq_avail *avail;  // 可用环（驱动->设备）
  struct virtq_used *used;    // 已用环（设备->驱动）
  char free[NUM];             // 描述符空闲标记
  uint16 used_idx;            // 已用索引
  struct {
    struct buf *b;
    char status;
  } info[NUM];                // 操作信息
  struct virtio_blk_req ops[NUM]; // 命令头
  struct spinlock vdisk_lock;
} disk;
```

**核心函数：**

- **`virtio_disk_init()`** - 初始化 VirtIO 设备
  - 检查设备存在性
  - 协商特性
  - 设置队列
  - 分配描述符内存

- **`virtio_disk_rw()`** - 执行磁盘读写
  - 分配3个描述符（命令、数据、状态）
  - 设置 DMA 地址
  - 提交到可用环
  - 通知设备
  - 等待完成

- **`virtio_disk_intr()`** - 处理磁盘中断
  - 检查完成的操作
  - 唤醒等待的进程

### 9. `virtio.h` - VirtIO 协议定义

**功能：**
- 定义 VirtIO MMIO 寄存器地址
- 定义 VirtIO 数据结构（描述符、队列等）
- 定义设备特性标志

**关键定义：**
- MMIO 寄存器偏移（MAGIC_VALUE, VERSION, DEVICE_ID 等）
- 状态寄存器位（ACKNOWLEDGE, DRIVER, DRIVER_OK 等）
- 描述符标志（NEXT, WRITE）
- 块设备命令类型（IN, OUT）

### 10. `stat.h` - 文件状态结构

**功能：**
- 定义文件状态结构（用于 `fstat` 系统调用）

```c
struct stat {
  int dev;     // File system's disk device
  uint ino;    // Inode number
  short type;  // Type of file (T_DIR, T_FILE, T_DEVICE)
  short nlink; // Number of links to file
  uint64 size; // Size of file in bytes
};
```

## 四、文件之间的协作关系

### 4.1 数据流图

```
用户程序
   ↓ sys_write()
系统调用层 (syscall.c)
   ↓ filewrite()
文件描述符层 (file.c)
   ↓ begin_op() → log_write() → writei()
日志层 (log.c) ← → Inode层 (fs.c)
   ↓ bwrite()
块缓存层 (bio.c)
   ↓ virtio_disk_rw()
磁盘驱动层 (virtio_disk.c)
   ↓ DMA操作
硬件磁盘
```

### 4.2 写文件操作流程

1. **系统调用层**：`sys_write()` 接收用户请求
2. **文件描述符层**：`filewrite()` 检查权限和类型
3. **日志层**：`begin_op()` 开始事务
4. **Inode 层**：`ilock()` 锁定 inode
5. **Inode 层**：`writei()` 写入数据
   - `bmap()` 获取/分配数据块
   - `balloc()` 分配新块（如需要）
   - `log_write()` 记录到日志
   - `bread()` 读取块
   - 修改数据
6. **Inode 层**：`iupdate()` 更新 inode 元数据
7. **Inode 层**：`iunlock()` 解锁 inode
8. **日志层**：`end_op()` 提交事务
   - `commit()` 执行提交
   - `write_log()` 写日志内容
   - `write_head()` 写日志头
   - `install_trans()` 应用到实际位置

### 4.3 读文件操作流程

1. **系统调用层**：`sys_read()` 接收用户请求
2. **文件描述符层**：`fileread()` 检查权限
3. **Inode 层**：`ilock()` 锁定 inode
4. **Inode 层**：`readi()` 读取数据
   - `bmap()` 获取数据块地址
   - `bread()` 从缓存读取块
   - `copyout()` 复制到用户空间
5. **Inode 层**：`iunlock()` 解锁 inode

### 4.4 关键协作点

#### 日志系统与块缓存
```c
log_write(bp);  // 记录块到日志，调用 bpin() 防止块被释放
brelse(bp);      // 释放缓冲区
```
- `log_write()` 记录块到日志，并调用 `bpin()` 防止块被释放
- 事务提交后调用 `bunpin()` 释放引用

#### 块分配与日志
```c
log_write(bp);   // 记录位图修改
brelse(bp);
bzero(dev, b + bi);  // 清零新分配的块
```
- 分配块时先标记位图，记录到日志
- 然后清零新分配的块

#### 文件关闭与 Inode 释放
```c
begin_op();
iput(ff.ip);  // 可能触发 inode 和块的释放
end_op();
```
- 文件关闭时在事务中释放 inode
- `iput()` 可能触发 inode 和块的释放

## 五、同步机制

文件系统使用多层锁机制保证并发安全：

1. **块缓存锁**：`bcache.lock`（自旋锁）
   - 保护块缓存表
   - 保护引用计数

2. **Inode 表锁**：`itable.lock`（自旋锁）
   - 保护 inode 表
   - 保护 inode 分配

3. **Inode 锁**：`ip->lock`（睡眠锁）
   - 保护单个 inode 的数据
   - 允许长时间持有

4. **缓冲区锁**：`b->lock`（睡眠锁）
   - 保护单个缓冲区的数据
   - 允许磁盘 I/O 期间持有

5. **日志锁**：`log.lock`（自旋锁）
   - 保护日志结构
   - 保护事务计数

6. **文件表锁**：`ftable.lock`（自旋锁）
   - 保护文件表
   - 保护文件引用计数

7. **磁盘锁**：`disk.vdisk_lock`（自旋锁）
   - 保护 VirtIO 磁盘操作
   - 保护描述符分配

## 六、事务日志的优势

1. **原子性**：多步操作要么全部成功，要么全部回滚
   - 例如：创建文件需要分配 inode、更新目录、更新位图
   - 如果中途崩溃，所有操作都会回滚

2. **崩溃恢复**：系统启动时自动恢复未完成的事务
   - 检查日志头
   - 如果有未完成的事务，重新应用

3. **性能优化**：批量提交减少磁盘 I/O
   - 多个操作共享一个事务
   - 减少磁盘寻道时间

4. **并发控制**：多个操作可以共享一个事务
   - `outstanding` 计数管理
   - 当日志空间不足时自动等待

## 七、Inode 结构

### 内存中的 Inode
```c
struct inode {
  uint dev;           // 设备号
  uint inum;          // Inode 号
  int ref;            // 引用计数
  struct sleeplock lock; // 保护以下字段
  int valid;          // inode 是否已从磁盘读取
  
  // 从磁盘 inode 复制的字段
  short type;         // 文件类型
  short major;        // 主设备号
  short minor;        // 次设备号
  short nlink;        // 链接数
  uint size;          // 文件大小
  uint addrs[NDIRECT+1]; // 数据块地址
};
```

### 磁盘上的 Inode
```c
struct dinode {
  short type;
  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT+1];
};
```

### 数据块寻址
- **直接块**：前 12 个块使用直接寻址（`addrs[0..11]`）
- **间接块**：第 13 个块是间接块（`addrs[12]`），指向一个包含 256 个块地址的块
- **最大文件大小**：12 * 1KB + 256 * 1KB = 268KB

## 八、目录实现

目录是特殊的文件，包含目录项序列：

```c
struct dirent {
  ushort inum;        // Inode 号
  char name[DIRSIZ];  // 文件名（最多14字符）
};
```

目录操作：
- **查找**：遍历目录项，比较文件名
- **创建**：查找空闲目录项，设置 inode 号和文件名
- **删除**：将目录项的 inum 设为 0

## 九、总结

该文件系统实现了：

**完整的文件系统抽象**
- 文件、目录、设备文件支持
- 标准的路径名解析
- 文件描述符机制

**高效的块缓存机制**
- 减少磁盘 I/O
- 提供同步点
- LRU 缓存管理

**可靠的事务日志系统**
- Write-Ahead Logging
- 原子性保证
- 崩溃恢复

**设备文件支持**
- 统一的设备接口
- 控制台设备集成

**清晰的层次结构**
- 各层职责明确
- 统一的接口设计
- 良好的模块化

各层通过统一的接口协作，实现了可靠且高效的文件系统，为操作系统提供了稳定的存储抽象。

