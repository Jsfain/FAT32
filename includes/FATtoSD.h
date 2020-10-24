/*
***********************************************************************************************************************
* File   : FATtoSD.H
* Author : Joshua Fain
* Target : ATMega1280
*
*
* DESCRIPTION: 
* This file in included as an interface between the AVR-FAT module and the AVR-SDCard module. A module/driver for
* accessing raw data on a FAT32-formatted volume is required. The AVR-FAT module is intended to be independent of the
* physical volume, and implementing a different raw data access driver would is possible as long as "non-private"
* functions are implemented that have the same functionality as those here. 
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

uint8_t FATtoDisk_ReadSingleSector( uint32_t address, uint8_t * arr );

uint32_t FATtoDisk_FindBootSector();

#endif //FATTOSD_H