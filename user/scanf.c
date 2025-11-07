// 用户空间的 scanf 实现
#include "syscall.h"

// 如果编译器支持 stdarg.h
#ifdef __STDC_VERSION__
#include <stdarg.h>
#else
// 如果没有 stdarg.h，定义基本的 va_list 类型
typedef char *va_list;
#define va_start(ap, last) ((ap) = (va_list)&(last) + sizeof(last))
#define va_arg(ap, type) (*(type *)((ap) += sizeof(type), (ap) - sizeof(type)))
#define va_end(ap) ((void)0)
#endif

// 判断字符是否为空白字符
static int isspace(int c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v');
}

// 判断字符是否为数字
static int isdigit(int c) {
    return (c >= '0' && c <= '9');
}

// 跳过空白字符
static void skip_whitespace(char **s) {
    while (**s && isspace(**s)) {
        (*s)++;
    }
}

// 读取一行输入
static int read_line(char *buf, int maxlen) {
    int n = sys_read(0, buf, maxlen - 1);  // 从stdin读取
    if (n < 0) {
        return -1;
    }
    buf[n] = '\0';  // 确保字符串以null结尾
    return n;
}

// 解析十进制整数
static int parse_int(char **s, int *value) {
    int sign = 1;
    int result = 0;
    
    skip_whitespace(s);
    
    // 处理符号
    if (**s == '-') {
        sign = -1;
        (*s)++;
    } else if (**s == '+') {
        (*s)++;
    }
    
    // 解析数字
    if (!isdigit(**s)) {
        return 0;  // 不是数字
    }
    
    while (isdigit(**s)) {
        result = result * 10 + (**s - '0');
        (*s)++;
    }
    
    *value = sign * result;
    return 1;  // 成功
}

// 解析无符号整数
static int parse_unsigned(char **s, unsigned int *value) {
    unsigned int result = 0;
    
    skip_whitespace(s);
    
    if (!isdigit(**s)) {
        return 0;
    }
    
    while (isdigit(**s)) {
        result = result * 10 + (**s - '0');
        (*s)++;
    }
    
    *value = result;
    return 1;
}

// 解析十六进制整数
static int parse_hex(char **s, unsigned int *value) {
    unsigned int result = 0;
    int digit;
    
    skip_whitespace(s);
    
    // 跳过可选的 "0x" 或 "0X"
    if (**s == '0' && ((*s)[1] == 'x' || (*s)[1] == 'X')) {
        *s += 2;
    }
    
    // 解析十六进制数字
    while (1) {
        if (isdigit(**s)) {
            digit = **s - '0';
        } else if (**s >= 'a' && **s <= 'f') {
            digit = **s - 'a' + 10;
        } else if (**s >= 'A' && **s <= 'F') {
            digit = **s - 'A' + 10;
        } else {
            break;
        }
        result = result * 16 + digit;
        (*s)++;
    }
    
    *value = result;
    return 1;
}

// 解析字符串（直到遇到空白字符）
static int parse_string(char **s, char *buf, int maxlen) {
    int i = 0;
    
    skip_whitespace(s);
    
    while (**s && !isspace(**s) && i < maxlen - 1) {
        buf[i++] = **s;
        (*s)++;
    }
    buf[i] = '\0';
    
    return i > 0;
}

// 解析单个字符
static int parse_char(char **s, char *c) {
    skip_whitespace(s);
    
    if (**s == '\0') {
        return 0;
    }
    
    *c = **s;
    (*s)++;
    return 1;
}

// 简单的 scanf 实现
// 支持格式: %d, %u, %x, %s, %c
int scanf(const char *fmt, ...) {
    char input_buf[256];  // 输入缓冲区
    char *input_ptr;
    va_list ap;
    int count = 0;
    
    // 读取一行输入
    if (read_line(input_buf, sizeof(input_buf)) < 0) {
        return -1;
    }
    
    input_ptr = input_buf;
    va_start(ap, fmt);
    
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
            case 'd': {  // 有符号整数
                int *ptr = va_arg(ap, int *);
                if (parse_int(&input_ptr, ptr)) {
                    count++;
                }
                break;
            }
            case 'u': {  // 无符号整数
                unsigned int *ptr = va_arg(ap, unsigned int *);
                if (parse_unsigned(&input_ptr, ptr)) {
                    count++;
                }
                break;
            }
            case 'x':  // 十六进制整数
            case 'X': {
                unsigned int *ptr = va_arg(ap, unsigned int *);
                if (parse_hex(&input_ptr, ptr)) {
                    count++;
                }
                break;
            }
            case 's': {  // 字符串
                char *ptr = va_arg(ap, char *);
                if (parse_string(&input_ptr, ptr, 256)) {
                    count++;
                }
                break;
            }
            case 'c': {  // 字符
                char *ptr = va_arg(ap, char *);
                if (parse_char(&input_ptr, ptr)) {
                    count++;
                }
                break;
            }
            case '%': {  // 字面量 %
                if (*input_ptr == '%') {
                    input_ptr++;
                }
                break;
            }
            default:
                // 未知格式，跳过
                break;
            }
            fmt++;
        } else if (isspace(*fmt)) {
            // 跳过格式字符串中的空白字符
            skip_whitespace(&input_ptr);
            fmt++;
        } else {
            // 普通字符，必须匹配
            if (*fmt == *input_ptr) {
                fmt++;
                input_ptr++;
            } else {
                // 不匹配，停止解析
                break;
            }
        }
    }
    
    va_end(ap);
    return count;
}

