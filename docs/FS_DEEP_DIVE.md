# RISC-V OS 文件系统深度讲解（结合本项目代码）

> 目标：你可以把这篇文档当成“从 0 到 1 的 FS 复盘手册”。
> 读完后应能回答：
> 1) 磁盘上到底放了什么；2) 一个 `open/read/write/close` 如何走到磁盘；3) 日志如何保证崩溃一致性；4) 这个项目 FS 当前有哪些简化与风险。

---

## 1. 文件系统全景图

本项目的 FS 可以分成 6 层：

1. 磁盘布局层（superblock + inode 区 + bitmap + data 区）
2. 块缓存层（buffer cache，块级缓存和并发控制）
3. 日志层（WAL 风格事务，保证多步更新原子性）
4. inode 层（文件元数据、块映射、读写）
5. 目录/路径层（dirent、namei、create 等）
6. 系统调用层（fd 管理、open/read/write/unlink/mkdir）

对应源码：

- 磁盘格式定义：`kernel/fs/fs.h`
- 镜像构建：`tools/mkfs.c`
- 缓存：`kernel/fs/bio.c`
- 日志：`kernel/fs/log.c`
- inode + 目录 + 路径：`kernel/fs/fs.c`
- fd 与 file 抽象：`kernel/fs/file.c`, `kernel/fs/file.h`
- syscall 到 FS：`kernel/trap/syscall.c`
- 块设备驱动：`kernel/fs/virtio_disk.c`

---

## 2. 先看“磁盘长什么样”

### 2.1 布局

在 `kernel/fs/fs.h` 里定义了磁盘布局：

```c
// [ boot block | super block | log | inode blocks | free bit map | data blocks ]
```

关键结构是 `struct superblock`：

- `size`：文件系统总块数
- `nblocks`：数据块数量
- `ninodes`：inode 数量
- `nlog`：日志块数量
- `logstart/inodestart/bmapstart`：各区域起始块号

### 2.2 常量含义

- `BSIZE=1024`：一个块 1KB
- `NDIRECT=12`：inode 里直接块指针个数
- `NINDIRECT=BSIZE/sizeof(uint)`：一级间接块可索引数量
- `MAXFILE=NDIRECT+NINDIRECT`

`struct dinode` 中 `addrs[NDIRECT+1]` 的最后一个槽位通常用于间接块指针。

### 2.3 镜像是怎么“烤”出来的

`tools/mkfs.c` 做了三件关键事：

1. 计算 superblock 参数并写入第 1 块。
2. 创建根目录 inode（`ROOTINO=1`），写入 `.` 和 `..`。
3. 把用户程序（`user/_xxx`）拷入数据块，建立根目录项。

要点：`mkfs` 的 `iappend()` 支持 direct + indirect 两级寻址（它会使用 `din.addrs[NDIRECT]`）。

---

## 3. 内核启动后如何挂载 FS

挂载入口在 `kernel/fs/fs.c` 的 `fsinit()`：

1. `readsb()` 读取 superblock（块 1）。
2. 校验 `FSMAGIC`。
3. `initlog()` 初始化日志并执行恢复。
4. `ireclaim()`（当前实现是简化版占位）。

也就是说：系统启动后，第一件事不是直接接收文件操作，而是先确保日志恢复完成，避免使用脏状态文件系统。

---

## 4. 块缓存层（bio.c）

### 4.1 你可以把 buffer cache 理解成什么

buffer cache 是“磁盘块的内存镜像池”：

- `bread(dev, blockno)`：拿到某块的缓存副本（如果无效则从磁盘读）
- `bwrite(b)`：把缓存块写回磁盘
- `brelse(b)`：释放块锁、减少引用

`struct buf` 定义在 `kernel/fs/buf.h`，里面有：

- `valid`：缓存数据是否已从磁盘载入
- `dev/blockno`：对应哪块
- `lock`：sleep lock，保护块内容
- `refcnt`：引用计数
- `disk`：是否有进行中的磁盘请求

### 4.2 本项目实现特点

`kernel/fs/bio.c` 注释写了 MVP 简化：

- 线性扫描查找块
- 无 LRU 双链表管理
- 无高级替换策略

优点：代码简单、容易教学。
缺点：块多、并发高时命中率和性能不稳定，可能出现 `bget: no buffers`。

---

## 5. 日志层（log.c）：事务一致性的核心

### 5.1 为什么要日志

一个“写文件”不是单次写盘，可能涉及：

- 修改 bitmap（分配块）
- 修改 inode（size/addrs）
- 修改数据块内容

若中途宕机，磁盘会出现“写了一半”的不一致状态。

### 5.2 本项目事务流程

`kernel/fs/log.c` 的核心调用链：

1. `begin_op()`：申请事务配额，避免日志空间不够。
2. 业务代码多次 `log_write(bp)`：登记被修改的块号（并 `bpin` 防驱逐）。
3. `end_op()`：减少 outstanding 计数；若归零，触发 `commit()`。
4. `commit()` 四步：
   - `write_log()`：把修改块拷贝到日志区
   - `write_head()`：写日志头（这是提交点）
   - `install_trans()`：把日志块安装到真实块
   - 清空日志头并写回

### 5.3 崩溃恢复

开机 `initlog()` 会 `recover_from_log()`：

- 若日志头 `n>0`，说明上次提交未完全安装。
- 执行 `install_trans(1)` 重放后清空头。

这保证了“要么全做完，要么不算做”。

---

## 6. inode 层（fs.c）

### 6.1 inode 生命周期

常见路径：

- `ialloc()`：在 inode 区找空闲 `dinode` 分配。
- `iget()`：把 `(dev, inum)` 关联到内存 inode 缓存项。
- `ilock()`：必要时从盘读入 inode 元数据并加锁。
- `iupdate()`：把内存 inode 回写到磁盘。
- `iput()`：减少引用，必要时回收（`nlink==0 && ref==1` 时 `itrunc()` + free）。

注意：

- `iget()` 不加 inode 睡眠锁，只做“引用持有”。
- 真正读写元数据要 `ilock()`。
- 这种分离是为了避免路径解析过程中的死锁与长锁持有。

### 6.2 数据块映射 bmap

本项目 `bmap()` 明确做了简化：只支持 direct 块。

```c
if (bn < NDIRECT) { ... } else panic(...)
```

含义：一个文件最大仅 `NDIRECT * BSIZE = 12 * 1024 = 12288 bytes`。

这与 `fs.h` / `mkfs.c` 中“支持 indirect”存在语义差异。
在当前实验规模下可能够用，但它是重要限制点。

### 6.3 readi/writei

- `readi()`：按块读取，支持 copyout 到用户空间。
- `writei()`：按块写入，必要时通过 `bmap()` 分配新块；写完 `iupdate()`。

`writei()` 里也有边界：

- `off + n > NDIRECT*BSIZE` 直接失败（因为只支持 direct）。

---

## 7. 目录与路径层（fs.c）

### 7.1 目录其实是“特殊文件”

目录数据由 `struct dirent` 数组组成：

- `inum`：目标 inode 号
- `name[DIRSIZ]`：文件名

`dirlookup()` 线性扫描目录文件内容查名字，`dirlink()` 负责新增目录项。

### 7.2 路径解析 namex

`skipelem()` 每次切一个 path token，`namex()` 循环：

1. 当前 inode 必须是目录
2. 在目录里 `dirlookup(next)`
3. 继续走下一段

对外导出：

- `namei(path)`：返回最终 inode
- `nameiparent(path, name)`：返回父目录 inode，并拿到末段名

### 7.3 create

`create(path, type, major, minor)`：

1. 找父目录
2. 若同名已存在：普通文件打开复用或失败
3. 分配新 inode，写初始元数据
4. 如果是目录，补 `.` 和 `..`
5. 在父目录写入目录项

失败路径会回滚 `nlink` 并释放，避免泄漏。

---

## 8. file 抽象与 fd 层（file.c + syscall.c）

### 8.1 两层对象

- `struct file`：进程打开文件后的“打开实例”（读写权限、偏移量 off、引用计数）
- `struct inode`：文件本体元数据（全局共享）

所以多个 fd 可以指向同一 inode，但各自 off 可不同。

### 8.2 关键接口

`kernel/fs/file.c`：

- `filealloc/filedup/fileclose`：打开实例生命周期
- `fileread/filewrite`：转到 inode 或设备读写
- `filestat`：读元数据

特别是 `filewrite()`：

- 会把大写入拆成多段（每段受日志容量约束）
- 每段包一层 `begin_op() ... end_op()`

这避免单次事务超过日志上限。

### 8.3 syscall 到 FS 的落地

`kernel/trap/syscall.c`：

- `sys_open`：`begin_op` -> `namei/create` -> `filealloc+fdalloc` -> 设置 file
- `sys_read`：经 `fileread` 进入 `readi`
- `sys_write`：经 `filewrite` 进入 `writei`
- `sys_unlink`：`nameiparent + dirlookup`，减 nlink 并清空目录项
- `sys_mkdir`：`create(path, T_DIR, ...)`

你可以把这看成：

用户 API -> fd 层 -> inode/目录层 -> 缓存层 -> virtio 块设备

---

## 9. 最底层：virtio 块设备（virtio_disk.c）

`kernel/fs/virtio_disk.c` 负责把“读写某个 block”变成 virtio 队列请求：

1. 组 3 个 descriptor（请求头 / 数据 / 状态字节）
2. 放入 avail ring，通知设备
3. 睡眠等待中断完成
4. 中断处理 `virtio_disk_intr()` 唤醒等待者

对上层来说，接口只有 `virtio_disk_rw(struct buf *b, int write)`，非常干净。

---

## 10. 一条完整数据流：write("hello")

以用户程序 `write(fd, "hello", 5)` 为例：

1. 触发陷入，进入 `sys_write`（syscall 层）。
2. 找到 `ofile[fd]` 对应 `struct file`。
3. `filewrite()` 按日志约束切片。
4. 每片执行：
   - `begin_op()` 开事务
   - `ilock(ip)`
   - `writei()`：
     - `bmap()` 找/分配数据块
     - `bread()` 拿缓存块
     - 拷数据到 `bp->data`
     - `log_write(bp)` 登记脏块
   - 更新 `ip->size/off` 并 `iupdate()`
   - `iunlock(ip)`
   - `end_op()`（可能触发 commit）
5. commit 时日志写盘并安装到真实块。

关键点：数据不是“直接写真实块并立即生效”，而是先进入日志协议管理。

---

## 11. 当前实现的简化点与潜在风险

### 11.1 已知简化

1. `bmap()` 只支持 direct block。
2. `bio.c` 没有 LRU 等替换策略。
3. `ireclaim()` 当前是占位实现。
4. `kernel/fs/namei.c` 是旧注释文件，真实路径实现在 `kernel/fs/fs.c`。

### 11.2 值得注意的不一致

- `mkfs.c` 允许构建含 indirect 数据的文件。
- 运行时内核 `bmap()` 却不支持 indirect。

当文件超过 direct 上限时，运行期访问可能失败或 panic。

### 11.3 一致性边界

日志保证的是“事务内元数据/块更新一致性”，但它不替代：

- 更高层语义校验（例如目录循环、复杂崩溃场景一致性检查）
- 配额、权限、并发可扩展性

---

## 12. 建议学习顺序（最快吃透）

按下面顺序读源码，效率最高：

1. `kernel/fs/fs.h`（先理解磁盘格式）
2. `tools/mkfs.c`（知道镜像如何生成）
3. `kernel/fs/bio.c`（理解块缓存）
4. `kernel/fs/log.c`（理解事务）
5. `kernel/fs/fs.c`（inode+目录+路径核心）
6. `kernel/fs/file.c`（fd 到 inode 桥接）
7. `kernel/trap/syscall.c`（用户接口如何落地）
8. `kernel/fs/virtio_disk.c`（到底如何和设备交互）

---

## 13. 可选进阶改造方向（你后续可做项目亮点）

1. 补齐 indirect block（`bmap/itrunc/readi/writei` 全链路支持大文件）。
2. 把 `bio` 改成哈希 + LRU 双链表，减少线性扫描。
3. 增加 `fsck` 风格一致性检查工具（inode、bitmap、目录树）。
4. 增强日志：支持更细粒度统计、故障注入测试、重放验证脚本。
5. 实现多级目录与并发压测基准，形成可量化性能报告。

---

## 14. 一句话总结

这个 FS 的核心价值在于：结构完整、教学路径清晰（缓存 + 日志 + inode + 目录 + syscall 全链路齐全），并且已经具备“可恢复”的基本一致性机制；当前主要短板是大文件能力和缓存策略，正好也是最适合继续打磨成高质量项目亮点的方向。
