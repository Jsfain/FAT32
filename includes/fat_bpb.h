/*
***********************************************************************************************************************
*                                                       AVR-FAT MODULE
*
* File    : FAT_BPB.H
* Version : 0.0.0.2
* Author  : Joshua Fain
* Target  : ATMega1280
* Copyright (c) 2020 Joshua Fain
*
*
* DESCRIPTION:
* Used for accessing and storing the values of the boot sector / bios parameter block of a FAT32 formatted volume.
*
*
* FUNCTION "PUBLIC":
*  (1) uint8_t  fat_set_bios_parameter_block (BPB * bpb)
*  (2) void     fat_print_boot_sector_error (uint8_t err)
*
*
* STRUCTS (defined in FAT.H)
*  typedef struct BiosParameterBlock BPB
*
***********************************************************************************************************************
*/


#ifndef FAT_BPB_H
#define FAT_BPB_H

#include <avr/io.h>

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


// ****** Bios Paramater Block struct

// An instance of this struct should hold specific values set in the FAT volume's
// BIOS Parameter Block, as well as a few calculated values based on BPB values.
// These values should be set only by passing an instance of the struct to 
// FAT (BPB * bpb)
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
***********************************************************************************************************************
 *                                                       FUNCTIONS
***********************************************************************************************************************
*/


/*
***********************************************************************************************************************
 *                                   SET MEMBERS OF THE BIOS PARAMETER BLOCK STRUCT
 * 
 * A valid BPB struct instance is a required argument of any function that accesses the FAT volume, therefore this
 * function should be called first, before implementing any other parts of this FAT module.
 *                                         
 * Description : This function will set the members of a BiosParameterBlock (BPB) struct instance according to the
 *               values specified within the FAT volume's Bios Parameter Block / Boot Sector. 
 * 
 * Argument    : *bpb        Pointer to a BPB struct instance. This function will set the members of this instance.
 * 
 * Return      : Boot sector error flag     See the FAT.H header file for a list of these flags. To print the returned
 *                                          value, pass it FATfat_print_boot_sector_error(err). If the BPB instance's
 *                                          members are successfully set then BOOT_SECTOR_VALID is returned. Any other 
 *                                          returned value indicates a failure. 
 * 
 * Note        : This function DOES NOT set the values a physical FAT volume's Bios Parameter Block as would be 
 *               required during formatting of a FAT volume. This module can only read a FAT volume's contents and does
 *               not have the capability to modify anything on the volume itself, this includes formatting a volume.
***********************************************************************************************************************
*/

uint8_t
fat_set_bios_parameter_block (BPB * bpb);



/*
***********************************************************************************************************************
 *                                             PRINT BOOT SECTOR ERROR FLAG
 * 
 * Description : Call this function to print the error flag returned by the functioFAT(). 
 * 
 * Argument    : err    Boot Sector Error flag returned the functioFAT().
***********************************************************************************************************************
*/

void
fat_print_boot_sector_error (uint8_t err);



#endif // FAT_BPB_H