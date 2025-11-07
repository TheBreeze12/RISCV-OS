#include "console.h"
#include <stdarg.h>
#include "../def.h"

#define BACKSPACE 0x100
#define DELETE    0x7F  // Delete键的ASCII码

void
cons_putc(int c)
{
  if(c == BACKSPACE){
    // if the user typed backspace, overwrite with a space.
    uart_putc('\b'); uart_putc(' '); uart_putc('\b');
  } else {
    uart_putc(c);
  }
}
void cons_puts(const char *s){
    while(*s){
        cons_putc(*s);
        s++;
    }
}

// 从控制台读取一个字符
int cons_getc(void) {
    int c = uart_getc();
    
    // 处理退格键和删除键
    if (c == '\r' || c == '\n') {
        return '\n';  // 统一返回换行符
    }
    
    // 将退格键和删除键统一转换为BACKSPACE
    if (c == '\b' || c == DELETE) {
        return BACKSPACE;
    }
    
    return c;
}


