#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "disk.h"

typedef struct
{
    uint8_t name[11];
    uint8_t attributes;
    uint8_t reserved;
    uint8_t creationTimeHundredths;
    uint16_t createdTime;
    uint16_t createdDate;
    uint16_t lastAccessedDate;
    uint16_t firstClusterHigh;
    uint16_t lastModificationTime;
    uint16_t lastModificationDate;
    uint16_t firstClusterLow;
    uint32_t size;
} __attribute__((packed)) FATDirectory;

typedef struct
{
   int handle;
   bool isDirectory;
   uint32_t size;
   uint32_t pos; 
} FATFile;

enum FATAttributes
{
    FAT_ATTRIBUTE_READ_ONLY         = 0x01,
    FAT_ATTRIBUTE_HIDDEN            = 0x02,
    FAT_ATTRIBUTE_SYSTEM            = 0x04,
    FAT_ATTRIBUTE_VOLUME_ID         = 0x08,
    FAT_ATTRIBUTE_DIRECTORY         = 0x10,
    FAT_ATTRIBUTE_ARCHIVE           = 0x20,
    FAT_ATTRIBUTE_LFN               = FAT_ATTRIBUTE_READ_ONLY | FAT_ATTRIBUTE_HIDDEN | FAT_ATTRIBUTE_SYSTEM | FAT_ATTRIBUTE_VOLUME_ID
};

bool FAT_Init(DISK* disk);
FATFile* FAT_Open(DISK* disk, const char* path);
uint32_t FAT_Read(DISK* disk, FATFile* file, uint32_t byteCount, void* dataOut);
bool FAT_ReadEntry(DISK* disk, FATFile* file, FATDirectory* dirEntry);
void FAT_Close(FATFile* file);