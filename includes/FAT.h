/*
***********************************************************************************************************************
*                                                       AVR-FAT MODULE
*
* File   : FAT.H
* Author : Joshua Fain
* Target : ATMega1280
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
*  (1) uint8_t  FAT_SetBiosParameterBlock(BPB * bpb);
*  (2) void     FAT_PrintBootSectorError (uint8_t err);
*  (3) void     FAT_SetDirectoryToRoot(FatDir * Dir, BPB * bpb);
*  (4) uint8_t  FAT_SetDirectory (FatDir * Dir, char * newDirStr, BPB * bpb);
*  (5) uint8_t  FAT_PrintDirectory (FatDir * Dir, uint8_t entryFilter, BPB * bpb);
*  (6) uint8_t  FAT_PrintFile (FatDir * Dir, char * file, BPB * bpb);
*  (7) void     FAT_PrintError(uint8_t err);
*
*
* STRUCTS USED (defined in FAT.H):
*  (1) typedef struct BiosParameterBlock BPB
*  (2) typedef struct FatDirectory FatDir
*
*                                                 
*                                                       MIT LICENSE
*
* Copyright (c) 2020 Joshua Fain
*
* Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated 
* documentation files (the "Software"), to deal in the Software without restriction, including without limitation the 
* rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to 
* permit ersons to whom the Software is furnished to do so, subject to the following conditions: The above copyright 
* notice and this permission notice shall be included in all copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE 
* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
* COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
* OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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
#define END_OF_CLUSTER                            0x0FFFFFFF


// ******* Maximum FAT String Length Flags

// Specify the max length of strings that can be stored in the members
// of a FatDir instance. These are lower than required by FAT32 specs 
// (255), to conserve memory. Adjust as needed.  
#define PATH_STRING_LEN_MAX                       100
#define LONG_NAME_STRING_LEN_MAX                  100


// ******* Boot Sector Error Flags
#define CORRUPT_BOOT_SECTOR                       0x01
#define NOT_BOOT_SECTOR                           0x02
#define INVALID_BYTES_PER_SECTOR                  0x04
#define INVALID_SECTORS_PER_CLUSTER               0x08
#define BOOT_SECTOR_NOT_FOUND                     0x10
#define BOOT_SECTOR_VALID                         0x20
#define FAILED_READ_BOOT_SECTOR                   0x40


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
// in FAT_PrintDirectory() to indicate which entries and their
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

// ****** Bios Paramater Block struct

// An instance of this struct should hold specific values set in the FAT volume's
// BIOS Parameter Block, as well as a few calculated values based on BPB values.
// These values should be set only by passing an instance of the struct to 
// FAT_SetBiosParameterBlock(BPB * bpb)
typedef struct BiosParameterBlock
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



/*
***********************************************************************************************************************
 *                                   SET MEMBERS OF THE BIOS PARAMETER BLOCK STRUCT
 * 
 * A valid BPB struct instance is a required argument of any function that accesses the FAT volume, therefore this
 * function should be called first, before implementing any other parts of this FAT module.
 *                                         
 * Description : This function will set the members of a BiosParameterBlock (BPB) struct instance according to the
 *               values specified within the FAT volume's Bios Parameter Block / Boot Sector. 
 * 
 * Argument    : *bpb        Pointer to a BPB struct instance. This function will set the members of this instance.
 * 
 * Return      : Boot sector error flag     See the FAT.H header file for a list of these flags. To print the returned
 *                                          value, pass it to FAT_PrintBootSectorError(err). If the BPB instance's
 *                                          members are successfully set then BOOT_SECTOR_VALID is returned. Any other 
 *                                          returned value indicates a failure. 
 * 
 * Note        : This function DOES NOT set the values a physical FAT volume's Bios Parameter Block as would be 
 *               required during formatting of a FAT volume. This module can only read a FAT volume's contents and does
 *               not have the capability to modify anything on the volume itself, this includes formatting a volume.
***********************************************************************************************************************
*/

uint8_t 
FAT_SetBiosParameterBlock(BPB * bpb);



/*
***********************************************************************************************************************
 *                                             PRINT BOOT SECTOR ERROR FLAG
 * 
 * Description : Call this function to print the error flag returned by the function FAT_SetBiosParameterBlock(). 
 * 
 * Argument    : err    Boot Sector Error flag returned the function FAT_SetBiosParameterBlock().
***********************************************************************************************************************
*/

void 
FAT_PrintBootSectorError (uint8_t err);



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
FAT_SetDirectoryToRoot(FatDir * Dir, BPB * bpb);



/*
***********************************************************************************************************************
 *                                                   SET FAT DIRECTORY
 *                                        
 * Description : Call this function to set a FatDirectory (FatDir) struct instance to a new directory. The new 
 *               directory must be a child, or the parent, of the struct's instance when this function is called. This
 *               function operates by searching the current directory for a name that matches string newDirStr. If a 
 *               matching entryPos is found, the members of the FatDir instance are updated to those corresponding to the 
 *               matching entryPos. To set to parent, pass the string ".." as newDirStr.
 *
 * Arguments   : *Dir            - Pointer to an instance of FatDir. The members of *Dir must point to a valid FAT32
 *                                 directory when the function is called. The members of the instance are updated by 
 *                                 this function.
 *             : *newDirStr      - Pointer to a C-string that specifies the name of the intended new directory. This 
 *                                 function will only search the current FatDir instance's directory for a matching  
 *                                 name, thus it is only possible to set FatDir to a child, or the parent (".."), of 
 *                                 the current directory. Paths must not be included in the string. This string is also 
 *                                 required to be a long name unless a long name does not exist for a given entryPos, only
 *                                 then can a short name be a valid string. This is case-sensitive.
 *             : *bpb            - Pointer to a valid instance of a BPB struct.
 * 
 * Return      : FAT Error Flag  - The returned value can be read by passing it to FAT_PrintError(ErrorFlag). If 
 *                                 SUCCESS is returned then the FatDir instance members were successfully updated to
 *                                 point the new directory. Any other returned value indicates a failure.
 *  
 * Limitation  : This function will not work with absolute paths, it will only set a FatDir instance to a new directory
 *               if the new directory is a child, or the parent, of the current directory.
***********************************************************************************************************************
*/

uint8_t 
FAT_SetDirectory (FatDir * Dir, char * newDirStr, BPB * bpb);



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
 *                                  returned indicates an error. To read, pass the value to FAT_PrintError(ErrorFlag).
***********************************************************************************************************************
*/

uint8_t 
FAT_PrintDirectory (FatDir * Dir, uint8_t entryFilter, BPB * bpb);



/*
***********************************************************************************************************************
 *                                               PRINT FILE TO SCREEN
 * 
 * Description : Prints the contents of a FAT file from the current directory to a terminal/screen.
 * 
 * Arguments   : *Dir              - Pointer to a FatDir struct instance pointing to a valid FAT32 directory.
 *             : *fileNameStr      - Ptr to C-string. This is the name of the file to be printed to the screen. This
 *                                   can only be a long name unless there is no long name for a given entry, in which
 *                                   case it must be a short name.
 *             : *bpb              - Pointer to a valid instance of a BPB struct.
 * 
 * Return      : FAT Error Flag     Returns END_OF_FILE if the function completed successfully and was able to read in
 *                                  and print a file's contents to the screen. Any other value returned indicates an
 *                                  an error. Pass the returned value to FAT_PrintError(ErrorFlag).
***********************************************************************************************************************
*/

uint8_t 
FAT_PrintFile (FatDir * Dir, char * file, BPB * bpb);



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
FAT_PrintError(uint8_t err);



#endif //FAT_H