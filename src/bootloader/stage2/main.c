#include <stdint.h>
#include "stdio.h"
#include "disk.h"
#include "fat.h"
#include "memdefs.h"

uint8_t* kernelLoadBuffer = (uint8_t*)MEMORY_LOAD_KERNEL;
uint8_t* kernel = (uint8_t*)MEMORY_KERNEL_ADDR;

typedef void (*KernelMain)();

void __attribute__((cdecl)) start(uint8_t bootDrive)
{
    clrscr();

    DISK disk;

    if (!DISK_Init(&disk, bootDrive))
    {
        printf("Cannot init disk!\n");
        goto end;
    }

    if (!FAT_Init(&disk))
    {
        printf("Cannot init FAT!\n");
        goto end;
    }

    // load kernel
    FAT_File* fd = FAT_Open(&disk, "/kernel.sys");
    uint32_t read;
    uint8_t* kBuffer = kernel;
    while ((read = FAT_Read(&disk, fd, MEMORY_LOAD_SIZE, kernelLoadBuffer)))
    {
        memcpy(kBuffer, kernelLoadBuffer, read);
        kBuffer += read;
    }
    FAT_Close(fd);

    // execute kernel
    KernelMain kmain = (KernelMain)kernel;
    kmain();

end:
    for(;;);
}