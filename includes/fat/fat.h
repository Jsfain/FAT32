/*
 * File    : FAT.H
 * Version : 0.0.0.2
 * Author  : Joshua Fain
 * Target  : ATMega1280
 * License : MIT LICENSE 
 * Copyright (c) 2020
 * 
 * Interface for navigating / accessing contents of a FAT32 formatted volume
 * using an AVR microconstroller. These only provide READ access to the 
 * volume's contents. No WRITE/ERASE access is currently implemented.
 */

#ifndef FAT_H
#define FAT_H

#include <avr/io.h>

/*
 ******************************************************************************
 *                                    MACROS      
 ******************************************************************************
 */

// macros for some common file size units
#ifndef BYTE
#define BYTE  1
#endif//BYTE 

#ifndef KB
#define KB    1000
#endif//KB

#ifndef MB
#define MB    1000000
#endif//MB

#ifndef GB
#define GB    1000000000
#endif//GB

//
// The two sets of MACROS below (FAT ENTRY ATTRIBUTE and LONG NAME POSITIONS)
// are flags and masks that directly correspond to bit and byte settings within 
// a FAT entry.
//

/* 
 * ----------------------------------------------------------------------------
 *                                                         FAT ENTRY ATTRIBUTES
 *
 * Description : These flags correspond to the attribute byte (byte 11) set in 
 *               a single 32 byte FAT entry. These settings are used to
 *               describe the entry type, access, etc... 
 * 
 * Notes       : If the 4 lowest bits/flags are set in the Attribute Byte of a 
 *               FAT entry, then the entry is part of a long name. To check,
 *               use LN_ATTR_MASK.
 * ----------------------------------------------------------------------------
 */
#define READ_ONLY_ATTR       0x01
#define HIDDEN_ATTR          0x02
#define SYSTEM_ATTR          0x04
#define VOLUME_ID_ATTR       0x08
#define DIR_ENTRY_ATTR       0x10
#define ARCHIVE_ATTR         0x20
#define LN_ATTR_MASK         0x0F      // OR'd 4 lowest bits mean long name

// position of attribute byte relative to first byte of a short name entry
#define ATTR_BYTE_OFFSET     11

// If first byte of entry is set to this, then entry is marked for deletion. 
#define DELETED_ENTRY_TOKEN  0xE5

/* 
 * ----------------------------------------------------------------------------
 *                                                          LONG NAME POSITIONS
 *
 * Description : These are specific to long name entries, and should be used 
 *               on the entry's first byte. They are used to determine how many 
 *               32-byte entries the long name requires and where a particular  
 *               entry lies in the order of its long name entries. 
 * 
 * Notes       : 1) If LN_LAST_ENTRY_FLAG is set in the first byte of a ln 
 *                  entry then that entry is the last entry of the ln.
 *               2) Use the LN_ORD_MASK on the first byte of a ln entry to
 *                  determine this entry's number (i.e. where it lies in the
 *                  sequence of its associated long name entries).
 * ----------------------------------------------------------------------------
 */
#define LN_LAST_ENTRY_FLAG   0x40  
#define LN_ORD_MASK          0x3F

/* 
 * ----------------------------------------------------------------------------
 *                                                ENTRY and SECTOR BYTE LENGTHS
 *
 * Description : Defines the byte lengths of FAT32 sector and directory entry.
 *       
 * Notes       : 1) ENTRY_LEN must always be 32.
 *               2) SECTOR_LEN should match the bios parameter block's 'bytes 
 *                  per sector' (BPS) field.
 * 
 * Warning     : Though the BPS field value can be different than 512, in this 
 *               implementation. anything other than 512 will not work. 
 * ----------------------------------------------------------------------------
 */
#define ENTRY_LEN                 32   // FAT entry byte length

#ifndef SECTOR_LEN
#define SECTOR_LEN                512  // must always be 512 here
#endif//SECTOR_LEN

#define FIRST_SECTOR_POS_IN_CLUS  0
#define FIRST_ENTRY_POS_IN_SEC    0
#define LAST_ENTRY_POS_IN_SEC     SECTOR_LEN - ENTRY_LEN


/* 
 * ----------------------------------------------------------------------------
 *                                                              FAT ERROR FLAGS
 *
 * Description : Error Flags returned by various FAT functions.
 * ----------------------------------------------------------------------------
 */
#define SUCCESS              0x00
#define INVALID_FILE_NAME    0x01
#define INVALID_DIR_NAME     0x02
#define FILE_NOT_FOUND       0x04
#define DIR_NOT_FOUND        0x08
#define END_OF_FILE          0x10
#define END_OF_DIRECTORY     0x20
#define CORRUPT_FAT_ENTRY    0x40
#ifndef FAILED_READ_SECTOR
#define FAILED_READ_SECTOR   0x80      // also defined in fat_to_disk.h
#endif//FAILED_READ_SECTOR

/* 
 * ----------------------------------------------------------------------------
 *                                                        FAT ENTRY FIELD FLAGS
 *
 * Description : Flags to specify which entries should be printed by any FAT 
 *               function that prints directory entries to a screen, e.g. 
 *               fat_PrintDir().
 * ----------------------------------------------------------------------------
 */
#define SHORT_NAME           0x01
#define LONG_NAME            0x02
#define HIDDEN               0x04
#define CREATION             0x08
#define LAST_ACCESS          0x10
#define LAST_MODIFIED        0x20
#define TYPE                 0x40 
#define FILE_SIZE            0x80
#define ALL                  0xFF

/* 
 * ----------------------------------------------------------------------------
 *                                                               STRING LENGTHS
 *
 * Description : These values are used to specify the max length of strings and
 *               character arrays associated with long / short names and paths.             
 * ----------------------------------------------------------------------------
 */
#define PATH_STR_LEN_MAX     100       // max length of path string
#define LN_STR_LEN_MAX       100       // max length of long name string
#define SN_NAME_STR_LEN      9         // num bytes occupied by sn + null
#define SN_EXT_STR_LEN       4         // num bytes occupied by sn ext + null
#define SN_STR_LEN           13        // 8 + 3 (sn + ext) and '.' and null

// Unit used when printing an entry's file size. Set to BYTE or KB 
#define FS_UNIT         BYTE               

// Value in the last FAT cluster index of a directory or file.
#define END_CLUSTER            0x0FFFFFFF

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
 *                  passing it to fat_SetDirToRoot.
 *               2) Most FAT functions require an instance of this struct to be
 *                  previously set and passed to it.
 *               3) This is primarily used as a 'Current Working Directory'.
 * 
 * Warnings    : All members of an instance of this struct must correspond to
 *               the same valid FAT directory. If not, then unexpected results 
 *               will occur when trying to navigate and/or print directory 
 *               names, contents, entries, and files. As such, members of an 
 *               instance of this struct should only be set through the FAT
 *               FAT functions.
 * ----------------------------------------------------------------------------
 */
typedef struct
{
  char lnStr[LN_STR_LEN_MAX + 1];      // directory long name
  char lnPathStr[PATH_STR_LEN_MAX + 1];// directory long name path
  char snStr[SN_NAME_STR_LEN];         // directory short name
  char snPathStr[PATH_STR_LEN_MAX];    // directory short name path
  uint32_t fstClusIndx;                // index of directory's first cluster
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
 *                  passing it to fat_InitEntry.
 *               2) Most of the FAT functions require an instance of this
 *                  struct to passed to it.
 * 
 * Warnings    : Members of an instance of this struct should never be set
 *               manually, but only by passing it to the FAT functions.
 * ----------------------------------------------------------------------------
 */
typedef struct 
{
  char lnStr[LN_STR_LEN_MAX + 1];      // entry long name
  char snStr[SN_STR_LEN];              // entry short name
  uint8_t snEnt[ENTRY_LEN];            // the 32 bytes of the short name entry
  uint32_t snEntClusIndx;              // cluster index of the sn entry
  uint8_t  snEntSecNumInClus;          // sector number in cluster of sn entry
  uint16_t nextEntPos;
} 
FatEntry;

/*
 ******************************************************************************
 *                           FUNCTION PROTOTYPES
 ******************************************************************************
 */

/* 
 * ----------------------------------------------------------------------------
 *                                                        SET TO ROOT DIRECTORY
 *                                        
 * Description : Sets instance of FatDir to the root directory.
 *
 * Arguments   : dir   - Pointer to FatDir instance to be set to root dir.
 *               bpb   - Pointer to the BPB struct instance.
 *
 * Returns     : void
 * ----------------------------------------------------------------------------
 */
void fat_SetDirToRoot(FatDir *dir, const BPB *bpb);

/*
 * ----------------------------------------------------------------------------
 *                                                 INITIALIZE FAT ENTRY TO ROOT
 *                                      
 * Description : Initialize an instance of FatEntry to the first entry of the
 *               root directory.
 * 
 * Arguments   : ent   - Pointer to the FatEntry instance to be initialized.           
 *               bpb   - Pointer to the BPB struct instance.
 * 
 * Returns     : void
 * ----------------------------------------------------------------------------
 */
void fat_InitEntry(FatEntry *ent, const BPB *bpb);

/*
 * ----------------------------------------------------------------------------
 *                                                  SET FAT ENTRY TO NEXT ENTRY 
 *                                      
 * Description : Updates current FatEntry instance to the next entry in its
 *               its directory.
 * 
 * Arguments   : currEnt   - Pointer to a FatEntry instance. Its members will 
 *                           be updated to point to the next entry. 
 *               bpb        - Pointer to the BPB struct instance.
 *
 * Returns     : A FAT Error Flag. If any value other than SUCCESS is returned 
 *               then the function was unable to update the FatEntry.
 * ----------------------------------------------------------------------------
 */
uint8_t fat_SetNextEntry(FatEntry *currEntry, const BPB *bpb);

/*
 * ----------------------------------------------------------------------------
 *                                                            SET FAT DIRECTORY
 *                                       
 * Description : Set FatDir instance to the directory specified by newDirStr.
 * 
 * Arguments   : dir         - Pointer to the FatDir instance to be set to the
 *                             new directory.             
 *               newDirStr   - Pointer to a string that specifies the name of 
 *                             the new directory.
 *               bpb        - Pointer to the BPB struct instance.
 * 
 * Returns     : A FAT Error Flag. If any value other than SUCCESS is returned 
 *               then the function was unable to update the FatEntry. 
 *  
 * Notes       : 1) This function can only set the directory to a child or the
 *                  parent of the FatDir instance (dir) when the function is
 *                  called, or reset the instance to the root directory.
 *               2) Do not include any paths (relative or absolute) in the 
 *                  newDirStr. newDirStr must be only be a directory name which
 *                  must be the name of a child directoy, or the parent, in the
 *                  current directory.
 *               3) If "." is passed as the newDirStr then the new directory
 *                  will be set to the parent of the current directory.               
 *               4) newDirStr is case-sensitive.
 *               5) newDirStr must be a long name, unless a long name does not
 *                  exist for a directory, only then can it be a short name.
 * ----------------------------------------------------------------------------
 */
uint8_t fat_SetDir(FatDir *dir, const char newDirStr[], const BPB *bpb);

/*
 * ----------------------------------------------------------------------------
 *                                            PRINT DIRECTORY ENTRIES TO SCREEN
 *                                       
 * Description : Prints a list of file and directory entries within a directory
 *               (FatDir instance) along with any specified fields.
 * 
 * Arguments   : dir        - Pointer to a FatDir instance. This directory's
 *                            entries will be printed to the screen.
 *               entFlds    - Any combination of the FAT ENTRY FIELD FLAGS.
 *                            These specify which entry types, and which of
 *                            their fields, will be printed to the screen.
 *               bpb        - Pointer to the BPB struct instance.
 *
 * Returns     : A FAT Error Flag. If any value other than END_OF_DIRECTORY is
 *               returned then there was an issue.
 *  
 * Notes       : 1) LONG_NAME and/or SHORT_NAME must be passed in the entFld
 *                  argument. If not, then no entries will be printed.
 *               2) If both LONG_NAME and SHORT_NAME are passed then both
 *                  the short and long names for each entry will be printed.
 *                  For any entry that does not have a long name, the short 
 *                  name is also stored in the long name string of the struct
 *                  and so the short name will effectively be printed twice -
 *                  once for the long name and once for the short name.
 * ----------------------------------------------------------------------------
 */
uint8_t fat_PrintDir(const FatDir *dir, const uint8_t entFlds, const BPB *bpb);

/*
 * ----------------------------------------------------------------------------
 *                                                         PRINT FILE TO SCREEN
 *                                       
 * Description : Prints contents of any file entry to the screen. 
 * 
 * Arguments   : dir        - Pointer to a FatDir instance. This directory must
 *                            contain the entry for the file to be printed.
 *               fileStr    - Pointer to a string. This is the name of the file
 *                            who's contents will be printed.
 *               bpb        - Pointer to the BPB struct instance.
 *
 * Returns     : FAT Error Flag. If any value other than END_OF_FILE is 
 *               returned, then an issue has occurred.
 *  
 * Notes       : fileStr must be a long name unless a long name for a given
 *               entry does not exist, in which case it must be a short name.
 * ----------------------------------------------------------------------------
 */
uint8_t fat_PrintFile(const FatDir *dir, const char fileStr[], const BPB *bpb);

/*
 *-----------------------------------------------------------------------------
 *                                                         PRINT FAT ERROR FLAG
 * 
 * Description : Prints the FAT Error Flag passed as the arguement. 
 *
 * Arguments   : err   - An error flag returned by one of the FAT functions.
 * 
 * Returns     : void
 * ----------------------------------------------------------------------------
 */
void fat_PrintError(uint8_t err);

#endif //FAT_H
