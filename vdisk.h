#ifndef VDISK_H
#define VDISK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>

/* Virtual Disk Constants */
#define VD_SIZE (64 * 1024 * 1024)      /* 64 MB */
#define BLOCK_SIZE 1024                 /* 1 KB blocks */
#define TOTAL_BLOCKS (VD_SIZE / BLOCK_SIZE)  /* 65536 blocks */
#define BITMAP_BLOCKS 8                 /* Blocks 1-8 for bitmap */
#define FAT_BLOCKS 256                  /* Blocks 9-264 for FAT */
#define ROOT_BLOCK 265                  /* Root directory block */
#define DATA_START_BLOCK 266            /* Data blocks start here */
#define BITS_PER_BYTE 8
#define PTRS_PER_FAT_BLOCK (BLOCK_SIZE / sizeof(unsigned int))  /* 256 */
#define ENTRIES_PER_BLOCK (BLOCK_SIZE / sizeof(struct metadata))  /* 32 */

/* Metadata structure for files and directories */
struct metadata {
    char type;              /* 'f' for file, 'd' for directory */
    char name[23];          /* Null-terminated name, max 22 chars */
    unsigned int size;      /* File size in bytes, or dir entries count */
    unsigned int firstblock; /* First block number */
};

/* Special constants */
#define META_SIZE sizeof(struct metadata)
#define NULL_BLOCK 0        /* NULL pointer in FAT (0 means no next block) */
#define SHM_KEY 0x12345678  /* Shared memory key */

#endif
