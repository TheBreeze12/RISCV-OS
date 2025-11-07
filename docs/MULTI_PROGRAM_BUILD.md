# 多程序构建指南

## 概述

系统现在支持同时构建多个用户程序，并将它们全部加入到文件系统镜像中。

## 添加新程序

### 1. 创建程序源文件

在 `user/` 目录下创建新的 `.c` 文件，例如 `user/hello.c`:

```c
#include "syscall.h"
#include "printf.h"

void main(void) {
    printf("Hello, World!\n");
    sys_exit(0);
}
```

### 2. 添加到构建列表

在 `Makefile` 中，找到 `USER_PROGRAMS` 变量，添加新程序名：

```makefile
USER_PROGRAMS = init hello myprogram
```

### 3. 构建

```bash
# 构建所有用户程序
make user-programs

# 构建文件系统镜像（包含所有程序）
make fsimg

# 或者一次性构建所有内容
make all
```

## 程序类型

### 独立程序

只需要 `printf` 等基础库的程序（如 `hello`）：

```makefile
user/hello.elf: user/hello.c user/printf.o $(USER_INCS)
	$(USER_CC) $(USER_CFLAGS) -c user/hello.c -o user/hello.o
	$(LD) $(USER_LDFLAGS) -o $@ user/hello.o user/printf.o
```

### 复杂程序

需要多个库的程序（如 `init`，需要 shell、scanf 等）：

```makefile
user/init.elf: user/init.c $(USER_COMMON_OBJS) $(USER_INCS)
	$(USER_CC) $(USER_CFLAGS) -c user/init.c -o user/init.o
	$(LD) $(USER_LDFLAGS) -o $@ user/init.o $(USER_COMMON_OBJS)
```

### 通用规则

对于简单的单文件程序，Makefile 提供了通用规则，会自动链接 `printf.o`。

## 文件系统镜像

所有在 `USER_PROGRAMS` 中列出的程序都会被自动加入到文件系统镜像中：

```bash
$ make fsimg
构建文件系统镜像，包含以下程序:
  - user/init.elf
  - user/hello.elf
文件系统镜像创建完成: fs.img
```

## 在Shell中使用

构建完成后，可以在shell中执行这些程序：

```bash
$ /init      # 执行init程序
$ /hello     # 执行hello程序
```

## 当前支持的程序

- `init`: Shell程序（需要 shell、scanf、printf）
- `hello`: Hello World程序（只需要 printf）

## 添加依赖

如果新程序需要额外的库文件：

1. **添加库源文件**：将 `.c` 文件放在 `user/` 目录
2. **添加到 USER_COMMON_SRCS**（如果多个程序需要）：
   ```makefile
   USER_COMMON_SRCS = user/printf.c user/scanf.c user/shell.c user/mylib.c
   ```
3. **在程序构建规则中链接**：
   ```makefile
   user/myprogram.elf: user/myprogram.c user/printf.o user/mylib.o $(USER_INCS)
       $(USER_CC) $(USER_CFLAGS) -c user/myprogram.c -o user/myprogram.o
       $(LD) $(USER_LDFLAGS) -o $@ user/myprogram.o user/printf.o user/mylib.o
   ```

## 清理

```bash
# 清理所有构建文件（包括用户程序）
make clean-all

# 只清理内核文件
make clean
```

## 示例：添加新程序

1. 创建 `user/test.c`:
```c
#include "syscall.h"
#include "printf.h"

void main(void) {
    printf("Test program\n");
    sys_exit(0);
}
```

2. 在 Makefile 中添加：
```makefile
USER_PROGRAMS = init hello test
```

3. 构建：
```bash
make all
```

4. 在shell中执行：
```bash
$ /test
Test program
```

