#include <stdarg.h>

// 声明外部的UART输出函数
extern void uart_putc(char c);
extern void uart_puts(const char *s);

// 简单的整数转字符串函数
static void itoa(int value, char *str, int base) {
    char *ptr = str;
    char *ptr1 = str;
    char tmp_char;
    int tmp_value;
    int negative = 0;

    // 处理负数
    if (value < 0 && base == 10) {
        negative = 1;
        value = -value;
    }

    // 处理0的特殊情况
    if (value == 0) {
        *str++ = '0';
        *str = '\0';
        return;
    }

    // 转换数字
    while (value != 0) {
        tmp_value = value % base;
        *ptr++ = (tmp_value < 10) ? (tmp_value + '0') : (tmp_value - 10 + 'a');
        value /= base;
    }

    // 添加负号
    if (negative) {
        *ptr++ = '-';
    }

    *ptr-- = '\0';

    // 反转字符串
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr = *ptr1;
        *ptr1 = tmp_char;
        ptr--;
        ptr1++;
    }
}

// 实现vprintf功能的核心函数
static void vprintf_internal(const char *fmt, va_list ap) {
    const char *p;
    char tmp[32];
    
    for (p = fmt; *p; p++) {
        if (*p != '%') {
            uart_putc(*p);
            continue;
        }
        
        // 处理格式化字符
        p++;
        switch (*p) {
            case 'd':  // 十进制整数
            case 'i': {
                int value = va_arg(ap, int);
                itoa(value, tmp, 10);
                uart_puts(tmp);
                break;
            }
            case 'x': {  // 十六进制整数（小写）
                int value = va_arg(ap, int);
                itoa(value, tmp, 16);
                uart_puts(tmp);
                break;
            }
            case 'X': {  // 十六进制整数（大写）
                int value = va_arg(ap, int);
                itoa(value, tmp, 16);
                // 转换为大写
                for (char *c = tmp; *c; c++) {
                    if (*c >= 'a' && *c <= 'f') {
                        *c = *c - 'a' + 'A';
                    }
                }
                uart_puts(tmp);
                break;
            }
            case 'c': {  // 字符
                char c = (char)va_arg(ap, int);  // char被提升为int
                uart_putc(c);
                break;
            }
            case 's': {  // 字符串
                const char *s = va_arg(ap, const char*);
                if (s) {
                    uart_puts(s);
                } else {
                    uart_puts("(null)");
                }
                break;
            }
            case '%': {  // 字面量%
                uart_putc('%');
                break;
            }
            default: {  // 未知格式，直接输出
                uart_putc('%');
                uart_putc(*p);
                break;
            }
        }
    }
}

void printf(const char *fmt, ...) {
    va_list ap;  // 声明va_list变量

    // 初始化可变参数列表，fmt是最后一个固定参数
    va_start(ap, fmt);
    
    // 调用实际的格式化输出函数
    vprintf_internal(fmt, ap);
    
    // 清理可变参数列表
    va_end(ap);
}
