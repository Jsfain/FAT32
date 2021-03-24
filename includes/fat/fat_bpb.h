/*
 * File       : FAT_BPB.H
 * Version    : 2.0
 * Target     : ATMega1280
 * Compiler   : AVR-GCC 9.3.0
 * Downloader : AVRDUDE 6.3
 * License    : GNU GPLv3
 * Author     : Joshua Fain
 * Copyright (c) 2020, 2021
 * 
 * Interface for accessing and storing the values of the boot sector / bios 
 * parameter block of a FAT32 formatted volume.
 */

#ifndef FAT_BPB_H
#define FAT_BPB_H

#include <avr/io.h>

/*
 ******************************************************************************
 *                                    MACROS      
 ******************************************************************************
 */

/* 
 * ----------------------------------------------------------------------------
 *                                                                SECTOR LENGTH
 *
 * Description : The expected value of a FAT32 sector length, in bytes. 
 *       
 * Notes       : This value should match the bios parameter block's 'bytes per
 *               sector' field.
 * 
 * Warning     : Currently this value should be set to 512 for the current 
 *               implementation to work. If this does not match the bps field
 *               then this may produce unexpected results.
 * ----------------------------------------------------------------------------
 */
#ifndef SECTOR_LEN
#define SECTOR_LEN                      512
#endif//SECTOR_LEN

/* 
 * ----------------------------------------------------------------------------
 *                                             BIOS PARAMETER BLOCK ERROR FLAGS
 *
 * Description : Flags that will be returned by functions that read the BPB 
 *               from the boot sector, e.g. fat_SetBPB().
 * ----------------------------------------------------------------------------
 */
#define CORRUPT_BPB                     0x01
#define NOT_BPB                         0x02
#define INVALID_BYTES_PER_SECTOR        0x04
#define INVALID_SECTORS_PER_CLUSTER     0x08
#define BPB_NOT_FOUND                   0x10
#define BPB_VALID                       0x20
#define FAILED_READ_BPB                 0x40

/*
 ******************************************************************************
 *                                 STRUCTS      
 ******************************************************************************
 */

/* 
 * ----------------------------------------------------------------------------
 *                                                  BIOS PARAMETER BLOCK STRUCT
 *
 * Description : The members of this struct correspond to the Bios Parameter 
 *               Block fields needed by this module.
 * 
 * Notes       : Some of the members are also for values calculated from other
 *               members (BPB fields) of this struct. 
 * ----------------------------------------------------------------------------
 */
typedef struct
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
 ******************************************************************************
 *                             FUNCTION PROTOTYPES      
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
uint8_t fat_SetBPB(BPB *bpb);

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
void fat_PrintErrorBPB(uint8_t err);

#endif // FAT_BPB_H
