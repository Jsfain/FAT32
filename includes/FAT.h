/*
***********************************************************************************************************************
*                                                       AVR-FAT MODULE
*
* File    : FAT.H
* Version : 0.0.0.2
* Author  : Joshua Fain
* Target  : ATMega1280
* Copyright (c) 2020 Joshua Fain
*
*
* DESCRIPTION: 
* Header file for FAT.C - declares functions defined there, as well as defines STRUCTS and MACROS. The module is to be
* used for accessing the contents of a FAT32 formatted volume using an AVR microconstroller. The fuctions declared here
* only provide READ access to the volume's contents (i.e. print file, print directory), no WRITE access is currently 
* possible. 
*
*
* FUNCTION "PUBLIC":
*  (1) void     fat_set_directory_to_root (FatDir * Dir, BPB * bpb)
*  (2) uint8_t  fat_set_directory (FatDir * Dir, char * newDirStr, BPB * bpb)
*  (3) uint8_t  fat_print_directory (FatDir * Dir, uint8_t entryFilter, BPB * bpb)
*  (4) uint8_t  fat_print_file (FatDir * Dir, char * file, BPB * bpb)
*  (5) void     fat_print_error (uint8_t err)
*
*
* STRUCTS USED (defined in FAT.H):
*  typedef struct FatDirectory FatDir
*
***********************************************************************************************************************
*/


#ifndef FAT_H
#define FAT_H


#include <avr/io.h>


/*
***********************************************************************************************************************
 *                                                       MACROS
***********************************************************************************************************************
*/


#define SECTOR_LEN                                512
#define ENTRY_LEN                                 32
#define END_CLUSTER                            0x0FFFFFFF


// ******* Maximum FAT String Length Flags

// Specify the max length of strings that can be stored in the members
// of a FatDir instance. These are lower than required by FAT32 specs 
// (255), to conserve memory. Adjust as needed.  
#define PATH_STRING_LEN_MAX                       100
#define LONG_NAME_STRING_LEN_MAX                  100



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


// ******* Long Name Distribution Flags

// These are set and used by a calling function to indicate
// how a particular entry long/short name is distributed among
// adjacent sectors.
#define LONG_NAME_EXISTS                          0x01
#define LONG_NAME_CROSSES_SECTOR_BOUNDARY         0x02
#define LONG_NAME_LAST_SECTOR_ENTRY               0x04


// ******** Entry Filter Flags

// Pass any combination of these flags as the entryFilter argument
// in fat_print_directory() to indicate which entries and their
// fields to print to the screen.
#define SHORT_NAME                                0x01
#define LONG_NAME                                 0x02
#define HIDDEN                                    0x04
#define CREATION                                  0x08
#define LAST_ACCESS                               0x10
#define LAST_MODIFIED                             0x20
#define TYPE                                      0x40 // specifies if entry is a directory or file
#define FILE_SIZE                                 0x80 // in kb. rounded.
#define ALL                       (CREATION | LAST_ACCESS | LAST_MODIFIED)


//********* FAT Entry Flags.

// Flags 0x01 to 0x20 correspond to the attribute flags that can be set 
// in the attribute byte (byte 11) of an entry. If the lowest four bytes
// are set then this indicates the entry is part of a long name.
#define READ_ONLY_ATTR                            0x01
#define HIDDEN_ATTR                               0x02
#define SYSTEM_ATTR                               0x04
#define VOLUME_ID_ATTR                            0x08
#define DIRECTORY_ENTRY_ATTR                      0x10
#define ARCHIVE_ATTR                              0x20
#define LONG_NAME_ATTR_MASK                       0x0F // OR'd lowest 4 attributes
// Other FAT specific flags / tokens.
#define LONG_NAME_LAST_ENTRY                      0x40  
#define LONG_NAME_ORDINAL_MASK                    0x3F


/*
***********************************************************************************************************************
 *                                                     STRUCTS
***********************************************************************************************************************
*/




// ****** FAT Directory Struct

// An instance of this struct should hold specific values according to a FAT directory.
// The members of the struct will hold the long and short names of the directory as 
// well as the long/short name path to the directory. These are useful for visual
// confirmation of which directory the instance is pointing, but the member 
// FATFirstCluster is the only value that determines which directory the instance is 
// actually pointing at. This should be initialized by setting it to the root directory
// using FAT_SetToRootDirectory(). After this, the values of this struct should only be 
// manipulated by passing it to other FAT functions.
typedef struct FatDirectory
{
  char longName[LONG_NAME_STRING_LEN_MAX];        // max 255 characters + '\0'
  char longParentPath[PATH_STRING_LEN_MAX];  // long name path to PARENT Dir 
  char shortName[9];         // max 8 char + '\0'. Directory extensions
                              // for short names not currently supported.
  char shortParentPath[PATH_STRING_LEN_MAX]; // short name path to PARENT Dir.
  uint32_t FATFirstCluster;  // Index of first cluster of current Dir.
} 
FatDir;


// Holds state of an entry in a Fat directory
typedef struct FatEntry
{
  char longName[LONG_NAME_STRING_LEN_MAX];
  char shortName[13]; // size 13 for 8+3 entry size, '.' separator and '\0' string termination 
  uint8_t  shortNameEntry[32]; // Array to hold all of the entry points 
  
  
  // these parameters are used to save the state when using "fat_next_entry()"
  uint8_t  longNameEntryCount; 
  uint32_t shortNameEntryClusIndex; 
  uint8_t  shortNameEntrySecNumInClus; 
  uint16_t entryPos;  
  uint8_t  lnFlags;
  uint16_t snPosCurrSec;  // need these for the error correction flag check.
  uint16_t snPosNextSec;  // which will also reset these.
}
FatEntry;


/*
***********************************************************************************************************************
 *                                                       FUNCTIONS
***********************************************************************************************************************
*/



/*
***********************************************************************************************************************
 *                                SET MEMBERS OF FAT DIRECTORY INSTANCE TO ROOT DIRECTORY
 * 
 * This function should be called before maniupulating/accessing the FatDir instance using the other FAT functions.
 * Call this function to set the members of the FatDir struct's instance to the root directory
 *                                         
 * Description : This function will set the members of a BiosParameterBlock (BPB) struct instance according to the
 *               values specified within the FAT volume's Bios Parameter Block / Boot Sector. 
 * 
 * Arguments   : *Dir          - Pointer to a FatDir struct whose members will be set to point to the root directory.
 *             : *bpb          - Pointer to a BPB struct instance.
***********************************************************************************************************************
*/

void
fat_set_directory_to_root(FatDir * Dir, BPB * bpb);


void
fat_init_entry(FatEntry * ent, BPB * bpb);


/*
***********************************************************************************************************************
 *                                                   SET FAT DIRECTORY
 *                                        
 * Description : Call this function to set a FatDirectory (FatDir) struct instance to a new directory. The new
 *               directory must be a child, or the parent, of the struct's instance when this function is called. This
 *               function operates by searching the current directory for a name that matches string newDirStr. If a 
 *               matching entryPos is found, the members of the FatDir instance are updated to those corresponding to
 *               the matching entryPos. To set to parent, pass the string ".." as newDirStr.
 *
 * Arguments   : *Dir            - Pointer to an instance of FatDir. The members of *Dir must point to a valid FAT32
 *                                 directory when the function is called. The members of the instance are updated by 
 *                                 this function.
 *             : *newDirStr      - Pointer to a C-string that specifies the name of the intended new directory. This 
 *                                 function will only search the current FatDir instance's directory for a matching  
 *                                 name, thus it is only possible to set FatDir to a child, or the parent (".."), of 
 *                                 the current directory. Paths must not be included in the string. This string is
 *                                 also required to be a long name unless a long name does not exist for a given
 *                                 entryPos, only then can a short name be a valid string. This is case-sensitive.
 *             : *bpb            - Pointer to a valid instance of a BPB struct.
 * 
 * Return      : FAT Error Flag  - The returned value can be read by passing it tp fat_print_error(err). If SUCCESS
 *                                 is returned then the FatDir instance members were successfully updated to point to
 *                                 the new directory. Any other returned value indicates a failure.
 *  
 * Limitation  : This function will not work with absolute paths, it will only set a FatDir instance to a new directory
 *               if the new directory is a child, or the parent, of the current directory.
***********************************************************************************************************************
*/

uint8_t 
fat_set_directory (FatDir * Dir, char * newDirStr, BPB * bpb);



/*
***********************************************************************************************************************
 *                                       PRINT ENTRIES IN A DIRECTORY TO A SCREEN
 * 
 * Description : Prints a list of the entries (files and directories) contained in the directory pointed to by the
 *               FatDir struct instance. Which entries and fields (e.g. hidden files, creation date, etc...) are
 *               selected by passing the desired combination of Entry Filter Flags as the entryFilter argument. See the
 *               specific Entry Filter Flags that can be passed in the FAT.H header file.
 * 
 * Arguments   : *Dir             - Pointer to a FatDir struct whose members must be associated with a valid FAT32 Dir
 *             : entryFilter      - Byte whose value specifies which entries and fields will be printed (short name, 
 *                                  long name, hidden, creation date, etc...). Any combination of flags can be passed. 
 *                                  If neither LONG_NAME or SHORT_NAME are passed then no entries will be printed.
 *             : *bpb             - Pointer to a valid instance of a BPB struct.
 * 
 * Return      : FAT Error Flag     Returns END_OF_DIRECTORY if the function completed successfully and it was able to
 *                                  read in and print entries until reaching the end of the directory. Any other value
 *                                  returned indicates an error. To read, pass the value to fat_print_error(err).
***********************************************************************************************************************
*/

uint8_t 
fat_print_directory (FatDir * Dir, uint8_t entryFilter, BPB * bpb);



/*
***********************************************************************************************************************
 *                                               PRINT FILE TO SCREEN
 * 
 * Description : Prints the contents of a FAT file from the current directory to a terminal/screen.
 * 
 * Arguments   : *Dir              - Pointer to a FatDir struct instance pointing to a valid FAT32 directory.
 *             : *fileNameStr      - Pointer to C-string. This is the name of the file to be printed to the screen.
 *                                   This can only be a long name unless there is no long name for a given entry, in
 *                                   which case it must be a short name.
 *             : *bpb              - Pointer to a valid instance of a BPB struct.
 * 
 * Return      : FAT Error Flag     Returns END_OF_FILE if the function completed successfully and was able to read in
 *                                  and print a file's contents to the screen. Any other value returned indicates an
 *                                  an error. Pass the returned value to fat_print_error(err).
***********************************************************************************************************************
*/

uint8_t 
fat_print_file (FatDir * Dir, char * file, BPB * bpb);



uint8_t 
fat_next_entry (FatDir * currDir, FatEntry * currEntry, BPB * bpb);



/*
***********************************************************************************************************************
 *                                        PRINT ERROR RETURNED BY A FAT FUNCTION
 * 
 * Description : Call this function to print an error flag returned by one of the FAT functions to the screen. 
 * 
 * Argument    : err    This should be an error flag returned by one of the FAT file/directory functions.
***********************************************************************************************************************
*/

void 
fat_print_error(uint8_t err);



#endif //FAT_H
