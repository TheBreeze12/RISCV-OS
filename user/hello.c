#include "syscall.h"
#include "printf.h"

void main(void) {
    printf("Hello, World!\n");
    sys_exit(0);
}