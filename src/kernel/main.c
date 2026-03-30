#include <stdint.h>
#include "stdio.h"
#include "memory.h"
#include <hal/hal.h>

extern uint8_t __bss_start;
extern uint8_t __end;

void __attribute__((section(".entry"))) kmain(uint8_t bootDrive)
{
    memset(&__bss_start, 0, (&__end) - (&__bss_start));

    HAL_Init();

    clrscr();

    printf("Hello world from kernel!\n");

    for(;;);
}