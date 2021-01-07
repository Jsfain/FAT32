/*
*******************************************************************************
*                              AVR-FAT MODULE
*
* File    : FAT_BPB.H
* Version : 0.0.0.2
* Author  : Joshua Fain
* Target  : ATMega1280
* License : MIT
* Copyright (c) 2020 Joshua Fain
* 
* DESCRIPTION:
* Interface for accessing and storing the values of the boot sector / bios 
* parameter block of a FAT32 formatted volume.
*******************************************************************************
*/

#ifndef FAT_BPB_H
#define FAT_BPB_H

#include <avr/io.h>





/*
*******************************************************************************
*******************************************************************************
 *                     
 *                                    MACROS      
 *  
*******************************************************************************
*******************************************************************************
*/

#ifndef SECTOR_LEN
#define SECTOR_LEN                      512
#endif // SECTOR_LEN



// ******* Boot Sector Error Flags
#define CORRUPT_BOOT_SECTOR             0x01
#define NOT_BOOT_SECTOR                 0x02
#define INVALID_BYTES_PER_SECTOR        0x04
#define INVALID_SECTORS_PER_CLUSTER     0x08
#define BOOT_SECTOR_NOT_FOUND           0x10
#define BOOT_SECTOR_VALID               0x20
#define FAILED_READ_BOOT_SECTOR         0x40





/*
*******************************************************************************
*******************************************************************************
 *                     
 *                                 STRUCTS      
 *  
*******************************************************************************
*******************************************************************************
*/

// ****** Bios Paramater Block struct

// An instance of this struct should be used to hold the values of the BIOS 
// Parameter Block, and some calculated values, of a FAT32-formatted volume.
typedef struct BiosParameterBlock
{
  uint16_t bytesPerSec;
  uint8_t  secPerClus;
  uint16_t rsvdSecCnt;
  uint8_t  numOfFats;
  uint32_t fatSize32;
  uint32_t rootClus;

  uint32_t bootSecAddr;
  uint32_t dataRegionFirstSector;
} 
BPB;





/*
*******************************************************************************
*******************************************************************************
 *                     
 *                           FUNCTION DECLARATIONS       
 *  
*******************************************************************************
*******************************************************************************
*/

/*
-------------------------------------------------------------------------------
|                SET MEMBERS OF THE BIOS PARAMETER BLOCK STRUCT 
|                                        
| A valid BPB struct instance is a required argument of many functions that 
| access the FAT volume, therefore this function should be called first, before
| implementing any other parts of the FAT module.
|                                         
| Description : Gets values of the Bios Parameter Block / Boot Sector fields in
|               a FAT volume and sets the corresponding members of a BPB struct
|               instance accordingly.
|
| Argument    : *bpb      - ptr to a BPB struct instance. This function will
|                           set the members of this instance.
| 
| Return      : Boot Sector Error Flag. If any value other than
|               BOOT_SECTOR_VALID is returned then setting the BPB instance 
|               failed. To print, pass to fat_printBootSectorError().
-------------------------------------------------------------------------------
*/

uint8_t
fat_setBPB (BPB * bpb);



/*
-------------------------------------------------------------------------------
|                           PRINT BOOT SECTOR ERROR FLAG 
|                                        
| Description : Print the Boot Sector Error Flag. 
|
| Argument    : err   - boot sector error flag.
-------------------------------------------------------------------------------
*/

void
fat_printBootSectorError (uint8_t err);



#endif // FAT_BPB_H