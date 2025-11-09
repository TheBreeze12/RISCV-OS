#include "../include/def.h"
#define WriteReg(reg, v) (*(reg) = (v))
#define ReadReg(reg) (*(reg))

#define UART_BASE 0x10000000
#define UART_THR  (UART_BASE + 0)  // Transmit Holding Register
#define UART_RBR  (UART_BASE + 0)  // Receiver Buffer Register (same address as THR)
#define UART_LSR  (UART_BASE + 5)  // Line Status Register

// LSR bits
#define LSR_THRE  0x20  // Transmit Holding Register Empty (bit 5)
#define LSR_DR    0x01  // Data Ready (bit 0)

void uart_putc(char c) {
    volatile uint8 *thr = (volatile uint8*)UART_THR;   
    volatile uint8 *lsr = (volatile uint8*)UART_LSR;
    while ((*lsr & LSR_THRE) == 0);
    
    WriteReg(thr, c);
}

char uart_getc(void) {
    volatile uint8 *rbr = (volatile uint8*)UART_RBR;
    volatile uint8 *lsr = (volatile uint8*)UART_LSR;
    
    // 等待数据就绪
    while ((*lsr & LSR_DR) == 0);
    
    // 读取数据
    return (char)ReadReg(rbr);
}

