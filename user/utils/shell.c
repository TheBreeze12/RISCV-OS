// 简单的Shell程序实现
#include "syscall.h"
#include "printf.h"

#define MAXLINE 256    // 最大命令行长度
#define MAXARGS 32     // 最大参数数量

// 字符串处理函数
static int strcmp(const char *p, const char *q) {
    while (*p && *p == *q)
        p++, q++;
    return (unsigned char)*p - (unsigned char)*q;
}

static int isspace(int c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

// 跳过空白字符
static char* skip_whitespace(char *s) {
    while (isspace(*s))
        s++;
    return s;
}

// 查找下一个空白字符
static char* find_whitespace(char *s) {
    while (*s && !isspace(*s))
        s++;
    return s;
}

// 解析命令行，将命令和参数分离
static int parse_line(char *buf, char **argv) {
    int argc = 0;
    char *p = buf;
    
    // 跳过前导空白
    p = skip_whitespace(p);
    
    while (*p && argc < MAXARGS - 1) {
        if (*p == '\n' || *p == '\0')
            break;
            
        argv[argc++] = p;
        p = find_whitespace(p);
        
        if (*p == '\0')
            break;
            
        *p++ = '\0';
        p = skip_whitespace(p);
    }
    
    argv[argc] = 0;  // 参数列表以NULL结尾
    return argc;
}

// 读取一行输入
static int read_line(char *buf, int maxlen) {
    int n = sys_read(0, buf, maxlen - 1);
    if (n < 0) {
        return -1;
    }
    buf[n] = '\0';
    return n;
}

// 内置命令：退出
static int cmd_exit(int argc, char **argv) {
    if (argc > 1) {
        printf("用法: exit\n");
        return 0;
    }
    printf("再见！\n");
    sys_exit(0);
    return 0;  // 不会到达这里
}

// 内置命令：帮助
static int cmd_help(int argc, char **argv) {
    printf("可用的内置命令:\n");
    printf("  exit    - 退出shell\n");
    printf("  help    - 显示此帮助信息\n");
    printf("  echo    - 回显参数\n");
    printf("  pid     - 显示当前进程ID\n");
    printf("  fork    - 测试fork功能\n");
    printf("  sleep   - 睡眠指定秒数\n");
    printf("  clear   - 清除屏幕\n");
    return 0;
}

// 内置命令：回显
static int cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        printf("%s", argv[i]);
        if (i < argc - 1) {
            printf(" ");
        }
    }
    printf("\n");
    return 0;
}

// 内置命令：显示进程ID
static int cmd_pid(int argc, char **argv) {
    int pid = sys_getpid();
    printf("当前进程ID: %d\n", pid);
    return 0;
}

// 内置命令：测试fork
static int cmd_fork(int argc, char **argv) {
    printf("执行fork测试...\n");
    int pid = sys_fork();
    if (pid == 0) {
        printf("这是子进程 (pid=%d)\n", sys_getpid());
        sys_sleep(2);
        printf("子进程退出\n");
        sys_exit(0);
    } else if (pid > 0) {
        printf("这是父进程 (pid=%d), 子进程pid=%d\n", sys_getpid(), pid);
        int status = sys_wait();
        printf("父进程等待子进程结束，返回值: %d\n", status);
    } else {
        printf("fork失败\n");
    }
    return 0;
}

// 内置命令：睡眠
static int cmd_sleep(int argc, char **argv) {
    if (argc < 2) {
        printf("用法: sleep <秒数>\n");
        return 1;
    }
    
    // 简单的字符串转整数
    int seconds = 0;
    char *p = argv[1];
    while (*p >= '0' && *p <= '9') {
        seconds = seconds * 10 + (*p - '0');
        p++;
    }
    
    if (*p != '\0') {
        printf("错误: 无效的数字: %s\n", argv[1]);
        return 1;
    }
    
    printf("睡眠 %d 秒...\n", seconds);
    sys_sleep(seconds);
    printf("醒来！\n");
    return 0;
}

static int cmd_clear(int argc, char **argv) {
    printf("\033[2J"); // 清除整个屏幕
    printf("\033[H");  // 光标回到左上角 (1,1)
    return 0;
}

// 执行内置命令
static int execute_builtin(int argc, char **argv) {
    if (argc == 0)
        return 0;
        
    if (strcmp(argv[0], "exit") == 0) {
        return cmd_exit(argc, argv);
    } else if (strcmp(argv[0], "help") == 0) {
        return cmd_help(argc, argv);
    } else if (strcmp(argv[0], "echo") == 0) {
        return cmd_echo(argc, argv);
    } else if (strcmp(argv[0], "pid") == 0) {
        return cmd_pid(argc, argv);
    } else if (strcmp(argv[0], "fork") == 0) {
        return cmd_fork(argc, argv);
    } else if (strcmp(argv[0], "sleep") == 0) {
        return cmd_sleep(argc, argv);
    } else if (strcmp(argv[0], "clear") == 0) {
        return cmd_clear(argc, argv);
    }
    
    return -1;  // 不是内置命令
}

// 执行外部程序（通过exec）
static int execute_external(int argc, char **argv) {
    // TODO: 当exec系统调用完全实现后，这里可以执行外部程序
    // 目前先返回错误
    printf("错误: 外部程序执行尚未实现\n");
    printf("提示: 当前只支持内置命令，使用 'help' 查看可用命令\n");
    return 1;
}

// 执行命令
static int execute(int argc, char **argv) {
    if (argc == 0)
        return 0;
    
    // 先尝试内置命令
    int ret = execute_builtin(argc, argv);
    if (ret != -1) {
        return ret;
    }
    
    // 如果不是内置命令，尝试执行外部程序
    return execute_external(argc, argv);
}

// Shell主循环
void shell_main(void) {
    char buf[MAXLINE];
    char *argv[MAXARGS];
    
    printf("=== RISC-V OS Shell ===\n");
    printf("输入 'help' 查看可用命令\n");
    printf("输入 'exit' 退出\n\n");
    
    while (1) {
        // 显示提示符
        printf("$ ");
        
        // 读取命令行
        if (read_line(buf, MAXLINE) < 0) {
            printf("\n读取输入失败，退出\n");
            break;
        }
        
        // 跳过空行
        char *p = skip_whitespace(buf);
        if (*p == '\0' || *p == '\n') {
            continue;
        }
        
        // 解析命令行
        int argc = parse_line(buf, argv);
        if (argc == 0) {
            continue;
        }
        
        // 执行命令
        execute(argc, argv);
    }
}

