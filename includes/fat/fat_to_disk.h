/*
 * File       : FATtoDISK.H
 * Version    : 2.0
 * Target     : ATMega1280
 * Compiler   : AVR-GCC 9.3.0
 * Downloader : AVRDUDE 6.3
 * License    : GNU GPLv3
 * Author     : Joshua Fain
 * Copyright (c) 2020, 2021
 *
 * This file provides prototypes for functions required by the AVR-FAT module 
 * to interface with a physical disk.
 */

#ifndef FATTOSD_H
#define FATTOSD_H

/*
 ******************************************************************************
 *                                 MACROS
 ******************************************************************************
 */

//
// If FATtoDisk_FindBootSector is successful it will return the physical 
// location of the boot sector (i.e. it's block/sector address on the disk).
// If the boot sector is not found it should return the value of
// FAILED_FIND_BOOT_SECTOR. This is set to the highest possible 32-bit integer
// because the boot sector is never expected to be found here.
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
 *               SD card. This function is used by fat_SetBPB from fat_bpb.c(h)
 * 
 * Arguments   : void
 * 
 * Returns     : Address of the boot sector on the SD card.
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
 * Description : This function must be implemented in order to load the 
 *               contents of a FAT sector located at a specified address on a 
 *               physical disk to an array.
 * 
 * Arguments   : addr     Address of the sector on the physical disk that will
 *                        be read into the array, *arr.
 * 
 *               arr   -  Pointer to the array is to be loaded with the
 *                        contents the FAT sector at address, addr, on the 
 *                        physical disk.
 * 
 * Returns     : 0 sector is successfully loaded into the array.
 *               1 if there is a failure to load the array with the sector.
 * ----------------------------------------------------------------------------
 */
uint8_t FATtoDisk_ReadSingleSector(uint32_t blkNum, uint8_t blkArr[]);

#endif //FATTOSD_H
