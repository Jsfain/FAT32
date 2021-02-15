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
 * Returns     : Boot Sector Error Flag. If any value other than BPB_VALID is
 *               returned then setting the BPB instance failed. To print, pass
 *               to the returned value to fat_PrintErrorBPB().
 * 
 * Notes       : A valid BPB struct instance is a required argument of many 
 *               functions that access the FAT volume, therefore this function 
 *               should be called first, before implementing any other parts of
 *               the FAT module.
 * 
 * Limitation  : Only works if Boot Sector, which contains the BPB, is sector 0 
 *               on the physical disk. 
 * ----------------------------------------------------------------------------
 */
uint8_t fat_SetBPB(BPB *bpb)
{
  // array to hold contents of the Boot Sector, which contains the BPB.
  uint8_t BootSector[SECTOR_LEN];    
  uint8_t err;

  // Locate boot sector address on the disk. 
  bpb->bootSecAddr = FATtoDisk_FindBootSector();
  if (bpb->bootSecAddr != FAILED_FIND_BOOT_SECTOR)
  {
    // load the from boot sector into the BootSector array.
    err = FATtoDisk_ReadSingleSector(bpb->bootSecAddr, BootSector);
    if (err == 1) 
      return FAILED_READ_BPB;
  }
  else
    return BPB_NOT_FOUND;
  
  //
  // The last 2 bytes of the Boot Sector are signature bytes. The values of
  // of these must be 0x55 and 0xAA.
  //
  if (BootSector[SECTOR_LEN - 2] == 0x55 && BootSector[SECTOR_LEN - 1] == 0xAA)
  {
    // bytes 11 and 12 of BS / BPB contain the Bytes Per Sector value.
    bpb->bytesPerSec = BootSector[12];      
    bpb->bytesPerSec <<= 8;                 
    bpb->bytesPerSec |= BootSector[11];
    
    // currently Bytes Per Sector must 512 (=SECTOR_LEN).
    if (bpb->bytesPerSec != SECTOR_LEN)
      return INVALID_BYTES_PER_SECTOR;

    // byte 13 of the BS / BPB is the Sectors Per Cluster value
    bpb->secPerClus = BootSector[13];
    
    // Only numbers listed below are valid for the Sectors Per Cluster value.
    if (   bpb->secPerClus != 1  && bpb->secPerClus != 2 
        && bpb->secPerClus != 4  && bpb->secPerClus != 8  
        && bpb->secPerClus != 16 && bpb->secPerClus != 32
        && bpb->secPerClus != 64 && bpb->secPerClus != 128)
    {
      return INVALID_SECTORS_PER_CLUSTER;
    }

    // bytes 14 and 15 of the BS / BPB are the Reserved Sector Count
    bpb->rsvdSecCnt = BootSector[15];
    bpb->rsvdSecCnt <<= 8;
    bpb->rsvdSecCnt |= BootSector[14];

    // byte 16 of the BS / BPB give the total number of FAT in the volume
    bpb->numOfFats = BootSector[16];

    //
    // bytes 36 to 39 of the BS / BPB give the size of a single FAT in a FAT32
    // volume. The size is the total sector count required by a single FAT.
    //
    bpb->fatSize32 =  BootSector[39];
    bpb->fatSize32 <<= 8;
    bpb->fatSize32 |= BootSector[38];
    bpb->fatSize32 <<= 8;
    bpb->fatSize32 |= BootSector[37];
    bpb->fatSize32 <<= 8;
    bpb->fatSize32 |= BootSector[36];

    // 
    // bytes 44 to 47 of the BS / BPB give the cluster number of the root dir
    // in the FAT32 volume. This value is the index of the cluster in the FAT.
    // The value should be 2 or the first usable (not bad) cluster available
    // after that.
    //
    bpb->rootClus =  BootSector[47];
    bpb->rootClus <<= 8;
    bpb->rootClus |= BootSector[46];
    bpb->rootClus <<= 8;
    bpb->rootClus |= BootSector[45];
    bpb->rootClus <<= 8;
    bpb->rootClus |= BootSector[44];

    //
    // The absolute physical disk's sector number/address corresponding to the
    // first sector of the FAT32 volume's Data Region. Since the first cluster
    // of the Data Region is the Root Directory, this value points to the 
    // absolute physical sector number of the Root Directory. The value is
    // calculated from BPB parameter values.
    //
    bpb->dataRegionFirstSector = bpb->bootSecAddr + bpb->rsvdSecCnt 
                                  + (bpb->numOfFats * bpb->fatSize32);
    return BPB_VALID;
  }
  else 
    return NOT_BPB;
}

/*
 * ----------------------------------------------------------------------------
 *                                       PRINT BIOS PARAMETER BLOCK ERROR FLAGS 
 * 
 * Description : Print the Bios Parameter Block Error Flag. 
 * 
 * Arguments   : err     BPB error flag(s).
 * 
 * Returns     : void
 * ----------------------------------------------------------------------------
 */
void fat_PrintErrorBPB(uint8_t err)
{  
  switch(err)
  {
    case BPB_VALID:
      print_Str("BPB_VALID ");
      break;
    case CORRUPT_BPB:
      print_Str("CORRUPT_BPB ");
      break;
    case NOT_BPB:
      print_Str("NOT_BPB ");
      break;
    case INVALID_BYTES_PER_SECTOR:
      print_Str("INVALID_BYTES_PER_SECTOR");
      break;
    case INVALID_SECTORS_PER_CLUSTER:
      print_Str("INVALID_SECTORS_PER_CLUSTER");
      break;
    case BPB_NOT_FOUND:
      print_Str("BPB_NOT_FOUND");
      break;
    case FAILED_READ_BPB:
      print_Str("FAILED_READ_BPB");
      break;
    default:
      print_Str("UNKNOWN_ERROR");
      break;
  }
}
