#include <stdint.h>
#include "stdio.h"
#include "disk.h"
#include "fat.h"

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

    FATFile* fd = FAT_Open(&disk, "mydir/test.txt");
    char buffer[100];
    if (!FAT_Read(&disk, fd, sizeof(buffer), buffer))
    {
        printf("Cannot read file!\n");
        goto end;
    }
    for (int i = 0; i < sizeof(buffer) && buffer[i] != '\0'; i++)
        printf("%c", buffer[i]);
    
    printf("\n");

    FAT_Close(fd);

end:
    for(;;);
}