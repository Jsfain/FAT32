/*
*******************************************************************************
*                             AVR-FAT to DISK INTERFACE
*
* File    : FATtoDISK_INTERFACE.C
* Version : 0.0.0.2
* Author  : Joshua Fain
* Target  : ATMega1280
* License : MIT
* Copyright (c) 2020 Joshua Fain
* 
*
* DESCRIPTION: 
* Interface between the AVR-FAT module and a physical disk driver. A disk
* driver will be required for the raw data access on the physical FAT32-
* formatted disk. The two functions declared here must be implemented in order
* for the AVR-FAT module to operate.
*******************************************************************************
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