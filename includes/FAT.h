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


#define SECTOR_LEN 512
#define ENTRY_LEN   32

#define LONG_NAME_LEN_MAX  64

// FAT Entry Attribute Flags
#define READ_ONLY_ATTR_FLAG         0x01
#define HIDDEN_ATTR_FLAG            0x02
#define SYSTEM_ATTR_FLAG            0x04
#define VOLUME_ID_ATTR_FLAG         0x08
#define DIRECTORY_ENTRY_ATTR_FLAG   0x10
#define ARCHIVE_ATTR_FLAG           0x20
#define LONG_NAME_ATTR_MASK         (READ_ONLY_ATTR_FLAG || HIDDEN_ATTR_FLAG || SYSTEM_ATTR_FLAG || VOLUME_ID_ATTR_FLAG)

// Other FAT specific flags / tokens
#define LONG_NAME_LAST_ENTRY_FLAG 0x40
#define LONG_NAME_ORDINAL_MASK    0x3F
#define END_OF_CLUSTER            0x0FFFFFFF

// Error Flags
#define SUCCESS              0x000
#define INVALID_FILE_NAME    0x001
#define INVALID_DIR_NAME     0x002
#define FILE_NOT_FOUND       0x004
#define DIR_NOT_FOUND        0x008
#define END_OF_FILE          0x010
#define END_OF_DIRECTORY     0x020
#define CORRUPT_FAT_ENTRY    0x040

// Filter Flags
#define SHORT_NAME     0x01
#define LONG_NAME      0x02
#define HIDDEN         0x04
#define CREATION       0x08
#define LAST_ACCESS    0x10
#define LAST_MODIFIED  0x20
#define ALL            (CREATION | LAST_ACCESS | LAST_MODIFIED)

// Boot Sector Error Flags
#define CORRUPT_BOOT_SECTOR              0x0001
#define NOT_BOOT_SECTOR                  0x0002
#define INVALID_BYTES_PER_SECTOR         0x0004
#define INVALID_SECTORS_PER_CLUSTER      0x0008
#define BOOT_SECTOR_NOT_FOUND            0x0010
#define BOOT_SECTOR_VALID                0x0020

// Interface Error Flags : These errors are to be applied by the phyiscal interface.
#define READ_SECTOR_ERROR       0x01
#define READ_SECTOR_TIMEOUT     0x02
#define READ_SECTOR_SUCCESSFUL  0x04


// Struct to hold certain parameters of the Boot Sector / BIOS Parameter Block as well as some calculated values
typedef struct
{
    uint16_t bytesPerSector;
    uint8_t  sectorsPerCluster;
    uint16_t reservedSectorCount;
    uint8_t  numberOfFats;
    uint32_t fatSize32;
    uint32_t rootCluster;

    uint32_t bootSectorAddress;
    uint32_t dataRegionFirstSector;

} BiosParameterBlock;


// Struct to hold parameters for the current directory.
typedef struct
{
    char longName[256];        // max 255 characters + '\0'
    char longParentPath[256];  // long name path to PARENT directory 
    char shortName[9];         // max 8 char + '\0'. Directory extensions
                               // for short names not currently supported.
    char shortParentPath[256]; // short name path to PARENT directory.
    uint32_t FATFirstCluster;  // Index of first cluster of current directory.
} FatCurrentDirectory;



uint16_t 
FAT_GetBiosParameterBlock(BiosParameterBlock * bpb);



/*
***********************************************************************************************************************
 *                                             SET A CURRENT DIRECTORY
 *                                        
 * DESCRIPTION : Call this function to set a new current directory by setting the Sets currentDirectory to 
 *               newDirectoryStr, if found, by updating the members of the FatCurrentDirectory struct with values 
 *               corresponding to a matching directory entry for newDirectory.
 *
 * ARGUMENTS   : *currentDirectory      pointer to a FatCurrentDirectory struct whose members must point to a valid
 *                                      FAT32 directory.
 *             : *newDirectoryStr       pointer to a string that specifies the long name of the intended new directory.
 *                                      newDirectoryStr can only be a short name if there is no long name for the entry.
 *             : *bpb                   Bios Parameter block
 * 
 * RETURNS     : FAT Error Flag         This can be read by passing it to PrintFatError(ErrorFlag).
***********************************************************************************************************************
*/
uint16_t 
FAT_SetCurrentDirectory (FatCurrentDirectory * currentDirectory, char * newDirectoryStr, BiosParameterBlock * bpb);



/*
***********************************************************************************************************************
 *                                       PRINT ENTRIES IN THE CURRENT DIRECTORY
 * 
 * DESCRIPTION : Prints the contents of currentDirectory to the screen. Which contents are printed is specified by 
 *               ENTRY_FLAG (see ENTRY FLAGS list above.)
 * 
 * ARGUMENT    : *currentDirectory       pointer to a FatCurrentDirectory struct whose members must point to a valid
 *                                       FAT32 directory.
 *             : entryFilterFlag
 *             : *bpb
 * 
 * RETURNS     : FAT Error Flag. This can be read by passing it to PrintFatError(ErrorFlag).
***********************************************************************************************************************
*/
uint16_t 
FAT_PrintCurrentDirectory (FatCurrentDirectory *currentDirectory, uint8_t entryFilterFlag, BiosParameterBlock * bpb);



/*
***********************************************************************************************************************
 *                                                  PRINT FILE
 * 
 * DESCRIPTION : Prints the contents of *file to a terminal/screen.
 * 
 * ARGUMENTS   : *currentDirectory      pointer to a FatCurrentDirectory struct whose members must point to a valid
 *                                      FAT32 directory.
 *             : *file                  ptr to string that is the long name of the file to be printed to the 
 *                                      screen. *file can only point to a short name if there is no long name for 
 *                                      that entry.
 *             : *bpb
 * RETURNS     : FAT Error Flag. This can be read by passing it to PrintFatError(ErrorFlag).
***********************************************************************************************************************
*/
uint16_t 
FAT_PrintFile (FatCurrentDirectory * currentDirectory, char * file, BiosParameterBlock * bpb);



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
FAT_PrintError(uint16_t err);



#endif //FAT_H