#include "disk.h"
#include "x86.h"

typedef struct
{
    uint8_t size;
    uint8_t useless;
    uint16_t sectors;
    uint32_t ptr;
    uint64_t lba;
} DISK_ExtansionDAP;

void* linear_to_segoffset(void* linear)
{
    uint32_t addr = (uint32_t)linear;

    uint16_t segment = addr >> 4;
    uint16_t offset = addr & 0xF;

    return (void *)((segment << 16) | offset);
}

bool DISK_Init(DISK* disk, uint8_t drive)
{
    if (!x86_Disk_CheckExtansionsPresent(drive))
        return false;

    disk->drive = drive;

    return true;
}

bool DISK_Read(DISK* disk, uint64_t lba, uint16_t sectors, void* lowerDataOut)
{
    DISK_ExtansionDAP dap =
    {
        .size = 0x10,
        .useless = 0,
        .sectors = sectors,
        .ptr = (uint32_t)linear_to_segoffset(lowerDataOut),
        .lba = lba
    };

    if (!x86_Disk_ExtansionRead(disk->drive, &dap))
        return false;

    return true;
}