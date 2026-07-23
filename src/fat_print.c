/*
 * File       : FAT_PRINT.C
 * Version    : 2.0
 * License    : GNU GPLv3
 * Author     : Joshua Fain
 * Copyright (c) 2025
 * 
 * Implementation of FAT_PRINT.H
 */

#include <stdint.h>
#include <string.h>
#include "prints.h"
//#include "fat_bpb.h"
#include "fat.h"
#include "fat_disk_if.h"

static uint8_t pvt_CheckName(const char nameStr[]);
static uint32_t pvt_GetNextClusIndex(uint32_t clusIndex, const BPB *bpb);
static void pvt_PrintEntFields(const uint8_t *byte, uint8_t flags);
static uint8_t pvt_PrintFile(const uint8_t snEnt[], const BPB *bpb);

/*
 ******************************************************************************
 *                                   FUNCTIONS   
 ******************************************************************************
 */

/*
 * ----------------------------------------------------------------------------
 *                                       PRINT BIOS PARAMETER BLOCK ERROR FLAGS 
 * 
 * Description : Print the Bios Parameter Block Error Flag. 
 * 
 * Arguments   : err     BPB error flag(s).
 * 
 * Returns     : void
 * ----------------------------------------------------------------------------
 */
void fat_PrintErrorBPB(uint8_t err)
{  
  switch(err)
  {
    case BPB_VALID:
      print_Str("BPB_VALID ");
      break;
    case CORRUPT_BPB:
      print_Str("CORRUPT_BPB ");
      break;
    case NOT_BPB:
      print_Str("NOT_BPB ");
      break;
    case INVALID_BYTES_PER_SECTOR:
      print_Str("INVALID_BYTES_PER_SECTOR");
      break;
    case INVALID_SECTORS_PER_CLUSTER:
      print_Str("INVALID_SECTORS_PER_CLUSTER");
      break;
    case BPB_NOT_FOUND:
      print_Str("BPB_NOT_FOUND");
      break;
    case FAILED_READ_BPB:
      print_Str("FAILED_READ_BPB");
      break;
    default:
      print_Str("UNKNOWN_ERROR");
      break;
  }
}

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
uint8_t fat_PrintDir(const FatDir *dir, uint8_t entFlds, const BPB *bpb)
{
  // for function return errors. This is the loop cond. and the return value.
  uint8_t err;

  // 
  // Create and initialize a FatEntry instance. This will set the snEntClusIndx
  // member to the root directory and all other members to 0 or null strings, 
  // then update the snEntClusIndx to point to the first cluster index of the 
  // FatDir instance (dir).
  //
  FatEntry ent;
  fat_InitEntry(&ent, bpb);
  ent.snEntClusIndx = dir->fstClusIndx;

  // 
  // set the ent FatEntry instance to the next entry in the directory, then 
  // print the entry and fields according to entFlds. After all entries in the
  // dir have been loaded, fat_SetNextEntry will return END_OF_DIRECTORY.
  //
  while ((err = fat_SetNextEntry(&ent, bpb)) == SUCCESS)
  { 
    // Do not print entry if it is hidden and hidden filter flag is not set
    if (ent.snEnt[ATTR_BYTE_OFFSET] & HIDDEN_ATTR && !(entFlds & HIDDEN))
      continue;

    // Do not print entry if it is the Volume ID entry
    if (ent.snEnt[ATTR_BYTE_OFFSET] & VOLUME_ID_ATTR)
      continue;
    
    // Print short names if the SHORT_NAME filter flag is set.
    if ((entFlds & SHORT_NAME) == SHORT_NAME)
    {
      pvt_PrintEntFields(ent.snEnt, entFlds);
      print_Str(ent.snStr);
    }

    // Print long names if the LONG_NAME filter flag is set.
    if ((entFlds & LONG_NAME) == LONG_NAME)
    {
      pvt_PrintEntFields(ent.snEnt, entFlds);
      print_Str(ent.lnStr);
    }
  }
  // return END_OF_DIRECTORY if successful. Any other value returned is error.
  return err;
}

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
uint8_t fat_PrintFile(const FatDir *dir, const char fileStr[], const BPB *bpb)
{
  // for function return errors. This is the loop cond. and the return value.
  uint8_t err;
    
  if (pvt_CheckName(fileStr) == INVALID_NAME)
    return INVALID_NAME;

  // 
  // Create and initialize a FatEntry instance. This will set the snEntClusIndx
  // member to the root directory and all other members to 0 or null strings, 
  // then, update the snEntClusIndx to point to the first cluster index of the 
  // FatDir instance (dir).
  //
  FatEntry ent;
  fat_InitEntry(&ent, bpb);
  ent.snEntClusIndx = dir->fstClusIndx;

  // 
  // Search for a file matching fileStr in the current directory. Do this
  // by calling fat_SetNextEntry() to set the FatEntry instance to the next
  // entry in the directory. This is repeated until a file name is returned
  // that matches fileStr. Once a match is found then the "private"
  // function, pvt_PrintFile() is called to print this file. If no matching 
  // file is found, then the loop will exit once END_OF_DIRECTORY is returned.
  //
  while ((err = fat_SetNextEntry(&ent, bpb)) == SUCCESS) 
  { 
    // if entry is a directory, continue
    if (ent.snEnt[ATTR_BYTE_OFFSET] & DIR_ENTRY_ATTR)
      continue;

    // if matching file is found print its contents
    if (!strcmp(ent.lnStr, fileStr))
    {
      print_Str("\n\n\r");
      err = pvt_PrintFile(ent.snEnt, bpb);  //END_OF_FILE or FAILED_READ_SECTOR
      return err;
    }
  }
  return err;                               // no matching file was found.
}


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
void fat_PrintError (uint8_t err)
{  
  switch(err)
  {
    case SUCCESS: 
      print_Str("\n\rSUCCESS");
      break;
    case END_OF_DIRECTORY:
      print_Str("\n\rEND_OF_DIRECTORY");
      break;
    case INVALID_NAME:
      print_Str("\n\rINVALID_NAME");
      break;
    case FILE_NOT_FOUND:
      print_Str("\n\rFILE_NOT_FOUND");
      break;
    case DIR_NOT_FOUND:
      print_Str("\n\rDIR_NOT_FOUND");
      break;
    case CORRUPT_FAT_ENTRY:
      print_Str("\n\rCORRUPT_FAT_ENTRY");
      break;
    case END_OF_FILE:
      print_Str("\n\rEND_OF_FILE");
      break;
    case FAILED_READ_SECTOR:
      print_Str("\n\rFAILED_READ_SECTOR");
      break;
    default:
      print_Str("\n\rUNKNOWN_ERROR");
  }
}


/*
 ******************************************************************************
 *                           "PRIVATE" FUNCTIONS    
 ******************************************************************************
 */
/*
 * ----------------------------------------------------------------------------
 *                                               (PRIVATE) CHECK FOR LEGAL NAME
 *  
 * Description : Checks whether a string is a valid and legal FAT entry name. 
 * 
 * Arguments   : nameStr   - Pointer to the string to be verified as a legal 
 *                           FAT entry name.
 * 
 * Returns     : SUCCESS or INVALID_NAME
 * -----------------------------------------------------------------------------
 */
static uint8_t pvt_CheckName(const char nameStr[])
{
  // check that long name is not too large for current settings
  if (strlen(nameStr) > LN_STR_LEN_MAX) 
    return INVALID_NAME;
  
  // illegal if empty string or begins with a space char
  if (strcmp(nameStr, "") == 0 || nameStr[0] == ' ') 
    return INVALID_NAME;

  // illegal if contains an illegal char. Ends with null to treat as string.
  const char illCharsArr[] = {'\\','/',':','*','?','"','<','>','|','\0'};
  for (const char *namePtr = nameStr; *namePtr; ++namePtr)
    for (const char *illPtr = illCharsArr; *illPtr;)
      if (*nameStr == *illPtr++)
        return INVALID_NAME;

  // illegal if all space characters
  while (*nameStr)
    if (*nameStr++ != ' ')
      return SUCCESS;
  return INVALID_NAME;
}

/*
 * ----------------------------------------------------------------------------
 *                                      (PRIVATE) GET FAT INDEX OF NEXT CLUSTER
 * 
 * Description : Finds and returns the next FAT cluster index.
 * 
 * Arguments   : clusIndex   - The current cluster's FAT index.
 *               bpb         - Pointer to the BPB struct instance.
 * 
 * Returns     : A file or dir's next FAT cluster index. If END_CLUSTER is 
 *               returned, the current cluster is the last of the file or dir.
 * 
 * Notes       : The returned value locates the index in the FAT. The index is
 *               offset (typically by -2) from the actual cluster number in the
 *               data region. The root cluster is always cluster 0 in the data
 *               region, but its FAT index is 2 or higher.
 * ----------------------------------------------------------------------------
 */
static uint32_t pvt_GetNextClusIndex(uint32_t clusIndx, const BPB *bpb)
{
  // calculate address of sector containing the current cluster index
  uint16_t fatIndxsPerSec = bpb->bytesPerSec / BYTES_PER_INDEX;
  uint32_t fatSectorToRead = (clusIndx / fatIndxsPerSec) + bpb->rsvdSecCnt;

  // load current cluster's index sector into secArr
  uint8_t secArr[bpb->bytesPerSec];
  fatDisk_ReadSector(fatSectorToRead, secArr);

  // Value at the current cluster index is the index of the next cluster.
  uint32_t nextClusIndx = 0;
  uint16_t posNextClusIndxInSec = BYTES_PER_INDEX 
                                  * (clusIndx % fatIndxsPerSec);
 
  // load the index of the next cluster.
  for (uint8_t offset = BYTES_PER_INDEX - 1; offset > 0; --offset)
  {
    nextClusIndx |= secArr[posNextClusIndxInSec + offset];
    nextClusIndx <<= 8;
  }
  nextClusIndx |= secArr[posNextClusIndxInSec];

  return nextClusIndx;
}

/*
 * ----------------------------------------------------------------------------
 *                                      (PRIVATE) PRINT THE FIELDS OF FAT ENTRY
 * 
 * Description : Prints a FatEntry instance's fields according to flags param.
 *
 * Arguments   : secArr   - Pointer to an array holding then 32 bit short name
 *                          entry to be printed. Field values are located here.
 *               flags    - Entry Field Flags specifying which fields to print.
 * 
 * Returns     : void 
 * ----------------------------------------------------------------------------
 */
static void pvt_PrintEntFields(const uint8_t secArr[], uint8_t flags)
{
  print_Str ("\n\r");

  // Print creation date and time 
  if (CREATION & flags)
  {
    uint16_t createTime;
    uint16_t createDate;

    // load create date and time
    createDate = secArr[CREATION_DATE_BYTE_OFFSET_1];
    createDate <<= 8;
    createDate |= secArr[CREATION_DATE_BYTE_OFFSET_0];
    createTime = secArr[CREATION_TIME_BYTE_OFFSET_1];
    createTime <<= 8;
    createTime |= secArr[CREATION_TIME_BYTE_OFFSET_0];

    // print month
    print_Str("    ");
    uint8_t month = MONTH_CALC(createDate);
    if (month < 10)
      print_Str("0");    
    print_Dec(month);
    print_Str("/");

    // print day
    uint8_t day = DAY_CALC(createDate);
    if (day < 10)
      print_Str("0");
    print_Dec(day);
    print_Str("/");

    // print year
    print_Dec((uint16_t)YEAR_CALC(createDate));
    print_Str("  ");

    // print hours
    uint8_t hour = HOUR_CALC(createTime);
    if (hour < 10) 
      print_Str("0");
    print_Dec(hour);
    print_Str(":");

    // print minutes
    uint8_t min = MIN_CALC(createTime);
    if (min < 10)
      print_Str("0");
    print_Dec(min);
    print_Str(":");

    // print seconds (resolution is 2 seconds).
    uint8_t sec = SEC_CALC(createTime);
    if (sec < 10) 
      print_Str("0");
    print_Dec(sec);
  }

  // Print last access date
  if (LAST_ACCESS & flags)
  {
    uint16_t lastAccDate;

    // load last access date
    lastAccDate = secArr[LAST_ACCESS_DATE_BYTE_OFFSET_1];
    lastAccDate <<= 8;
    lastAccDate |= secArr[LAST_ACCESS_DATE_BYTE_OFFSET_0];

    // print month
    print_Str("     ");
    uint8_t month = MONTH_CALC(lastAccDate);
    if (month < 10)
      print_Str("0");
    print_Dec(month);
    print_Str("/");

    // print day
    uint8_t day = DAY_CALC(lastAccDate);
    if (day < 10)
      print_Str("0");
    print_Dec(day);
    print_Str("/");

    // print year
    print_Dec((uint16_t)YEAR_CALC(lastAccDate));
  }

  // Print last modified date / time
  if (LAST_MODIFIED & flags)
  {
    uint16_t writeDate;
    uint16_t writeTime;
    
    // Load last modified write date and time
    writeDate = secArr[WRITE_DATE_BYTE_OFFSET_1];
    writeDate <<= 8;
    writeDate |= secArr[WRITE_DATE_BYTE_OFFSET_0];

    writeTime = secArr[WRITE_TIME_BYTE_OFFSET_1];
    writeTime <<= 8;
    writeTime |= secArr[WRITE_TIME_BYTE_OFFSET_0];
  
    // print month
    print_Str("     ");
    uint8_t month = MONTH_CALC(writeDate);
    if (month < 10) 
      print_Str("0");
    print_Dec(month);
    print_Str("/");
    
    // print day
    uint8_t day = DAY_CALC(writeDate);
    if (day < 10) 
      print_Str("0");
    print_Dec(day);
    print_Str("/");

    // print year
    print_Dec((uint16_t)YEAR_CALC(writeDate));
    print_Str("  ");

    // print hour
    uint8_t hour = HOUR_CALC(writeTime);
    if (hour < 10)
      print_Str("0");
    print_Dec(hour);
    print_Str(":");

    // print minute
    uint8_t min = MIN_CALC(writeTime);   
    if (min < 10) 
      print_Str("0");
    print_Dec(min);
    print_Str(":");

    // print second
    uint8_t sec = SEC_CALC(writeTime);
    if (sec < 10) 
      print_Str("0");
    print_Dec(sec);
  }
  print_Str("     ");

  // Print file size in bytes
  if (FILE_SIZE & flags)
  {
    uint32_t fileSize;    
    
    // load file size
    fileSize = secArr[FILE_SIZE_BYTE_OFFSET_3];
    fileSize <<= 8;
    fileSize |= secArr[FILE_SIZE_BYTE_OFFSET_2];
    fileSize <<= 8;
    fileSize |= secArr[FILE_SIZE_BYTE_OFFSET_1];
    fileSize <<= 8;
    fileSize |= secArr[FILE_SIZE_BYTE_OFFSET_0];

    // Print spaces for formatting output. Add 1 to prevent starting at 0.
    for (uint64_t sp = 1 + fileSize / FS_UNIT; sp < GIGA / FS_UNIT; sp *= 10)
      print_Str(" ");

    // print file size and selected units
    print_Dec(fileSize / FS_UNIT);
    if (FS_UNIT == KILO)                               
      print_Str("KB  ");
    else  
      print_Str("B  ");
  }

  // print entry type
  if (TYPE & flags)
  {
    if (secArr[ATTR_BYTE_OFFSET] & DIR_ENTRY_ATTR) 
      print_Str(" <DIR>   ");
    else 
      print_Str(" <FILE>  ");
  }
}


/*
 * ----------------------------------------------------------------------------
 *                                                   (PRIVATE) PRINT A FAT FILE
 * 
 * Description : Performs 'print file' operation. This will output the contents
 *               of any file to the screen.
 * 
 * Arguments   : snEnt   - Pointer to 32 byte array holding the sn entry.
 *               bpb     - Pointer to the BPB struct instance.
 * 
 * Returns     : END_OF_FILE (success) or FAILED_READ_SECTOR fat error flag.
 * ----------------------------------------------------------------------------
 */
static uint8_t pvt_PrintFile(const uint8_t snEnt[], const BPB *bpb)
{
  //get FAT index for file's first cluster
  uint32_t clus; 
  clus = snEnt[FST_CLUS_INDX_BYTE_OFFSET_3];
  clus <<= 8;
  clus |= snEnt[FST_CLUS_INDX_BYTE_OFFSET_2];
  clus <<= 8;
  clus |= snEnt[FST_CLUS_INDX_BYTE_OFFSET_1];
  clus <<= 8;
  clus |= snEnt[FST_CLUS_INDX_BYTE_OFFSET_0];

  // loop over clusters to read in and print file
  do
  {
    // loop over sectors in the cluster to read in and print file
    for (uint32_t secNumInClus = 0; secNumInClus < bpb->secPerClus; 
         ++secNumInClus)
    {
      // calculate address of the sector on the physical disk
      uint32_t secNumOnDisk = secNumInClus + bpb->dataRegionFirstSector
                          + (clus - bpb->rootClus) * bpb->secPerClus;

      // read disk sector into the sector array
      uint8_t secArr[bpb->bytesPerSec];
      if (fatDisk_ReadSector(secNumOnDisk, secArr) 
          == FAILED_READ_SECTOR)
        return FAILED_READ_SECTOR;

      for (uint16_t byteNum = 0; byteNum < bpb->bytesPerSec; ++byteNum)
      {
        // end of file flag. Set to 1 if eof is detected.
        uint8_t eof = 0;

        // 
        // for output formatting. Currently reads are NOT text mode, so "\n\r"
        // is not automatically printed when "\n" is present by itself.
        //
        if (secArr[byteNum] == '\n') 
          print_Str ("\n\r");
        
        // else if not 0, just print the character directly to the screen.
        else if (secArr[byteNum])
        {
          // two byte array for single char string, to use print_Str.
          char str[2] = {secArr[byteNum], '\0'};
          print_Str(str);
        }
        // else character is zero. Possible indicatin of eof.
        else 
        {
          // assume eof and set flag
          eof = 1;
          
          // confirm rest of bytes in the current sector are 0
          for (++byteNum; byteNum < bpb->bytesPerSec; ++byteNum)
          {
            // if any byte is not 0 then not at eof. Reset eof to 0
            if (secArr[byteNum]) 
            { 
              --byteNum;
              eof = 0;
              break;
            }
          }
        }
        if (eof)
          return END_OF_FILE;
      }
    }
  } 
  while ((clus = pvt_GetNextClusIndex(clus, bpb)) != END_CLUSTER);
  
  return END_OF_FILE;
}

