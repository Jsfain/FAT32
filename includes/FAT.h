/*
*******************************************************************************
*                              AVR-FAT MODULE
*
* File    : FAT.H
* Version : 0.0.0.2
* Author  : Joshua Fain
* Target  : ATMega1280
* License : MIT LICENSE
* Copyright (c) 2020
* 
*
* DESCRIPTION: 
* Interface for navigating / accessing contents of a FAT32 formatted volume
* using an AVR microconstroller. Note, these only provide READ access to the 
* volume's contents. No WRITE/ERASE access is currently implemented.
*******************************************************************************
*/

#ifndef FAT_H
#define FAT_H


#include <avr/io.h>





/*
*******************************************************************************
*******************************************************************************
 *                     
 *                                    MACROS      
 *  
*******************************************************************************
*******************************************************************************
*/

#ifndef SECTOR_LEN
#define SECTOR_LEN                                512
#endif // SECTOR_LEN

#define ENTRY_LEN                                 32
#define END_CLUSTER                               0x0FFFFFFF



// ******* Maximum FAT String Length Flags

// Specify the max string length for long names and paths.  
#define PATH_STRING_LEN_MAX                       100
#define LN_STRING_LEN_MAX                         100



// ******* Fat Error Flags
#define SUCCESS                                   0x00
#define INVALID_FILE_NAME                         0x01
#define INVALID_DIR_NAME                          0x02
#define FILE_NOT_FOUND                            0x04
#define DIR_NOT_FOUND                             0x08
#define END_OF_FILE                               0x10
#define END_OF_DIRECTORY                          0x20
#define CORRUPT_FAT_ENTRY                         0x40
#define FAILED_READ_SECTOR                        0x80



// ******* Long Name Flags

// Used to indicate if a long name exists and how it and
// its shor name are distributed between adjacent sectors.
#define LN_EXISTS                                 0x01
#define LN_CROSS_SEC                              0x02
#define LN_LAST_SEC_ENTRY                         0x04



// ******** Entry Field Flags

// Flags to specify which entry fields should be printed.
#define SHORT_NAME                                0x01
#define LONG_NAME                                 0x02
#define HIDDEN                                    0x04
#define CREATION                                  0x08
#define LAST_ACCESS                               0x10
#define LAST_MODIFIED                             0x20
#define TYPE                                      0x40 
#define FILE_SIZE                                 0x80
#define ALL                       (CREATION | LAST_ACCESS | LAST_MODIFIED)



//********* FAT Entry Flags.

// Flags 0x01 to 0x20 are the attribute flags that can be set in the 
// attribute byte (byte 11) of a short name entry. If the lowest four
// bytes are set then this indicates the entry is part of a long name.
#define READ_ONLY_ATTR                            0x01
#define HIDDEN_ATTR                               0x02
#define SYSTEM_ATTR                               0x04
#define VOLUME_ID_ATTR                            0x08
#define DIR_ENTRY_ATTR                            0x10
#define ARCHIVE_ATTR                              0x20
#define LN_ATTR_MASK                              0x0F

// Other FAT specific flags / tokens.
#define LN_LAST_ENTRY                             0x40  
#define LN_ORD_MASK                               0x3F





/*
*******************************************************************************
*******************************************************************************
 *                     
 *                                 STRUCTS      
 *  
*******************************************************************************
*******************************************************************************
*/

// ****** FAT Directory Struct

// Instance members should correspond to a FAT directory. Use this to specify
// a current working directory. Members should not be set directly, but rather
// by calling the specific FAT functions below. Any instance must first be 
// initialized by passing it to fat_setDirToRoot() before it can be used.
typedef struct FatDirectory
{
  char longName[LN_STRING_LEN_MAX];
  char longParentPath[PATH_STRING_LEN_MAX];
  char shortName[9];
  char shortParentPath[PATH_STRING_LEN_MAX];
  uint32_t FATFirstCluster;
} 
FatDir;



// ****** FAT Entry Struct

// Instance members should correspond to a FAT file or directory entry. Members
// should not be set directly, but rather by calling specific FAT functions. 
// Any instance must first be initialized by calling fat_initEntry();
typedef struct FatEntry
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
  uint16_t snPosCurrSec;  // need these for the error correction flag check.
  uint16_t snPosNextSec;  // which will also reset these.
}
FatEntry;





/*
*******************************************************************************
*******************************************************************************
 *                     
 *                           FUNCTION DECLARATIONS       
 *  
*******************************************************************************
*******************************************************************************
*/

/* 
------------------------------------------------------------------------------
|                              SET ROOT DIRECTORY
|                                        
| Description : Sets an instance of FatDir to the root direcotry.
|
| Arguments   : *Dir    - Pointer to the instance of FatDir that will be set to
|                         the root directory.
|             : *bpb    - Pointer to a valid instance of a BPB struct.
-------------------------------------------------------------------------------
*/

void
fat_setDirToRoot(FatDir * Dir, BPB * bpb);



/*
-------------------------------------------------------------------------------
|                       INITIALIZE ENTRY STRUCT INSTANCE
|                                        
| Description : Initializes an entry of FatEntry.
|
| Arguments   : *ent   - Pointer to FatEntry instance that will be initialized.
|             : *bpb   - Pointer to a valid instance of a BPB struct.
-------------------------------------------------------------------------------
*/

void
fat_initEntry(FatEntry * ent, BPB * bpb);



/*
-------------------------------------------------------------------------------
|                               SET TO NEXT ENTRY 
|                                        
| Description : Updates the currEnt, a FatEntry instance, to the next entry
|               in the current directory (currDir).
|
| Arguments   : *currDir        - Current directory. Pointer to an instance
|                                 of FatDir.
|             : *currEnt      - Current etnry. Pointer to an instance of 
|                                 FatEntry
|             : *bpb            - Pointer to a valid instance of a BPB struct.
| 
| Return      : FAT Error Flag  - If any value other than SUCCESS is returned 
|                                 the function was unable to update the
|                                 FatEntry. To read, pass to fat_printError()
-------------------------------------------------------------------------------
*/

uint8_t 
fat_setNextEntry (FatDir * currDir, FatEntry * currEntry, BPB * bpb);

/*
-------------------------------------------------------------------------------
|                              SET FAT DIRECTORY
|                                        
| Description : Set Dir (FatDir instance) to directory specified by newDirStr.
|
| Arguments   : *Dir            - Pointer to the FatDir instance that will be
|                                 set to the new directory.
|             : *newDirStr      - Pointer to a string that specifies the name 
|                                 of the new directory.
|             : *bpb            - Pointer to a valid instance of a BPB struct.
| 
| Return      : FAT Error Flag  - If any value other than SUCCESS is returned 
|                                 the function was unable to update the
|                                 FatEntry. To read, pass to fat_printError().
|  
| Notes       : - Can only set the directory to a child or parent of the  
|                 current FatDir directory when the function is called.
|               - newDirStr must be a directory name. Will not work with paths.  
|               - newDirStr is case-sensitive.
-------------------------------------------------------------------------------
*/

uint8_t 
fat_setDir (FatDir * Dir, char * newDirStr, BPB * bpb);



/*
-------------------------------------------------------------------------------
|                  PRINT ENTRIES IN A DIRECTORY TO A SCREEN
|
| 
| Description : Prints the entries (files / dirs) in the FatDir directory. 
|  
| Arguments   : *Dir         - Pointer to a FatDir struct. This directory's
|                              entries will be printed to the screen.
|             : entFilt      - Byte of Entry Filter Flags. This specifies which
|                              entries and fields will be printed.
|             : *bpb         - Pointer to a valid instance of a BPB struct.
| 
| Return      : FAT Error Flag   - Returns END_OF_DIRECTORY if the function 
|                                  completes without issue. Any other value
|                                  indicates an error. To read, pass the value
|                                  to fat_printError().
| 
| Notes       : If neither LONG_NAME or SHORT_NAME are passed to entFilt
|               then no entries will be printed to the screen.   
-------------------------------------------------------------------------------
*/

uint8_t 
fat_printDir (FatDir * Dir, uint8_t entryFilter, BPB * bpb);



/*
-------------------------------------------------------------------------------
|                            PRINT FILE TO SCREEN
| 
| Description : Prints the contents of a file to the screen.
| 
| Arguments   : *Dir           - Pointer to a FatDir instance. This directory
|                                must contain the file to be printed.
|             : *fileNameStr   - Pointer to string. This is the name of the 
|                                file who's contents will be printed.
|             : *bpb           - Pointer to a valid instance of a BPB struct.
| 
| Return      : FAT Error Flag   - Returns END_OF_FILE if the function 
|                                  completes without issue. Any other value 
|                                  indicates an error. To read, pass the value
|                                  to fat_printError().
|                               
| Notes       : fileNameStr must be a long name unless a long name for a given
|               entry does not exist, in which case it must be a short name.
-------------------------------------------------------------------------------
*/

uint8_t 
fat_printFile (FatDir * Dir, char * file, BPB * bpb);



/*
-------------------------------------------------------------------------------
|                               PRINT FAT ERROR FLAG
| 
| Description : Prints the FAT Error Flag passed as the arguement. 
| 
| Arguments   : err      - An error flag returned by one of the FAT functions.
-------------------------------------------------------------------------------
*/

void 
fat_printError(uint8_t err);



#endif //FAT_H
