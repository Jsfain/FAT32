/*
 * File       : FAT_BPB.H
 * Version    : 2.0
 * License    : GNU GPLv3
 * Author     : Joshua Fain
 * Copyright (c) 2020 - 2025
 * 
 * Interface for accessing and storing the values of the boot sector / bios 
 * parameter block of a FAT32 formatted volume.
 */

#ifndef FAT_BPB_H
#define FAT_BPB_H

/*
 ******************************************************************************
 *                                    MACROS      
 ******************************************************************************
 */

/* 
 * ----------------------------------------------------------------------------
 *                                                                SECTOR LENGTH
 *
 * Description : The expected byte length of FAT32 sector. 
 *       
 * Notes       : (1) Value must match the BPB's 'bytes per sector field
 *               (2) This value should be set to 512 for the current
 *                   implementation to work.
 * ----------------------------------------------------------------------------
 */
#ifndef SECTOR_LEN
#define SECTOR_LEN                      512
#endif//SECTOR_LEN

/* 
 * ----------------------------------------------------------------------------
 *                                             BIOS PARAMETER BLOCK ERROR FLAGS
 *
 * Description : Flags returned by fat_SetBPB.
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
 * ----------------------------------------------------------------------------
 *                                               BIOS PARAMETER FIELD POSITIONS
 *
 * Description : Positions in the BPB of the corresponding fields. Only those
 *               fields necessary for the BPB struct are provided.
 * ----------------------------------------------------------------------------
 */
#define BYTES_PER_SEC_POS_LSB  11
#define BYTES_PER_SEC_POS_MSB  12
#define SEC_PER_CLUS_POS       13
#define RSVD_SEC_CNT_POS_LSB   14
#define RSVD_SEC_CNT_POS_MSB   15
#define NUM_FATS_POS           16
#define FAT32_SIZE_POS1        36
#define FAT32_SIZE_POS2        37
#define FAT32_SIZE_POS3        38
#define FAT32_SIZE_POS4        39
#define ROOT_CLUS_POS1         44
#define ROOT_CLUS_POS2         45
#define ROOT_CLUS_POS3         46
#define ROOT_CLUS_POS4         47


// Returns True if Sectors Per Cluster is a valid value and false otherwise.
#define CHK_VLD_SEC_PER_CLUS(SPC)  ((SPC == 1)  || (SPC == 2)  || (SPC == 4)  \
                                 || (SPC == 8)  || (SPC == 16) || (SPC == 32) \
                                 || (SPC == 64) || (SPC == 128))

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
 * Notes       : dataRegionFirstSector is not a BPB field is a value calculated
 *               from the BPB values that is used frequently.
 * ----------------------------------------------------------------------------
 */
typedef struct
{
  uint8_t  secPerClus;
  uint8_t  numOfFats;
  uint16_t bytesPerSec;
  uint16_t rsvdSecCnt;
  uint32_t fatSize32;
  uint32_t rootClus;
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
//void fat_PrintErrorBPB(uint8_t err);

#endif // FAT_BPB_H
