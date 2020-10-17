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



/*
***********************************************************************************************************************
 *                                   SET MEMBERS OF THE BIOS PARAMETER BLOCK STRUCT
 * 
 * An instantiated and valid BiosParameterBlock struct is a required argument of any function that accesses the FAT 
 * volume, therefore this function should be called first before implementing any other parts of this FAT module.
 *                                         
 * Description : This function will set the members of a BiosParameterBlock struct instance according to the values
 *               specified within the FAT volume's Bios Parameter Block / Boot Sector. 
 * 
 * Argument    : *bpb    pointer to an instance of a BiosParameterBlock struct.
 * 
 * Return      : Boot sector error flag     See the FAT.H header file for a list of these flags. To print the returned
 *                                          value, pass it to FAT_PrintBootSectorError(err). If the BiosParameterBlock
 *                                          struct's members have been successfully set then BOOT_SECTOR_VALID is
 *                                          returned. Any other returned value indicates a failure to set the BPB. 
 * 
 * Note        : This function DOES NOT set the values a physical FAT volume's Bios Parameter Block as would be 
 *               required during formatting of a FAT volume. This module can only read a FAT volume's contents and does
 *               not have the capability to modify anything on the volume, this includes formatting a FAT volume.
***********************************************************************************************************************
*/

uint16_t 
FAT_SetBiosParameterBlock(BiosParameterBlock * bpb);



/*
***********************************************************************************************************************
 *                                               SET A CURRENT DIRECTORY
 *                                        
 * Description : Call this function to set a new current directory. This operates by searching the current directory, 
 *               pointed to by the currentDirectory struct, for a name that matches newDirectoryStr. If a match is
 *               found the members of the currentDirectory struct are updated to those corresponding to the matching 
 *               newDirectoryStr entry.
 *
 * Arguments   : *currentDirectory   pointer to a FatCurrentDirectory struct whose members must point to a valid
 *                                   FAT32 directory.
 *             : *newDirectoryStr    pointer to a C-string that is the name of the intended new directory. The function
 *                                   takes this and searches the current directory for a matching name. This string
 *                                   must be a long name unless a long name does not exist for a given entry. Only then
 *                                   will a short name be searched.
 *             : *bpb                pointer to a BiosParameterBlock struct.
 * 
 * Returns     : FAT Error Flag      The returned value can be read by passing it to FAT_PrintError(ErrorFlag). If 
 *                                   SUCCESS is returned then the currentDirectory struct members were successfully 
 *                                   updated, but any other returned value indicates a failure struct members will not
 *                                   have been modified updated.
 *  
 * Limitation  : This function will not work with absolute paths, it will only set a new directory that is a child or
 *               the parent of the current directory. 
***********************************************************************************************************************
*/

uint16_t 
FAT_SetCurrentDirectory (FatCurrentDirectory * currentDirectory, char * newDirectoryStr, BiosParameterBlock * bpb);



/*
***********************************************************************************************************************
 *                                       PRINT THE ENTRIES OF THE CURRENT DIRECTORY
 * 
 * Description : Prints a list of entries (files and directories) contained in the current directory. Which entries and
 *               which associated data (hidden files, creation date, ...) are indicated by the ENTRY_FLAG. See the 
 *               specific ENTRY_FLAGs that can be passed in the FAT.H header file.
 * 
 * Argument    : *currentDirectory   pointer to a FatCurrentDirectory struct whose members must be associated with a 
 *                                   valid FAT32 directory.
 *             : entryFilter         byte of ENTRY_FLAGs, used to determine which entries will be printed. Any 
 *                                   combination of flags can be set. If neither LONG_NAME or SHORT_NAME are passed 
 *                                   then no entries will be printed.
 *             : *bpb                pointer to a BiosParameterBlock struct.
 * 
 * Returns     : FAT Error Flag     Returns END_OF_DIRECTORY if the function completed successfully and was able to
 *                                  read in and print entries until it reached the end of the directory. Any other 
 *                                  value returned indicates an error. Pass the value to FAT_PrintError(ErrorFlag). 
***********************************************************************************************************************
*/

uint16_t 
FAT_PrintCurrentDirectory (FatCurrentDirectory *currentDirectory, uint8_t entryFilter, BiosParameterBlock * bpb);



/*
***********************************************************************************************************************
 *                                               PRINT FILE TO SCREEN
 * 
 * Description : Prints the contents of a file from the current directory to a terminal/screen.
 * 
 * Arguments   : *currentDirectory   pointer to a FatCurrentDirectory struct whose members must be associated with a 
 *                                   valid FAT32 directory.
 *             : *fileNameStr        ptr to C-string that is the name of the file to be printed to the screen. This
 *                                   must be a long name, unless there is no associated long name with an entry, in 
 *                                   which case it can be a short name.
 *             : *bpb                pointer to a BiosParameterBlock struct.
 * 
 * Return      : FAT Error Flag     Returns END_OF_FILE if the function completed successfully and was able to
 *                                  read in and print a file's contents to the screen. Any other value returned 
 *                                  indicates an error. Pass the returned value to FAT_PrintError(ErrorFlag).
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