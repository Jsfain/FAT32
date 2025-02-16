/*
 * File       : FAT_TO_DISK_IF.H
 * Version    : 2.0
 * License    : GNU GPLv3
 * Author     : Joshua Fain
 * Copyright (c) 2020, 2021
 *
 * This file provides macros and function prototypes required by the FAT32 
 * module to interface with a disk driver handling the function of accessing a
 * a FAT32 formatted disk itself. These functions must be implemented in order 
 * for the FAT32 module to function.
 */

#ifndef FAT_TO_DISK_IF_H
#define FAT_TO_DISK_IF_H

/*
 ******************************************************************************
 *                                 MACROS
 ******************************************************************************
 */

//
// If FATtoDisk_FindBootSector is successful it should return the location of 
// the boot sector on the disk, i.e. it's block/sector address on the disk. If
// the boot sector is NOT found it should return this value. This is set to the
// highest possible 32-bit integer since the boot sector should never be 
// expected to be found here.
// 
#define FAILED_FIND_BOOT_SECTOR        0xFFFFFFFF

//
// These macros are used by FATtoDisk_FindBootSector to specify the range of 
// blocks over which to search for the boot sector.
//
#define FBS_SEARCH_START_BLOCK         0    // specifies starting block
#define FBS_MAX_NUM_BLKS_SEARCH_MAX    50   // specifies number of blocks

// values that can be returned by FATtoDisk_ReadSingleSector. 
#define READ_SECTOR_SUCCESS     0     
#ifndef FAILED_READ_SECTOR
#define FAILED_READ_SECTOR      0x08        // This should be defined in fat.h
#endif//FAILED_READ_SECTOR

// Boot sector signature bytes. The last two bytes of BS should be these.
#define BS_SIGN_1     0x55
#define BS_SIGN_2     0xAA

//
// The first three bytes to the boot sector are the JUMP BOOT bytes. These can
// either be, where X is 'don't care':
//   (1) type A : [0] = 0xEB [1] = X [2] = 0x90 OR
//   (2) type B : [0] = 0xE9 [1] = X [2] = X
//
#define JMP_BOOT_1A     0xEB
#define JMP_BOOT_3A     0x90
#define JMP_BOOT_1B     0xE9

/*
 ******************************************************************************
 *                            FUNCTION PROTOTYPES
 ******************************************************************************
 */

/*
 * ----------------------------------------------------------------------------
 *                                                             FIND BOOT SECTOR
 *                                 
 * Description : Finds the address of the boot sector on the FAT32-formatted 
 *               disk. This function is required by fat_SetBPB in fat_bpb.c.
 * 
 * Arguments   : void
 * 
 * Returns     : Address of the boot sector on the disk.
 * 
 * Notes       : The search for the boot sector will begin at
 *               FBS_SEARCH_START_BLOCK, and search a total of 
 *               FBS_MAX_NUM_BLKS_SEARCH_MAX blocks. 
 * ----------------------------------------------------------------------------
 */
uint32_t FATtoDisk_FindBootSector(void);

/* 
 * ----------------------------------------------------------------------------
 *                                                 READ SINGLE SECTOR FROM DISK
 *                                       
 * Description : Loads the contents of the sector/block at the specified 
 *               address on the SD card into the array, blckArr.
 *
 * Arguments   : blkNum    - Block number address of the sector/block on the SD
 *                           card that should be read into blkArr.
 * 
 *               blkArr    - Pointer to the array that will be loaded with the 
 *                           contents of the sector/block on the SD card at 
 *                           block specified by blkNum.
 * 
 * Returns     : READ_SECTOR_SUCCES if successful.
 *               READ_SECTOR_FAILED if failure.
 * ----------------------------------------------------------------------------
 */
uint8_t FATtoDisk_ReadSingleSector(uint32_t blkNum, uint8_t blkArr[]);

#endif //FAT_TO_DISK_IF_
