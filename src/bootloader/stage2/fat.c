#include "fat.h"
#include "stdio.h"
#include "memdefs.h"
#include "string.h"
#include "memory.h"
#include "ctype.h"
#include <stddef.h>
#include "minmax.h"

#define SECTOR_SIZE             512
#define MAX_PATH_SIZE           256
#define MAX_FILE_HANDLES        10
#define ROOT_DIRECTORY_HANDLE   -1

typedef struct 
{
    uint8_t bootJumpInstruction[3];
    uint8_t oemId[8];
    uint16_t bytesPerSector;
    uint8_t sectorsPerCluster;
    uint16_t reservedSectors;
    uint8_t fatCount;
    uint16_t dirEntryCount;
    uint16_t totalSectors;
    uint8_t mediaDescriptorType;
    uint16_t sectorsPerFat;
    uint16_t sectorsPerTrack;
    uint16_t heads;
    uint32_t hiddenSectors;
    uint32_t largeSectorCount;

    // extended boot record
    uint8_t driveNumber;
    uint8_t reserved;
    uint8_t signature;
    uint32_t volumeId;          // serial number, value doesn't matter
    uint8_t volumeLabel[11];    // 11 bytes, padded with spaces
    uint8_t fileSystem[8];
} __attribute__((packed)) FATBootSector;

typedef struct
{
    uint8_t buffer[SECTOR_SIZE];
    FATFile file;
    bool opened;
    uint32_t firstCluster;
    uint32_t currentCluster;
    uint32_t currentSectorInCluster;
} FATFileData;

typedef struct
{
    union
    {
        FATBootSector bootSector;
        uint8_t bootSectorBytes[SECTOR_SIZE];
    } BS;

    FATFileData rootDirectory;
    FATFileData openedFiles[MAX_FILE_HANDLES];
} FATData;

static FATData* data;
static uint8_t* fat = NULL;
static uint32_t dataSectionLba;

bool FAT_ReadBootSector(DISK* disk)
{
    return DISK_Read(disk, 0, 1, data->BS.bootSectorBytes);
}

bool FAT_ReadFat(DISK* disk)
{
    return DISK_Read(disk, data->BS.bootSector.reservedSectors, data->BS.bootSector.sectorsPerFat, fat);
}

bool FAT_Init(DISK* disk)
{
    data = (FATData*)MEMORY_FAT_ADDR;

    // read boot sector
    if (!FAT_ReadBootSector(disk))
    {
        printf("FAT: read boot sector failed\n");
        return false;
    }

    // read FAT
    fat = (uint8_t*)data + sizeof(FATData);
    uint32_t fatSize = data->BS.bootSector.bytesPerSector * data->BS.bootSector.sectorsPerFat;
    if (sizeof(FATData) + fatSize >= MEMORY_FAT_SIZE)
    {
        printf("FAT: not enough memory to read FAT! Required %lu, only have %u\n", sizeof(FATData) + fatSize, MEMORY_FAT_SIZE);
        return false;
    }

    if (!FAT_ReadFat(disk))
    {
        printf("FAT: read FAT failed\n");
        return false;
    }

    // open root directory file
    uint32_t rootDirLba = data->BS.bootSector.reservedSectors + data->BS.bootSector.sectorsPerFat * data->BS.bootSector.fatCount;
    uint32_t rootDirSize = sizeof(FATDirectory) * data->BS.bootSector.dirEntryCount;

    data->rootDirectory.file.handle = ROOT_DIRECTORY_HANDLE;
    data->rootDirectory.file.isDirectory = true;
    data->rootDirectory.file.pos = 0;
    data->rootDirectory.file.size = sizeof(FATDirectory) * data->BS.bootSector.dirEntryCount;
    data->rootDirectory.opened = true;
    data->rootDirectory.firstCluster = rootDirLba;
    data->rootDirectory.currentCluster = rootDirLba;
    data->rootDirectory.currentSectorInCluster = 0;

    if (!DISK_Read(disk, rootDirLba, 1, data->rootDirectory.buffer))
    {
        printf("FAT: read root directory failed\n");
        return false;
    }

    // calculate data section
    uint32_t rootDirSectors = (rootDirSize + data->BS.bootSector.bytesPerSector - 1) / data->BS.bootSector.bytesPerSector;
    dataSectionLba = rootDirLba + rootDirSectors;

    // reset opened files
    for (int i = 0; i < MAX_FILE_HANDLES; i++)
        data->openedFiles[i].opened = false;

    return true;
}

uint32_t FAT_ClusterToLba(uint32_t cluster)
{
    return dataSectionLba + (cluster - 2) * data->BS.bootSector.sectorsPerCluster;
}

FATFile* FAT_OpenEntry(DISK* disk, FATDirectory* entry)
{
    // find empty handle
    int handle = -1;
    for (int i = 0; i < MAX_FILE_HANDLES && handle < 0; i++)
    {
        if (!data->openedFiles[i].opened)
            handle = i;
    }

    // out of handles
    if (handle < 0)
    {
        printf("FAT: out of file handles\n");
        return false;
    }

    // setup vars
    FATFileData* fd = &data->openedFiles[handle];
    fd->file.handle = handle;
    fd->file.isDirectory = (entry->attributes & FAT_ATTRIBUTE_DIRECTORY) != 0;
    fd->file.pos = 0;
    fd->file.size = entry->size;
    fd->firstCluster = entry->firstClusterLow + ((uint32_t)entry->firstClusterHigh << 16);
    fd->currentCluster = fd->firstCluster;
    fd->currentSectorInCluster = 0;

    if (!DISK_Read(disk, FAT_ClusterToLba(fd->currentCluster), 1, fd->buffer))
    {
        printf("FAT: open entry failed - read error\n");
        return false;
    }

    fd->opened = true;

    return &fd->file;
}

uint32_t FAT_NextCluster(uint32_t currentCluster)
{    
    uint32_t fatIndex = currentCluster * 3 / 2;

    if (currentCluster % 2 == 0)
        return (*(uint16_t*)(fat + fatIndex)) & 0x0FFF;
    else
        return (*(uint16_t*)(fat + fatIndex)) >> 4;
}

uint32_t FAT_Read(DISK* disk, FATFile* file, uint32_t byteCount, void* dataOut)
{
    // get file data
    FATFileData* fd = (file->handle == ROOT_DIRECTORY_HANDLE) 
        ? &data->rootDirectory 
        : &data->openedFiles[file->handle];

    uint8_t* u8DataOut = (uint8_t*)dataOut;

    // don't read past the end of the file
    if (!fd->file.isDirectory || (fd->file.isDirectory && fd->file.size > 0)) 
        byteCount = min(byteCount, fd->file.size - fd->file.pos);

    while (byteCount > 0)
    {
        uint32_t leftInBuffer = SECTOR_SIZE - (fd->file.pos % SECTOR_SIZE);
        uint32_t take = min(byteCount, leftInBuffer);

        memcpy(u8DataOut, fd->buffer + fd->file.pos % SECTOR_SIZE, take);
        u8DataOut += take;
        fd->file.pos += take;
        byteCount -= take;

        // printf("leftInBuffer=%lu take=%lu\r\n", leftInBuffer, take);
        // See if we need to read more data
        if (leftInBuffer == take)
        {
            // Special handling for root directory
            if (fd->file.handle == ROOT_DIRECTORY_HANDLE)
            {
                ++fd->currentCluster;

                // read next sector
                if (!DISK_Read(disk, fd->currentCluster, 1, fd->buffer))
                {
                    printf("FAT: read error!\n");
                    break;
                }
            }
            else
            {
                // calculate next cluster & sector to read
                if (++fd->currentSectorInCluster >= data->BS.bootSector.sectorsPerCluster)
                {
                    fd->currentSectorInCluster = 0;
                    fd->currentCluster = FAT_NextCluster(fd->currentCluster);
                }

                if (fd->currentCluster >= 0xFF8)
                {
                    // Mark end of file
                    fd->file.size = fd->file.pos;
                    break;
                }

                // read next sector
                if (!DISK_Read(disk, FAT_ClusterToLba(fd->currentCluster) + fd->currentSectorInCluster, 1, fd->buffer))
                {
                    printf("FAT: read error!\n");
                    break;
                }
            }
        }
    }

    return u8DataOut - (uint8_t*)dataOut;
}

bool FAT_ReadEntry(DISK* disk, FATFile* file, FATDirectory* dirEntry)
{
    return FAT_Read(disk, file, sizeof(FATDirectory), dirEntry) == sizeof(FATDirectory);
}

void FAT_Close(FATFile* file)
{
    if (file->handle == ROOT_DIRECTORY_HANDLE)
    {
        file->pos = 0;
        data->rootDirectory.currentCluster = data->rootDirectory.firstCluster;
    }
    else
    {
        data->openedFiles[file->handle].opened = false;
    }
}

bool FAT_FindFile(DISK* disk, FATFile* file, const char* name, FATDirectory* entryOut)
{
    char fatName[12];
    FATDirectory entry;

    // convert from name to fat name
    memset(fatName, ' ', sizeof(fatName));
    fatName[11] = '\0';

    const char* ext = strchr(name, '.');
    if (ext == NULL)
        ext = name + 11;

    for (int i = 0; i < 8 && name[i] && name + i < ext; i++)
        fatName[i] = toupper(name[i]);

    if (ext != name + 11)
    {
        for (int i = 0; i < 3 && ext[i + 1]; i++)
            fatName[i + 8] = toupper(ext[i + 1]);
    }

    while (FAT_ReadEntry(disk, file, &entry))
    {
        if (memcmp(fatName, entry.name, 11) == 0)
        {
            *entryOut = entry;
            return true;
        }
    }
    
    return false;
}

FATFile* FAT_Open(DISK* disk, const char* path)
{
    char name[MAX_PATH_SIZE];

    // ignore leading slash
    if (path[0] == '/')
        path++;

    FATFile* current = &data->rootDirectory.file;

    while (*path)
    {
        // extract next file name from path
        bool isLast = false;
        const char* delim = strchr(path, '/');
        if (delim != NULL)
        {
            memcpy(name, path, delim - path);
            name[delim - path + 1] = '\0';
            path = delim + 1;
        }
        else
        {
            unsigned len = strlen(path);
            memcpy(name, path, len);
            name[len + 1] = '\0';
            path += len;
            isLast = true;
        }
        
        // find directory entry in current directory
        FATDirectory entry;
        if (FAT_FindFile(disk, current, name, &entry))
        {
            FAT_Close(current);

            // check if directory
            if (!isLast && entry.attributes & FAT_ATTRIBUTE_DIRECTORY == 0)
            {
                printf("FAT: %s not a directory\n", name);
                return NULL;
            }

            // open new directory entry
            current = FAT_OpenEntry(disk, &entry);
        }
        else
        {
            FAT_Close(current);

            printf("FAT: %s not found\n", name);
            return NULL;
        }
    }

    return current;
}