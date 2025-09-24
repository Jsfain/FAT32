/*
 * File       : FAT_DISK_IF.H
 * Version    : 2.0
 * License    : GNU GPLv3
 * Author     : Joshua Fain
 * Copyright (c) 2020 - 2025
 *
 * This is the interface between the FAT module and a disk module capable of
 * accessing the raw data from a disk. In this current implementation, the disk
 * module only needs to be capable of reading single data sectors/blocks by
 * specifying the desired data block address. The raw block data must be read 
 * into an array to be passed to the FAT module to interpret accordingly.
 * See the function descriptions below for the exact requirements.
 */

#ifndef FAT_DISK_IF_H
#define FAT_DISK_IF_H

/*
 ******************************************************************************
 *                                 MACROS
 ******************************************************************************
 */

 //
 // The expected byte length of the FAT32 sector. Note (1) The Value must match
 // the BPB struct's 'bytesPerSec' field and (2) This value should be set to 
 // 512 for the current implementation to work.
 //
#ifndef SECTOR_LEN
#define SECTOR_LEN                      512
#endif//SECTOR_LEN

//
// If fatDisk_FindBootSector is successful it returns the address of the boot 
// sector, i.e. it's block/sector address on the disk. If the boot sector is 
// NOT found it returns this value set to the highest 32-bit integer since the
// boot sector should never be expected to be found at this address.
// 
#define FAILED_FIND_BOOT_SECTOR        0xFFFFFFFF

// values that can be returned by fatDisk_ReadSector. 
#define READ_SECTOR_SUCCESS     0     
#ifndef FAILED_READ_SECTOR
#define FAILED_READ_SECTOR      0x08        // should be defined in fat.h
#endif//FAILED_READ_SECTOR

// Boot sector signature bytes. These are values of the last two bytes of BS.
#define BS_SIGN_1     0x55
#define BS_SIGN_2     0xAA

//
// First three bytes to the boot sector are the JUMP BOOT bytes. These can
// be either of the following (X is 'don't care'):
//   (1) type A : [0] = 0xEB [1] = X [2] = 0x90 OR
//   (2) type B : [0] = 0xE9 [1] = X [2] = X
//
#define JMP_BOOT_1A     0xEB                // type A byte 1
#define JMP_BOOT_3A     0x90                // type A byte 3
#define JMP_BOOT_1B     0xE9                // type B byte 1

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
 * ----------------------------------------------------------------------------
 */
uint32_t fatDisk_FindBootSector(void);

/* 
 * ----------------------------------------------------------------------------
 *                                                 READ SINGLE SECTOR FROM DISK
 *                                       
 * Description : Loads the contents of the sector/block at the specified 
 *               address on the disk into array, blckArr.
 *
 * Arguments   : blkNum    - Block number/address of the sector/block on the 
 *                           disk that should be read into blkArr.
 * 
 *               blkArr    - Pointer to array that will be loaded with the 
 *                           contents of the sector/block on the disk at the  
 *                           block specified by blkNum.
 * 
 * Returns     : READ_SECTOR_SUCCES if successful.
 *               FAILED_READ_SECTOR if failure.
 * ----------------------------------------------------------------------------
 */
uint8_t fatDisk_ReadSector(uint32_t blkNum, uint8_t blkArr[]);

#endif //FAT_TO_DISK_IF_
