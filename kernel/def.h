#include "type.h"
#include "mm/riscv.h"

// 自定义assert宏
#define assert(condition) \
    do { \
        if (!(condition)) { \
            printf("Assertion failed: %s, file %s, line %d\n", \
                   #condition, __FILE__, __LINE__); \
            panic("Assertion failed"); \
        } \
    } while(0)

//printf.c
void printint(long long value, int base, int sgn);
void printf(const char *fmt, ...);
void panic(char *s);
void clear_screen(void);
void clear_line(void);
void goto_xy(int x, int y);
void set_text_color(const char *color);
void set_background_color(const char *color);
void reset_colors(void);
void printf_color(const char *color, const char *fmt, ...);

//uart.c
void uart_putc(char c);

//console.c
void cons_putc(int c);
void cons_puts(const char *s);

// kalloc.c
void*           kalloc(void);
void            kfree(void *);
void            kinit(void);

// string.c
void* memset(void *dst, int c, uint n);


// vm.c
void            kvminit(void);
void            kvminithart(void);
void            kvmmap(pagetable_t, uint64, uint64, uint64, int);

int             mappages(pagetable_t, uint64, uint64, uint64, int);
pagetable_t     create_pagetable(void);
void            free_pagetable(pagetable_t);
pte_t *         walk(pagetable_t, uint64, int);
uint64          walkaddr(pagetable_t, uint64);
int             ismapped(pagetable_t, uint64);