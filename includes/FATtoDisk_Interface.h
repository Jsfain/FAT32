/*
***********************************************************************************************************************
*                                               AVR-FAT to DISK INTERFACE
*
* File   : FATtoDISK_INTERFACE.H
* Author : Joshua Fain
* Target : ATMega1280
*
*
* DESCRIPTION: 
* This file in to be used as the interface file between the AVR-FAT module and a physical disk layer/driver. The disk
* layer will be required for the raw data access on the physical FAT32-formatted disk. The two functions declared here
* must be implemented in order for the AVR-FAT module to operate. In the current test, these are implemented in the
* FATtoSD.C file to read access the contents of a FAT32-formatted SD Card. The AVR-SD Card source files are also
* included in the repo, but should not be considered part of the module.
*  
* 
* FUNCTIONS:
* (1) uint32_t FATtoDisk_FindBootSector();                                               
* (2) uint8_t  FATtoDisk_ReadSingleSector (uint32_t address, uint8_t *sectorByteArry)
*                                                
*                                                       MIT LICENSE
*
* Copyright (c) 2020 Joshua Fain
*
* Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated 
* documentation files (the "Software"), to deal in the Software without restriction, including without limitation the 
* rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to 
* permit ersons to whom the Software is furnished to do so, subject to the following conditions: The above copyright 
* notice and this permission notice shall be included in all copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE 
* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
* COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
* OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
***********************************************************************************************************************
*/

#ifndef FATTOSD_H
#define FATTOSD_H

#include <avr/io.h>


// This function is required to interface with the physical disk hosting the FAT volume. It returns
// a value corresponding to the addressed location of the Boot Sector / Bios Parameter Block on the
// physical disk. This function is used by FAT_SetBiosParameterBlock().
uint32_t FATtoDisk_FindBootSector();



// This function is required to interface with the physical disk hosting the FAT volume. This function
// should load the contents of the sector at the physical address specified in the address argument into
// the array pointed at by *sectorByteArray. This function is used by all FAT functions requiring access
// to the physical disk. The function will return 1 if there is a read failure and 0 if it is successful. 
uint8_t FATtoDisk_ReadSingleSector( uint32_t address, uint8_t * arr );


#endif //FATTOSD_H