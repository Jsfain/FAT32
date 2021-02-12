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
 *                            FUNCTION PROTOTYPES
 ******************************************************************************
 */

/* 
 * ----------------------------------------------------------------------------
 *                                                             FIND BOOT SECTOR
 *                                        
 * Description : This function must be implemented locate the address of the 
 *               FAT boot sector on a FAT32-formatted disk. This function is 
 *               used by fat_setBPB().
 * 
 * Arguments   : void
 * 
 * Returns     : address of the boot sector on the physical disk.
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