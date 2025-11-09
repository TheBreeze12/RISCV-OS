#include "./utils/syscall.h"
#include "./utils/printf.h"

void main(void) {
    printf("Hello, World!\n");
    sys_exit(0);
}