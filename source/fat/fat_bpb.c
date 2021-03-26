/*
 * File       : FAT_BPB.C
 * Version    : 2.0
 * Target     : ATMega1280
 * Compiler   : AVR-GCC 9.3.0
 * Downloader : AVRDUDE 6.3
 * License    : GNU GPLv3
 * Author     : Joshua Fain
 * Copyright (c) 2020, 2021
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
 *               from a FAT volume and sets the corresponding members of an
 *               instance of the BPB struct accordingly.
 * 
 * Arguments   : bpb   - Pointer to an instance of a BPB struct. This function
 *                       will set the members of this instance.
 * 
 * Returns     : Boot Sector Error Flag. If any value other than BPB_VALID is
 *               returned then setting the BPB instance failed. To print, pass
 *               the returned value to fat_PrintErrorBPB().
 * 
 * Notes       : A valid BPB struct instance is a required argument of many 
 *               functions that access the FAT volume, therefore this function 
 *               should be called first, before implementing any other parts of
 *               the FAT module.
 * 
 * Limitation  : Currently will only work if Boot Sector is block 0 on SD Card.
 * ----------------------------------------------------------------------------
 */
uint8_t fat_SetBPB(BPB *bpb)
{
  uint8_t bootSecArr[SECTOR_LEN], err; 

  // Locate boot sector address on the disk. 
  uint32_t bootSecAddr = FATtoDisk_FindBootSector();
  if (bootSecAddr != FAILED_FIND_BOOT_SECTOR)
  {
    // load data from boot sector into bootSecArr.
    err = FATtoDisk_ReadSingleSector(bootSecAddr, bootSecArr);
    if (err == FAILED_READ_SECTOR) 
      return FAILED_READ_BPB;
  }
  else
    return BPB_NOT_FOUND;
  
  // 
  // Confirm the sector loaded is the Boot Sector by checking the signature
  // bytes - the last two bytes of sector. If true, then begin loading the 
  // necessary BPB field values into their respective BPB struct members.
  // 
  if (bootSecArr[SECTOR_LEN - 2] == BS_SIGN_1 
      && bootSecArr[SECTOR_LEN - 1] == BS_SIGN_2)
  {
    bpb->bytesPerSec = bootSecArr[BYTES_PER_SEC_POS_MSB];      
    bpb->bytesPerSec <<= 8;                 
    bpb->bytesPerSec |= bootSecArr[BYTES_PER_SEC_POS_LSB];
    
    // Bytes Per Sector must be the same as SECTOR_LEN
    if (bpb->bytesPerSec != SECTOR_LEN)
      return INVALID_BYTES_PER_SECTOR;

    bpb->secPerClus = bootSecArr[SEC_PER_CLUS_POS];

    // check that secPerClus is a valid value.   
    if (!CHK_VLD_SEC_PER_CLUS(bpb->secPerClus))
      return INVALID_SECTORS_PER_CLUSTER;
    
    // number of reserved sectors
    bpb->rsvdSecCnt = bootSecArr[RSVD_SEC_CNT_POS_MSB];
    bpb->rsvdSecCnt <<= 8;
    bpb->rsvdSecCnt |= bootSecArr[RSVD_SEC_CNT_POS_LSB];

    // number of FATs
    bpb->numOfFats = bootSecArr[NUM_FATS_POS];

    // Size of a single FAT
    bpb->fatSize32 =  bootSecArr[FAT32_SIZE_POS4];
    bpb->fatSize32 <<= 8;
    bpb->fatSize32 |= bootSecArr[FAT32_SIZE_POS3];
    bpb->fatSize32 <<= 8;
    bpb->fatSize32 |= bootSecArr[FAT32_SIZE_POS2];
    bpb->fatSize32 <<= 8;
    bpb->fatSize32 |= bootSecArr[FAT32_SIZE_POS1];

    // Root directory cluster index
    bpb->rootClus =  bootSecArr[ROOT_CLUS_POS4];
    bpb->rootClus <<= 8;
    bpb->rootClus |= bootSecArr[ROOT_CLUS_POS3];
    bpb->rootClus <<= 8;
    bpb->rootClus |= bootSecArr[ROOT_CLUS_POS2];
    bpb->rootClus <<= 8;
    bpb->rootClus |= bootSecArr[ROOT_CLUS_POS1];

    //
    // The disk's sector address corresponding to the first sector of the FAT32
    // volume's Data Region. Since the first cluster of the Data Region is the 
    // Root Directory, this value points to the sector number of the Root Dir.
    //
    bpb->dataRegionFirstSector = bootSecAddr + bpb->rsvdSecCnt 
                               + bpb->numOfFats * bpb->fatSize32;
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
