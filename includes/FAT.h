/*
***********************************************************************************************************************
*                                                       AVR-FAT
*                                        
*                                           Copyright (c) 2020 Joshua Fain
*                                                 All Rights Reserved
*
* DESCRIPTION: Module used for interfacing with a FAT32 formatted volume. Declares the functions defined in FAT.C as
*              well as defines the structs and flags to be used to interact with those functions.
*
*
* File : FAT.H
* By   : Joshua Fain
***********************************************************************************************************************
*/


#ifndef FAT_H
#define FAT_H


#include <avr/io.h>


#define SECTOR_LEN                                512
#define ENTRY_LEN                                 32

#define PATH_SIZE_MAX                             50
#define LONG_NAME_SIZE_MAX                        50

#define END_OF_CLUSTER                            0x0FFFFFFF

// Boot Sector Error Flags
#define CORRUPT_BOOT_SECTOR                       0x01
#define NOT_BOOT_SECTOR                           0x02
#define INVALID_BYTES_PER_SECTOR                  0x04
#define INVALID_SECTORS_PER_CLUSTER               0x08
#define BOOT_SECTOR_NOT_FOUND                     0x10
#define BOOT_SECTOR_VALID                         0x20

// Fat Error Flags
#define SUCCESS                                   0x00
#define INVALID_FILE_NAME                         0x01
#define INVALID_DIR_NAME                          0x02
#define FILE_NOT_FOUND                            0x04
#define DIR_NOT_FOUND                             0x08
#define END_OF_FILE                               0x10
#define END_OF_DIRECTORY                          0x20
#define CORRUPT_FAT_ENTRY                         0x40

// FAT Entry Attribute Flags
#define READ_ONLY_ATTR                            0x01
#define HIDDEN_ATTR                               0x02
#define SYSTEM_ATTR                               0x04
#define VOLUME_ID_ATTR                            0x08
#define DIRECTORY_ENTRY_ATTR                      0x10
#define ARCHIVE_ATTR                              0x20
#define LONG_NAME_ATTR_MASK                       0x0F
// Other FAT specific flags / tokens
#define LONG_NAME_LAST_ENTRY                      0x40
#define LONG_NAME_ORDINAL_MASK                    0x3F

// Long Name Flags
#define LONG_NAME_EXISTS                     0x01
#define LONG_NAME_CROSSES_SECTOR_BOUNDARY    0x02
#define LONG_NAME_LAST_SECTOR_ENTRY          0x04

// Entry Filter Flags
#define SHORT_NAME                                0x01
#define LONG_NAME                                 0x02
#define HIDDEN                                    0x04
#define CREATION                                  0x08
#define LAST_ACCESS                               0x10
#define LAST_MODIFIED                             0x20
#define TYPE                                      0x40
#define FILE_SIZE                                 0x80
#define ALL                                       (CREATION | LAST_ACCESS | LAST_MODIFIED)

// Interface Error Flags : These errors are to be applied by the phyiscal interface.
#define READ_SECTOR_ERROR                         0x01
#define READ_SECTOR_TIMEOUT                       0x02
#define READ_SECTOR_SUCCESSFUL                    0x04


// Struct to hold certain parameters of the Boot Sector / BIOS Parameter Block as well as some calculated values
typedef struct BPB
{
    uint16_t bytesPerSec;
    uint8_t  secPerClus;
    uint16_t rsvdSecCnt;
    uint8_t  numOfFats;
    uint32_t fatSize32;
    uint32_t rootClus;

    uint32_t bootSecAddr;
    uint32_t dataRegionFirstSector;
} BPB;


// Struct to hold parameters of a FAT Dir.
typedef struct FatDirectory
{
    char longName[LONG_NAME_SIZE_MAX];        // max 255 characters + '\0'
    char longParentPath[PATH_SIZE_MAX];  // long name path to PARENT Dir 
    char shortName[9];         // max 8 char + '\0'. Directory extensions
                               // for short names not currently supported.
    char shortParentPath[PATH_SIZE_MAX]; // short name path to PARENT Dir.
    uint32_t FATFirstCluster;  // Index of first cluster of current Dir.
} FatDir;



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
 * Description : Prints the contents of a file from the current Dir to a terminal/screen.
 * 
 * Arguments   : *Dir   pointer to a FatDir struct whose members must be associated with a 
 *                                   valid FAT32 Dir.
 *             : *fileNameStr        ptr to C-string that is the name of the file to be printed to the screen. This
 *                                   must be a long name, unless there is no associated long name with an entry, in 
 *                                   which case it can be a short name.
 *             : *bpb                pointer to a BPB struct.
 * 
 * Return      : FAT Error Flag     Returns END_OF_FILE if the function completed successfully and was able to
 *                                  read in and print a file's contents to the screen. Any other value returned 
 *                                  indicates an error. Pass the returned value to FAT_PrintError(ErrorFlag).
***********************************************************************************************************************
*/

uint8_t 
FAT_PrintFile (FatDir * Dir, char * file, BPB * bpb);



/*
***********************************************************************************************************************
 *                                                PRINT FAT ERRORS
 *
 * DESCRIPTION : Prints the error code returned by a FAT functions.
 * 
 * ARGUMENT    : err         uin8_t value which is the error to be printed by the function.
***********************************************************************************************************************
*/
void 
FAT_PrintError(uint8_t err);



#endif //FAT_H