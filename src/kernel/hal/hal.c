#include "hal.h"
#include <arch/i686/gdt.h>

void HAL_Init()
{
    i686_GDT_Init();
}