/*
 * File    : FAT_BPB.C
 * Version : 0.0.0.2
 * Author  : Joshua Fain
 * Target  : ATMega1280
 * License : MIT
 * Copyright (c) 2020 Joshua Fain
 * 
 * Implementation of FAT_BPB.H
 */


#include <string.h>
#include <avr/io.h>
#include "prints.h"
#include "usart0.h"
#include "fat_bpb.h"
#include "fat_to_disk.h"


/*
 ******************************************************************************
 *                                   FUNCTIONS   
 ******************************************************************************
 */

/*
 * ----------------------------------------------------------------------------
 *                                                       SET BPB STRUCT MEMBERS 
 *                                         
 * Description : Gets values of the Bios Parameter Block / Boot Sector fields 
 *               from a FAT32 volume and sets the corresponding members of an
 *               instance of the BPB struct accordingly.
 * 
 * Arguments   : bpb     Pointer to an instance of a BPB struct. This function
 *                       will set the members of this instance.
 * 
 * Returns     : Boot Sector Error Flag. If any value other than
 *               BOOT_SECTOR_VALID is returned then setting the BPB instance 
 *               failed. To print, pass to fat_printBootSectorError().
 * 
 * Notes       : A valid BPB struct instance is a required argument of many 
 *               functions that access the FAT volume, therefore this function 
 *               should be called first, before implementing any other parts of
 *               the FAT module.
 * ----------------------------------------------------------------------------
 */

uint8_t 
fat_setBPB (BPB * bpb)
{
  uint8_t BootSector[SECTOR_LEN];
  uint8_t err;

  // Locate boot sector address on the disk. 
  bpb->bootSecAddr = FATtoDisk_findBootSector();
  
  // If 0xFFFFFFFF was returned for the boot sector
  // addr then locating boot sector failed.
  if (bpb->bootSecAddr != 0xFFFFFFFF)
    {
      err = FATtoDisk_readSingleSector (bpb->bootSecAddr, BootSector);
      if (err == 1) 
        return FAILED_READ_BOOT_SECTOR;
    }
  else
    return BOOT_SECTOR_NOT_FOUND;

  // Confirm signature bytes and set BPB members
  if ((BootSector[SECTOR_LEN - 2] == 0x55) 
       && (BootSector[SECTOR_LEN - 1] == 0xAA))
    {
      bpb->bytesPerSec = BootSector[12];
      bpb->bytesPerSec <<= 8;
      bpb->bytesPerSec |= BootSector[11];
      
      if (bpb->bytesPerSec != SECTOR_LEN)
        return INVALID_BYTES_PER_SECTOR;

      bpb->secPerClus = BootSector[13];

      if ((bpb->secPerClus != 1 ) && (bpb->secPerClus != 2 ) 
           && (bpb->secPerClus != 4 ) && (bpb->secPerClus != 8) 
           && (bpb->secPerClus != 16) && (bpb->secPerClus != 32) 
           && (bpb->secPerClus != 64) && (bpb->secPerClus != 128))
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

      bpb->dataRegionFirstSector = bpb->bootSecAddr + bpb->rsvdSecCnt 
                                    + (bpb->numOfFats * bpb->fatSize32);
      
      return BOOT_SECTOR_VALID;
    }
  else 
    return NOT_BOOT_SECTOR;
}


/*
 * ----------------------------------------------------------------------------
 *                                                 PRINT BOOT SECTOR ERROR FLAG 
 * 
 * Description : Print the Boot Sector Error Flag. 
 * 
 * Arguments   : err     boot sector error flag(s).
 * 
 * Returns     : void
 * ----------------------------------------------------------------------------
 */

void 
fat_printBootSectorError (uint8_t err)
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
