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

#include <string.h>
#include <avr/io.h>
#include "../includes/prints.h"
#include "../includes/usart.h"
#include "../includes/fat_bpb.h"
#include "../includes/fattodisk_interface.h"




/*
***********************************************************************************************************************
 *                                            "PUBLIC" FUNCTION DEFINITIONS
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
 *                                          value, pass it to fat_print_boot_sector_error(err). If the BPB instance's
 *                                          members are successfully set then BOOT_SECTOR_VALID is returned. Any other
 *                                          returned value indicates a failure. 
 * 
 * Note        : This function DOES NOT set the values a physical FAT volume's Bios Parameter Block as would be 
 *               required during formatting of a FAT volume. This module can only read a FAT volume's contents and does
 *               not have the capability to modify anything on the volume itself, this includes formatting a volume.
***********************************************************************************************************************
*/

uint8_t 
fat_set_bios_parameter_block (BPB * bpb)
{
  uint8_t BootSector[SECTOR_LEN];
  uint8_t err;

  // 0xFFFFFFFF is returned for the boot sector location, then locating it failed.
  bpb->bootSecAddr = fat_to_disk_find_boot_sector();
  
  if (bpb->bootSecAddr != 0xFFFFFFFF)
    {
      err = fat_to_disk_read_single_sector (bpb->bootSecAddr, BootSector);
      if (err == 1) return FAILED_READ_BOOT_SECTOR;
    }
  else
    return BOOT_SECTOR_NOT_FOUND;

  // Confirm signature bytes
  if ((BootSector[SECTOR_LEN - 2] == 0x55) && (BootSector[SECTOR_LEN - 1] == 0xAA))
    {
      bpb->bytesPerSec = BootSector[12];
      bpb->bytesPerSec <<= 8;
      bpb->bytesPerSec |= BootSector[11];
      
      if (bpb->bytesPerSec != SECTOR_LEN)
        return INVALID_BYTES_PER_SECTOR;

      // secPerClus
      bpb->secPerClus = BootSector[13];

      if ((bpb->secPerClus != 1 ) && (bpb->secPerClus != 2 ) && (bpb->secPerClus != 4 ) && (bpb->secPerClus != 8) 
         && (bpb->secPerClus != 16) && (bpb->secPerClus != 32) && (bpb->secPerClus != 64) && (bpb->secPerClus != 128))
        {
          return INVALID_SECTORS_PER_CLUSTER;
        }

      bpb->rsvdSecCnt = BootSector[15];
      bpb->rsvdSecCnt <<= 8;
      bpb->rsvdSecCnt |= BootSector[14];

      bpb->numOfFats = BootSector[16];

      bpb->fatSize32 =  BootSector[39];
      bpb->fatSize32 <<= 8;
      bpb->fatSize32 |= BootSector[38];
      bpb->fatSize32 <<= 8;
      bpb->fatSize32 |= BootSector[37];
      bpb->fatSize32 <<= 8;
      bpb->fatSize32 |= BootSector[36];

      bpb->rootClus =  BootSector[47];
      bpb->rootClus <<= 8;
      bpb->rootClus |= BootSector[46];
      bpb->rootClus <<= 8;
      bpb->rootClus |= BootSector[45];
      bpb->rootClus <<= 8;
      bpb->rootClus |= BootSector[44];

      bpb->dataRegionFirstSector = bpb->bootSecAddr + bpb->rsvdSecCnt + (bpb->numOfFats * bpb->fatSize32);
      
      return BOOT_SECTOR_VALID;
    }
  else 
    return NOT_BOOT_SECTOR;
}



/*
***********************************************************************************************************************
 *                                             PRINT BOOT SECTOR ERROR FLAG
 * 
 * Description : Call this function to print the error flag returned by the function fat_set_bios_parameter_block(). 
 * 
 * Argument    : err    Boot Sector Error flag returned the function fat_set_bios_parameter_block().
***********************************************************************************************************************
*/

void 
fat_print_boot_sector_error (uint8_t err)
{  
  switch (err)
  {
    case BOOT_SECTOR_VALID : 
      print_str ("BOOT_SECTOR_VALID ");
      break;
    case CORRUPT_BOOT_SECTOR :
      print_str ("CORRUPT_BOOT_SECTOR ");
      break;
    case NOT_BOOT_SECTOR :
      print_str ("NOT_BOOT_SECTOR ");
      break;
    case INVALID_BYTES_PER_SECTOR:
      print_str ("INVALID_BYTES_PER_SECTOR");
      break;
    case INVALID_SECTORS_PER_CLUSTER:
      print_str ("INVALID_SECTORS_PER_CLUSTER");
      break;
    case BOOT_SECTOR_NOT_FOUND:
      print_str ("BOOT_SECTOR_NOT_FOUND");
      break;
    case FAILED_READ_BOOT_SECTOR:
      print_str ("FAILED_READ_BOOT_SECTOR");
      break;
    default:
      print_str ("UNKNOWN_ERROR");
      break;
  }
}