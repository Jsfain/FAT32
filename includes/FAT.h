/*
 * File    : FAT.H
 * Version : 0.0.0.2
 * Author  : Joshua Fain
 * Target  : ATMega1280
 * License : MIT LICENSE 
 * Copyright (c) 2020
 * 
 * Interface for navigating / accessing contents of a FAT32 formatted volume
 * using an AVR microconstroller. 
 * 
 * NOTES: These only provide READ access to the volume's contents. No 
 *        WRITE/ERASE access is currently implemented.
 */

#ifndef FAT_H
#define FAT_H


#include <avr/io.h>


/*
 ******************************************************************************
 *                                    MACROS      
 ******************************************************************************
 */

//
// The first two sets of MACROS below (FAT ENTRY ATTRIBUTE and
// LONG NAME POSITIONS) are flags and masks that directly correspond to 
// bit and byte settings within a FAT entry.
//

/* 
 * ----------------------------------------------------------------------------
 *                                                         FAT ENTRY ATTRIBUTES
 *
 * Description : These flags correspond to the attribute byte (byte 11) set in 
 *               a single 32 byte FAT entry. These settings are used to
 *               describe the entry type and any permissions / restrictions. 
 * 
 * Notes       : If the 4 lowest bits/flags are set in the Attribute Byte of a 
 *               FAT entry, then the entry is part of a long name. To check,
 *               use LN_ATTR_MASK.
 * ----------------------------------------------------------------------------
 */

#define READ_ONLY_ATTR         0x01
#define HIDDEN_ATTR            0x02
#define SYSTEM_ATTR            0x04
#define VOLUME_ID_ATTR         0x08
#define DIR_ENTRY_ATTR         0x10
#define ARCHIVE_ATTR           0x20
#define LN_ATTR_MASK           0x0F


/* 
 * ----------------------------------------------------------------------------
 *                                                          LONG NAME POSITIONS
 *
 * Description : These are specific to long name entries, i.e. LN_ATTR_MASK is 
 *               set above, and should be used against byte 1 of a long name 
 *               entry. They are used to determine how many entries the full 
 *               long name requires and where a particular entry lies in the 
 *               long name entry order. 
 * 
 * Notes       : 1) If LN_LAST_ENTRY is set in the first byte of a long name 
 *                  entry then this entry is the last entry of the long name.
 * 
 *               2) Use the LN_ORD_MASK on the first byte of a long name entry
 *                  to determine this entries number (i.e. where it lies in 
 *                  sequence of long name entries).
 * ----------------------------------------------------------------------------
 */

#define LN_LAST_ENTRY          0x40  
#define LN_ORD_MASK            0x3F


/* 
 * ----------------------------------------------------------------------------
 *                                                ENTRY and SECTOR BYTE LENGTHS
 *
 * Description : Defines the byte lengths of a FAT32 sector and an entry in a
 *               FAT32 directory. 
 *       
 * Notes       : 1) ENTRY_LEN must always be 32. 
 * 
 *               2) SECTOR_LEN should match the bios parameter block's 'bytes 
 *                  per sector' (BPS) field. 
 * 
 * Warning     : Although the BPS field value can be different than 512, in 
 *               the current implementation if it is anything other than 512
 *               then this will cause unexpected results in any program built
 *               using this module. 
 * ----------------------------------------------------------------------------
 */
#define ENTRY_LEN              32                /* FAT entry byte length */

#ifndef SECTOR_LEN
#define SECTOR_LEN             512               /* must always be 512 here */
#endif

// Value at the FAT index location of the last cluster in a FAT dir or file.
#define END_CLUSTER            0x0FFFFFFF


/* 
 * ----------------------------------------------------------------------------
 *                                          LONG NAME SECTOR DISTRIBUTION FLAGS
 *
 * Description : Flags used by FAT functions to indicate if a long name exists
 *               and how it, and its short name entry, are distributed between
 *               adjacent sectors.
 * ----------------------------------------------------------------------------
 */

#define LN_EXISTS              0x01
#define LN_CROSS_SEC           0x02
#define LN_LAST_SEC_ENTRY      0x04


/* 
 * ----------------------------------------------------------------------------
 *                                                              FAT ERROR FLAGS
 *
 * Description : Error Flags returned by various FAT functions.
 * ----------------------------------------------------------------------------
 */

#define SUCCESS                0x00
#define INVALID_FILE_NAME      0x01
#define INVALID_DIR_NAME       0x02
#define FILE_NOT_FOUND         0x04
#define DIR_NOT_FOUND          0x08
#define END_OF_FILE            0x10
#define END_OF_DIRECTORY       0x20
#define CORRUPT_FAT_ENTRY      0x40
#define FAILED_READ_SECTOR     0x80


/* 
 * ----------------------------------------------------------------------------
 *                                                        FAT ENTRY FIELD FLAGS
 *
 * Description : Flags to specify which entries should be printed by any FAT 
 *               function that prints directory entries to a screen.
 * ----------------------------------------------------------------------------
 */

#define SHORT_NAME             0x01
#define LONG_NAME              0x02
#define HIDDEN                 0x04
#define CREATION               0x08
#define LAST_ACCESS            0x10
#define LAST_MODIFIED          0x20
#define TYPE                   0x40 
#define FILE_SIZE              0x80
#define ALL                    0xFF


// set to any value < 256.
#define PATH_STRING_LEN_MAX    100         /* max length of path string */
#define LN_STRING_LEN_MAX      100         /* max length of long name string */


/*
 ******************************************************************************     
 *                                 STRUCTS      
 ******************************************************************************
 */

/* 
 * ----------------------------------------------------------------------------
 *                                                         FAT DIRECTORY STRUCT
 *
 * Description : Struct used to hold some parameters corresponding to a FAT
 *               directory.
 *       
 * Notes       : 1) Any instance of this struct must first be initialized by 
 *                  passing it to fat_setDirToRoot(), which will set it to the
 *                  root directory.
 * 
 *               2) Most of the FAT functions require an instance of this
 *                  struct to be previously set and passed to it.
 * 
 *               3) This is primarily used as a 'Current Working Directory'.
 * 
 * Warnings    : 1) All members of an instance of this struct must correspond 
 *                  to the same valid FAT directory. If not then unexpected 
 *                  results will occur when trying to navigate and/or print
 *                  directory names, contents, entries, and files.
 * 
 *               2) Members of an instance of this struct should never be set
 *                  manually, but only by passing it to FAT functions.
 * ----------------------------------------------------------------------------
 */

typedef struct
{
  char     longName[LN_STRING_LEN_MAX];          
  char     longParentPath[PATH_STRING_LEN_MAX];
  char     shortName[9];
  char     shortParentPath[PATH_STRING_LEN_MAX];
  uint32_t FATFirstCluster;
} 
FatDir;


/* 
 * ----------------------------------------------------------------------------
 *                                                         FAT DIRECTORY STRUCT
 *
 * Description : Instances of this struct are used to locate entries within a 
 *               FAT directory.
 *       
 * Notes       : 1) Any instance of this struct should first be initialized by
 *                  passing it to fat_initEntry().
 * 
 *               2) Most of the FAT functions require an instance of this
 *                  struct to passed to it.
 * 
 * Warnings    : Members of an instance of this struct should never be set
 *               manually, but only by passing it to the FAT functions.
 * ----------------------------------------------------------------------------
 */

typedef struct
{
  char    longName[LN_STRING_LEN_MAX];
  char    shortName[13];
  uint8_t snEnt[32]; 
  
  // these members save the entry's location for use when 
  // navigating a directory using fat_setNextEntry()
  uint32_t snEntClusIndx; 
  uint8_t  snEntSecNumInClus; 
  uint16_t entPos;  
  uint8_t  lnFlags;
  uint16_t snPosCurrSec;
  uint16_t snPosNextSec;           
}
FatEntry;


/*
 ******************************************************************************
 *                           FUNCTION PROTOTYPES
 ******************************************************************************
 */

/* 
 * ----------------------------------------------------------------------------
 *                                                           SET ROOT DIRECTORY
 *                                        
 * Description : Sets an instance of FatDir to the root direcotry.
 *
 * Arguments   : dir     Pointer to the FatDir instance to be set to the root
 *                       directory.
 *
 *               bpb     Pointer to a valid instance of a BPB struct.
 *
 * Returns     : void
 * ----------------------------------------------------------------------------
 */

void fat_setDirToRoot(FatDir * dir, BPB * bpb);


/*
 * ----------------------------------------------------------------------------
 *                                                         INITIALIZE FAT ENTRY
 *                                      
 * Description : Initializes an entry of FatEntry.
 * 
 * Arguments   : ent     Pointer to the FatEntry instance to be initialized.
 *            
 *               bpb     Pointer to a valid instance of a BPB struct.
 * 
 * Returns     : void
 * ----------------------------------------------------------------------------
 */

void fat_initEntry(FatEntry * ent, BPB * bpb);


/*
 * ----------------------------------------------------------------------------
 *                                                  SET FAT ENTRY TO NEXT ENTRY 
 *                                      
 * Description : Updates the FatEntry instance, currEnt, to the next entry in 
 *               the current directory (currDir).
 * 
 * Arguments   : currDir     Current directory. Pointer to a FatDir instance.
 * 
 *               currEnt     Current entry. Pointer to a FatEntry instance.
 * 
 *               bpb         Pointer to a valid instance of a BPB struct.
 *
 * Returns     : A FAT Error Flag. If any value other than SUCCESS is returned 
 *               then the function was unable to update the FatEntry.
 * -----------------------------------------------------------------------------
 */

uint8_t fat_setNextEntry (FatDir * currDir, FatEntry * currEntry, BPB * bpb);


/*
 * ----------------------------------------------------------------------------
 *                                                            SET FAT DIRECTORY
 *                                       
 * Description : Set a FatDir instance, dir, to the directory specified by 
 *               newDirStr.
 * 
 * Arguments   : dir           Pointer to the FatDir instance to be set to the
 *                             new directory.
 *             
 *               newDirStr     Pointer to a string that specifies the name of 
 *                             the new directory.
 * 
 *               bpb           Pointer to a valid instance of a BPB struct.
 *
 * Returns     : A FAT Error Flag. If any value other than SUCCESS is returned 
 *               then the function was unable to update the FatEntry.
 *  
 * Notes       : 1) This function can only set the directory to a child, or
 *                  the parent of the current instance of FatDir when the 
 *                  function is called.
 * 
 *               2) Do not include any paths (relative or absolute) in the 
 *                  newDirStr. newDirStr must be only be a directory name which
 *                  must be the name of a child directoy, or the parent, in the
 *                  current directory.
 * 
 *               3) If "." is passed as the newDirStr then the new directory
 *                  will be set to the parent of the current directory.
 *               
 *               4) newDirStr is case-sensitive.
 * 
 *               5) newDirStr must be a long name, unless a long name does not
 *                  exist for a directory. Only in this case can it be a short
 *                  name.
 * ----------------------------------------------------------------------------
 */

uint8_t fat_setDir (FatDir * dir, char * newDirStr, BPB * bpb);


/*
 * ----------------------------------------------------------------------------
 *                                          PRINT DIRECTORY ENTRIES TO A SCREEN
 *                                       
 * Description : Prints the contents of a directory, i.e. lists the file and
 *               directory entries of a FatDir instance.
 * 
 * Arguments   : dir         Pointer to a FatDir instance. This directory's
 *                           entries will be printed to the screen.
 *             
 *               entFld      Any combination of the Entry Field Flags. These
 *                           will specify which entry types, and which of their 
 *                           fields, will be printed to the screen.
 *               
 *               bpb         Pointer to a valid instance of a BPB struct.
 *
 * Returns     : A FAT Error Flag. If any value other than END_OF_DIRECTORY is
 *               returned then there was an issue.
 *  
 * Notes       : LONG_NAME and/or SHORT_NAME must be passed in the entFld
 *               argument. If not, then no entries will be printed.
 * ----------------------------------------------------------------------------
 */

uint8_t fat_printDir (FatDir * dir, uint8_t entFld, BPB * bpb);


/*
 * ----------------------------------------------------------------------------
 *                                                         PRINT FILE TO SCREEN
 *                                       
 * Description : Prints the contents of a file to the screen.
 * 
 * Arguments   : dir             Pointer to a FatDir instance. This directory
 *                               must contain the entry for the file that will
 *                               be printed.
 *             
 *               fileNameStr     Pointer to a string. This is the name of the 
 *                               file who's contents will be printed.
 *               
 *               bpb             Pointer to a valid instance of a BPB struct.
 *
 * Returns     : A FAT Error Flag. If any value other than END_OF_FILE is 
 *               returned, then an issue occurred.
 *  
 * Notes       : fileNameStr must be a long name unless a long name for a given
 *               entry does not exist, in which case it must be a short name.
 * ----------------------------------------------------------------------------
 */

uint8_t fat_printFile (FatDir * dir, char * fileNameStr, BPB * bpb);


/*
 *-----------------------------------------------------------------------------
 *                                                         PRINT FAT ERROR FLAG
 * 
 * Description : Prints the FAT Error Flag passed as the arguement. 
 *
 * Arguments   : err     An error flag returned by one of the FAT functions.
 * 
 * Returns     : void
 * ----------------------------------------------------------------------------
 */

void fat_printError(uint8_t err);

#endif //FAT_H
