/*
 * File       : FAT.H
 * Version    : 2.0
 * Target     : ATMega1280
 * Compiler   : AVR-GCC 9.3.0
 * Downloader : AVRDUDE 6.3
 * License    : GNU GPLv3
 * Author     : Joshua Fain
 * Copyright (c) 2020, 2021
 * 
 * Interface for navigating / accessing contents of a FAT32 formatted volume
 * using an AVR microconstroller. These only provide READ access to the 
 * volume's contents. No WRITE/ERASE access is currently implemented.
 */

#ifndef FAT_H
#define FAT_H

/*
 ******************************************************************************
 *                                    MACROS      
 ******************************************************************************
 */

// for common file sizes
#ifndef BYTE
#define BYTE    1
#endif//BYTE 

#ifndef KILO
#define KILO    1000
#endif//KILO

#ifndef MEGA
#define MEGA    1000000
#endif//MB

#ifndef GIGA
#define GIGA    1000000000
#endif//GIGA

/* 
 * ----------------------------------------------------------------------------
 *                                                ENTRY and SECTOR BYTE LENGTHS
 *
 * Description : Defines byte lengths of a FAT32 sector and directory entry.
 *       
 * Notes       : 1) ENTRY_LEN must always be 32.
 *               2) SECTOR_LEN should match the bios parameter block's 'bytes 
 *                  per sector' (BPS) field.
 * 
 * Warning     : Though the BPS field value can be different than 512, in this 
 *               implementation, anything other than 512 will not work. 
 * ----------------------------------------------------------------------------
 */
#define ENTRY_LEN                 32             // FAT entry byte length

#ifndef SECTOR_LEN
#define SECTOR_LEN                512            // must always be 512 here
#endif//SECTOR_LEN

#define FIRST_SEC_POS_IN_CLUS     0
#define FIRST_ENT_POS_IN_SEC      0
#define LAST_ENTRY_POS_IN_SEC     SECTOR_LEN - ENTRY_LEN

/* 
 * ----------------------------------------------------------------------------
 *                                                       SHORT NAME ENTRY BYTES
 *
 * Description : These are the bytes of a 32 byte short name entry. Their 
 *               values represent the byte offset relative to the first byte of
 *               an entry.
 * 
 * Notes       : bytes 0 to 10 are the 11 character short name, 8.3 format.
 * ----------------------------------------------------------------------------
 */
// SHORT_NAME_CHARS                        bytes 0 to 10
#define ATTR_BYTE_OFFSET                   11
//#define NTRES                            12    // reserved. unused. 
#define CREATION_TIME_TENTH_BYTE_OFFSET    13    // not currently used
#define CREATION_TIME_BYTE_OFFSET_0        14
#define CREATION_TIME_BYTE_OFFSET_1        15
#define CREATION_DATE_BYTE_OFFSET_0        16
#define CREATION_DATE_BYTE_OFFSET_1        17
#define LAST_ACCESS_DATE_BYTE_OFFSET_0     18
#define LAST_ACCESS_DATE_BYTE_OFFSET_1     19
#define FST_CLUS_INDX_BYTE_OFFSET_2        20
#define FST_CLUS_INDX_BYTE_OFFSET_3        21
#define WRITE_TIME_BYTE_OFFSET_0           22
#define WRITE_TIME_BYTE_OFFSET_1           23
#define WRITE_DATE_BYTE_OFFSET_0           24
#define WRITE_DATE_BYTE_OFFSET_1           25
#define FST_CLUS_INDX_BYTE_OFFSET_0        26
#define FST_CLUS_INDX_BYTE_OFFSET_1        27
#define FILE_SIZE_BYTE_OFFSET_0            28
#define FILE_SIZE_BYTE_OFFSET_1            29
#define FILE_SIZE_BYTE_OFFSET_2            30
#define FILE_SIZE_BYTE_OFFSET_3            31

/* 
 * ----------------------------------------------------------------------------
 *                                                         FAT ENTRY ATTRIBUTES
 *
 * Description : These flags correspond to the attribute byte (byte 11) set in 
 *               a single 32 byte FAT entry. These settings are used to
 *               describe the entry type, access, etc... 
 * 
 * Notes       : If the 4 lowest bits are set, then the entry is part of a long
 *               name. To check, use LN_ATTR_MASK.
 * ----------------------------------------------------------------------------
 */
#define READ_ONLY_ATTR       0x01
#define HIDDEN_ATTR          0x02
#define SYSTEM_ATTR          0x04
#define VOLUME_ID_ATTR       0x08
#define DIR_ENTRY_ATTR       0x10
#define ARCHIVE_ATTR         0x20
#define LN_ATTR_MASK         0x0F    // OR'd 4 lowest bits - entry is long name

/* 
 * ----------------------------------------------------------------------------
 *                                                       DATE TIME CALCULATIONS
 *
 * Description : The CALC macro functions are used to calculate the date/time 
 *               values from the short name date/time byte fields (e.g. last 
 *               access date). The mask macros are used to extract the
 *               necessary bits from the date/time bytes to calculate the value
 *               for the relevant field. 
 * ----------------------------------------------------------------------------
 */
#define MONTH_MASK           0x01E0
#define DAY_MASK             0x001F
#define YEAR_MASK            0xFE00
#define HOUR_MASK            0xF800
#define MIN_MASK             0x07E0
#define SEC_MASK             0x001F

#define MONTH_CALC(X)        (((X) & MONTH_MASK) >> 5)
#define DAY_CALC(X)          ((X) & DAY_MASK)
#define YEAR_CALC(X)         (1980 + (((X) & YEAR_MASK) >> 9))
#define HOUR_CALC(X)         (((X) & HOUR_MASK) >> 11)
#define MIN_CALC(X)          (((X) & MIN_MASK) >> 5)
#define SEC_CALC(X)          (2 * ((X) & SEC_MASK))

/* 
 * ----------------------------------------------------------------------------
 *                                                        LONG NAME ENTRY BYTES
 *
 * Description : The chars of a long name found in a single 32-byte entry are 
 *               found in three separate ranges of bytes. The values below
 *               represent the start and end byte offsets of these three 
 *               ranges. A long name may be distributed among multiple 32-byte
 *               entries.
 * ----------------------------------------------------------------------------
 */
#define LN_CHAR_RANGE_1_BEGIN      1
#define LN_CHAR_RANGE_1_END       11
#define LN_CHAR_RANGE_2_BEGIN     14
#define LN_CHAR_RANGE_2_END       26
#define LN_CHAR_RANGE_3_BEGIN     28
#define LN_CHAR_RANGE_3_END       32

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
#define LN_LAST_ENTRY_FLAG     0x40  
#define LN_ORD_MASK            0x3F

/* 
 * ----------------------------------------------------------------------------
 *                                                    MISC BYTES, MASKS, TOKENS
 * ----------------------------------------------------------------------------
 */
// If the first byte of entry is set to this, then entry is marked for deletion 
#define DELETED_ENTRY_TOKEN     0xE5

// Value in the last FAT cluster index of a directory or file.
#define END_CLUSTER             0x0FFFFFFF

// value of the last char in std ASCII char set.
#define LAST_STD_ASCII_CHAR     127  

// 4 bytes for FAT32
#define BYTES_PER_INDEX         4  

// unit used when printing an entry's file size. Set to BYTE or KB 
#define FS_UNIT                 BYTE   

/* 
 * ----------------------------------------------------------------------------
 *                                                              FAT ERROR FLAGS
 *
 * Description : Error Flags returned by various FAT functions.
 * ----------------------------------------------------------------------------
 */
#define SUCCESS                0x00
#define INVALID_NAME           0x01
#define FILE_NOT_FOUND         0x04
#define DIR_NOT_FOUND          0x08
#define END_OF_FILE            0x10
#define END_OF_DIRECTORY       0x20
#define CORRUPT_FAT_ENTRY      0x40
#ifndef FAILED_READ_SECTOR     
#define FAILED_READ_SECTOR     0x80 // also defined in fat_to_disk.h
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
#define SHORT_NAME        0x01
#define LONG_NAME         0x02
#define HIDDEN            0x04
#define CREATION          0x08
#define LAST_ACCESS       0x10
#define LAST_MODIFIED     0x20
#define TYPE              0x40 
#define FILE_SIZE         0x80
#define ALL               0xFF

/* 
 * ----------------------------------------------------------------------------
 *                                                               STRING LENGTHS
 *
 * Description : These values are used to specify the max length of strings and
 *               character arrays associated with long / short names and paths.             
 * ----------------------------------------------------------------------------
 */
#define PATH_STR_LEN_MAX     100       // max len of path string + null
#define LN_STR_LEN_MAX       100       // max len of ln string + null

// for the 8.3 format of a short name.
#define SN_NAME_CHAR_LEN       8       // max num chars in name of sn
#define SN_EXT_CHAR_LEN        3       // max num chars in extension of sn

// + 1 for '.' short name / extension separator.
#define SN_CHAR_LEN           SN_NAME_CHAR_LEN + SN_EXT_CHAR_LEN + 1      

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
 *               directory. An instance of this struct can be used as the 
 *               current working directory.
 *       
 * Notes       : 1) Any instance of this struct must first be initialized by 
 *                  passing it to fat_SetDirToRoot.
 *               2) Most FAT functions require an instance of this struct to be
 *                  previously set and passed to it.
 * 
 * Warnings    : All members of an instance of this struct must correspond to
 *               the same valid FAT directory. If not, then unexpected results 
 *               will occur when trying to navigate and/or print directory 
 *               names, contents, entries, and files. As such, members of an 
 *               instance of this struct should only be set through the FAT
 *               functions declared here.
 * ----------------------------------------------------------------------------
 */
typedef struct
{
  char lnStr[LN_STR_LEN_MAX];          // directory long name
  char lnPathStr[PATH_STR_LEN_MAX];    // directory long name path
  char snStr[SN_NAME_CHAR_LEN + 1];    // directory short name. Add 1 for null
  char snPathStr[PATH_STR_LEN_MAX];    // directory short name path
  uint32_t fstClusIndx;                // index of directory's first cluster
} 
FatDir;

/* 
 * ----------------------------------------------------------------------------
 *                                                             FAT ENTRY STRUCT
 *
 * Description : Instances of this struct are used to locate entries within a 
 *               FAT directory.
 *       
 * Notes       : Any instance of this struct should first be initialized by
 *               passing it to fat_InitEntry, after which, fat_SetNextEntry
 *               should be the only function that updates the instance.
 * 
 * Warnings    : Members of an instance of this struct should never be set
 *               manually, but only by passing it to the FAT functions.
 * ----------------------------------------------------------------------------
 */
typedef struct 
{
  char lnStr[LN_STR_LEN_MAX];          // entry long name
  char snStr[SN_CHAR_LEN + 1];         // entry short name. Add 1 for null
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
 *                                                         INITIALIZE FAT ENTRY
 *                                      
 * Description : Initializin an instance of a FatEntry struct will set it to 
 *               the first entry of the root directory.
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
 * Description : Updates a FatEntry instance to point to the next entry in its
 *               directory.
 * 
 * Arguments   : currEnt   - Pointer to a FatEntry instance. Its members will 
 *                           be updated to point to the next entry. 
 *               bpb       - Pointer to the BPB struct instance.
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
 *               bpb         - Pointer to the BPB struct instance.
 * 
 * Returns     : A FAT Error Flag. If any value other than SUCCESS is returned 
 *               then the function was unable to update the FatEntry. 
 *  
 * Notes       : 1) This function can only set the directory to a child or the
 *                  parent of the FatDir instance (dir) when the function is
 *                  called, or reset the instance to the root directory.
 *               2) Paths (relative or absolute) should not be included in the 
 *                  newDirStr. newDirStr must be only be a directory name which
 *                  must be the name of a child, or the parent, directory of
 *                  the current directory.
 *               3) If ".." is passed as the newDirStr then the new directory
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
 *               specified by a FatDir instance, along with any of its fields
 *               requested.
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
 * Notes       : 1) LONG_NAME and/or SHORT_NAME must be passed in the entFlds
 *                  argument. If not, then no entries will be printed.
 *               2) If both LONG_NAME and SHORT_NAME are passed then both
 *                  the short and long names for each entry will be printed.
 *                  For any entry that does not have a long name, the short 
 *                  name is also stored in the long name string of the struct
 *                  and so the short name will effectively be printed twice -
 *                  Once for the long name and once for the short name.
 * ----------------------------------------------------------------------------
 */
uint8_t fat_PrintDir(const FatDir *dir, uint8_t entFlds, const BPB *bpb);

/*
 * ----------------------------------------------------------------------------
 *                                                         PRINT FILE TO SCREEN
 *                                       
 * Description : Prints the contents of any file entry to the screen. 
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
