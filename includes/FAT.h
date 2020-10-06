/******************************************************************************
 * FAT.H
 * 
 * DESCRIPTION 
 * Module used for interfacing with a FAT32 formatted volume. Declares the
 * functions defined in FAT.C as well as defines the structs and flags to be
 * used to interact with those functions.
 * 
 * TARTGET
 * ATmega1280
 * 
 * REQUIRES
 * Driver to handle interaction with a physical disk. See ReadMe.
 * 
 * LIMITATIONS
 * (1) Only READ operations are allowed currently.
 * (2) Currently only been tested agains a FAT32 formatted 2GB SD Card.
 *
 * FUNCTIONS
 * (1) uint8_t   SetFatCurrentDirectory(
 *                      FatCurrentDirectory *currentDirectory, 
 *                      char *newDirectory)
 * (2) uint8_t   PrintFatCurrentDirectoryContents(
 *                      FatCurrentDirectory *currentDirectory, 
 *                      uint8_t FLAG)
 * (3) uint8_t   PrintFatFileContents(
 *                      FatCurrentDirectory *currentDirectory,
 *                      char *fileName)
 * (4)  void     PrintFatError(uint8_t err)
 * (5)  uint16_t GetFatBytsPerSec()
 * (6)  uint8_t  GetFatSecPerClus()
 * (7)  uint16_t GetFatRsvdSecCnt()
 * (8)  uint8_t  GetFatNumFATs()
 * (9)  uint32_t GetFatFATSz32()
 * (10) uint32_t GetFatRootClus()
 * 
 * Author: Joshua Fain
 * Date: 9/12/2020
 * ***************************************************************************/



#ifndef FAT_H
#define FAT_H

#include <avr/io.h>


// Struct to hold parameters for the current directory.
typedef struct //FatCurrentDirectory
{
    char longName[256];        // max 255 characters + '\0'
    char longParentPath[256];  // long name path to PARENT directory 
    char shortName[9];         // max 8 char + '\0'. Directory extensions
                               // for short names not currently supported.
    char shortParentPath[256]; // short name path to PARENT directory.
    uint32_t FATFirstCluster;  // Index of first cluster of current directory.
} FatCurrentDirectory;


#define SECTOR_LEN 512

//FAT Error Flags
#define SUCCESS                          0x000
#define INVALID_FILE_NAME                0x001
#define INVALID_DIR_NAME                 0x002
#define FILE_NOT_FOUND                   0x004
#define DIR_NOT_FOUND                    0x008
#define END_OF_FILE                      0x010
#define END_OF_DIRECTORY                 0x020
#define CORRUPT_FAT_ENTRY                0x040



//ENTRY FLAGS
//specify which fields to print when calling PrintFatCurrentDirectoryContents()
#define SHORT_NAME     0x01
#define LONG_NAME      0x02
#define HIDDEN         0x04
#define CREATION       0x08
#define LAST_ACCESS    0x10
#define LAST_MODIFIED  0x20
#define ALL ( CREATION | LAST_ACCESS | LAST_MODIFIED )


// INTERFACE ERROR
// These errors are to be applied by the phyiscal interface.
#define READ_SECTOR_ERROR       0x01
#define READ_SECTOR_TIMEOUT     0x02
#define READ_SECTOR_SUCCESSFUL  0x04


// BOOT SECTOR ERROR FLAGS
#define CORRUPT_BOOT_SECTOR              0x0001
#define NOT_BOOT_SECTOR                  0x0002
#define INVALID_BYTES_PER_SECTOR         0x0004
#define INVALID_SECTORS_PER_CLUSTER      0x0008
#define BOOT_SECTOR_NOT_FOUND            0x0010
#define BOOT_SECTOR_VALID                0x0020


typedef struct BiosParameterBlock{
    uint16_t bytesPerSector;
    uint8_t  sectorsPerCluster;
    uint16_t reservedSectorCount;
    uint8_t  numberOfFats;
    uint32_t fatSize32;
    uint32_t rootCluster;

    uint32_t bootSectorAddress;
    uint32_t dataRegionFirstSector;


}BiosParameterBlock;

uint16_t FAT_GetBiosParameterBlock(BiosParameterBlock * bpb);



/******************************************************************************
 * DESCRIPTION 
 * Sets currentDirectory to newDirectory, if found, by updating the members of
 * the FatCurrentDirectory struct with values corresponding to a matching 
 * directory entry for newDirectory.
 *
 * ARGUMENTS 
 * (1) *currentDirectory : ptr to a FatCurrentDirectory struct whose members
 *                         must point to a valid FAT32 directory.
 * (2) *newDirectory : ptr to a string that specifies the long name of the 
 *                     intended new directory. newDirectory can only be a short
 *                     name if there is no long name for the entry.
 * RETURNS 
 * FAT Error Flag. This can be read by passing it to PrintFatError(ErrorFlag).
******************************************************************************/
uint16_t SetFatCurrentDirectory(
                FatCurrentDirectory *currentDirectory, 
                char *newDirectory,
                BiosParameterBlock * bpb);



/******************************************************************************
 * DESCRIPTION
 * Prints the contents of currentDirectory to the screen. Which contents are 
 * printed is specified by ENTRY_FLAG (see ENTRY FLAGS list above.)
 * 
 * ARGUMENT
 * currentDirectory : ptr to a FatCurrentDirectory struct whose members must
 *                    point to a valid FAT32 directory.
 * RETURNS 
 * FAT Error Flag. This can be read by passing it to PrintFatError(ErrorFlag).
******************************************************************************/
uint16_t PrintFatCurrentDirectoryContents(
                FatCurrentDirectory *currentDirectory, 
                uint8_t ENTRY_FLAG, 
                BiosParameterBlock * bpb);



/******************************************************************************
 * DESCRIPTION
 * Prints the contents of *file to a terminal/screen.
 * 
 * ARGUMENTS
 * (1) *currentDirectory : ptr to a FatCurrentDirectory struct whose members
 *                         must point to a valid FAT32 directory.
 * (2) *file : ptr to string that is the long name of the file to be printed 
 *             to the screen. *file can only point to a short name if there is 
 *             no long name for that entry.
 * RETURNS 
 * FAT Error Flag. This can be read by passing it to PrintFatError(ErrorFlag).
******************************************************************************/
uint16_t PrintFatFileContents(
                FatCurrentDirectory * currentDirectory, 
                char * file,
                BiosParameterBlock * bpb);



/******************************************************************************
 * DESCRIPTION
 * Prints the error code returned by a FAT functions.
 * 
 * ARGUMENT
 * err : uin8_t value which is the error to be printed by the function.
******************************************************************************/
void PrintFatError(uint16_t err);


#endif //FAT_H