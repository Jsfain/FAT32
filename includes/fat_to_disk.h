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
------------------------------------------------------------------------------
|                              FIND BOOT SECTOR
|                                        
| Description : This function must be implemented to find address of the the 
|               boot sector on a FAT32-formatted disk. This function is used
|               by fat_setBPB();
|
| Returns     : address of the boot sector on the physical disk.
-------------------------------------------------------------------------------
*/

uint32_t 
FATtoDisk_findBootSector (void);



/* 
------------------------------------------------------------------------------
|                       READ SINGLE SECTOR FROM DISK
|                                        
| Description : This function must be implemented to load the contents of the
|               sector at a specified address on the physical address to an 
|               array.
|
| Arguments   : addr   - address of the sector on the physical disk that should
|                        be read into the array, *arr.
|             : *arr   - ptr to the array that will be loaded with the contents
|                        of the disk sector at the specified address.
|
| Returns     : 0 if successful, 1 if there is a read failure.
-------------------------------------------------------------------------------
*/

uint8_t 
FATtoDisk_readSingleSector (uint32_t addr, uint8_t *arr);



#endif //FATTOSD_H