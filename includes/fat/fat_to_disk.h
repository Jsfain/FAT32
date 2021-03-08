/*
 * File    : FATtoDISK.H
 * Version : 0.0.0.2
 * Author  : Joshua Fain
 * Target  : ATMega1280
 * License : MIT
 * Copyright (c) 2020 Joshua Fain
 *
 * Provides prototypes for functions required by this AVR-FAT module to
 * interface with a physical disk.
 */

#ifndef FATTOSD_H
#define FATTOSD_H

#include <avr/io.h>

/*
 ******************************************************************************
 *                                 MACROS
 ******************************************************************************
 */

//
// If FATtoDisk_FindBootSector() is successful it will return the physical 
// location of the boot sector (i.e. it's block/sector address on the disk).
// If the boot sector is not found it should return this value here. This is 
// set to the highest possible 32-bit integer because the boot sector should 
// never be found here, and if it is, then it would be pointless, as nothing 
// could exist at a higher address.
// 
#define FAILED_FIND_BOOT_SECTOR    0xFFFFFFFF

//
// values returned by FATtoDisk_ReadSingleSector(). Specify with FTD_ to
// indicate that this is from the FATtoDisk files.
//
#define READ_SECTOR_SUCCESS        0
#ifndef FAILED_READ_SECTOR
#define FAILED_READ_SECTOR         0x08
#endif//FAILED_READ_SECTOR
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
 *               SD card. This function is used by fat_SetBPB().
 * 
 * Arguments   : void
 * 
 * Returns     : Address of the boot sector on the SD card.
 * 
 * Notes       : The search for the boot sector will search a total of
 *               'maxNumOfBlcksToChck' starting at 'startBlckNum'.
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
uint8_t FATtoDisk_ReadSingleSector(uint32_t addr, uint8_t *arr);

#endif //FATTOSD_H
