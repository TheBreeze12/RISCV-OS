// 用户空间的 printf 实现
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

// 数字字符表
static const char digits[] = "0123456789abcdef";

// 计算字符串长度
static int strlen(const char *s) {
    int n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

// 整数转字符串
static void itoa(long long value, char *str, int base, int sgn) {
    char buf[32];
    int i, neg;
    unsigned long long x;

    neg = 0;
    if (sgn && value < 0) {
        neg = 1;
        x = (unsigned long long)(-value);
    } else {
        x = (unsigned long long)value;
    }

    i = 0;
    if (x == 0) {
        buf[i++] = '0';
    } else {
        do {
            buf[i++] = digits[x % base];
        } while ((x /= base) != 0);
    }
    
    if (neg) {
        buf[i++] = '-';
    }

    int j = 0;
    while (--i >= 0) {
        str[j++] = buf[i];
    }
    str[j] = '\0';
}

// 输出字符串到 stdout
static void puts(const char *s) {
    int len = strlen(s);
    if (len > 0) {
        sys_write(1, s, len);
    }
}

// 输出单个字符到 stdout
static void putc(char c) {
    sys_write(1, &c, 1);
}

// vprintf 核心实现
static void vprintf_internal(const char *fmt, va_list ap) {
    const char *p;
    char tmp[32];

    for (p = fmt; *p; p++) {
        if (*p != '%') {
            putc(*p);
            continue;
        }

        // 处理格式化字符
        p++;
        switch (*p) {
        case 'd':  // 十进制整数
        case 'i': {
            int value = va_arg(ap, int);
            itoa(value, tmp, 10, 1);
            puts(tmp);
            break;
        }
        case 'u': {  // 无符号十进制
            unsigned int value = va_arg(ap, unsigned int);
            itoa((long long)value, tmp, 10, 0);
            puts(tmp);
            break;
        }
        case 'x': {  // 十六进制整数（小写）
            int value = va_arg(ap, int);
            itoa(value, tmp, 16, 0);
            puts(tmp);
            break;
        }
        case 'X': {  // 十六进制整数（大写）
            int value = va_arg(ap, int);
            itoa(value, tmp, 16, 0);
            // 转换为大写
            for (char *c = tmp; *c; c++) {
                if (*c >= 'a' && *c <= 'f') {
                    *c = *c - 'a' + 'A';
                }
            }
            puts(tmp);
            break;
        }
        case 'c': {  // 字符
            char c = (char)va_arg(ap, int);
            putc(c);
            break;
        }
        case 's': {  // 字符串
            const char *s = va_arg(ap, const char *);
            if (s) {
                puts(s);
            } else {
                puts("(null)");
            }
            break;
        }
        case '%': {  // 字面量 %
            putc('%');
            break;
        }
        case '\n': {  // 换行
            putc('\n');
            break;
        }
        default: {  // 未知格式，直接输出
            putc('%');
            if (*p) {
                putc(*p);
            }
            break;
        }
        }
    }
}

// printf 主函数
void printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf_internal(fmt, ap);
    va_end(ap);
}