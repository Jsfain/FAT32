/*
 * File    : FAT.C
 * Version : 0.0.0.2
 * Author  : Joshua Fain
 * Target  : ATMega1280
 * License : MIT
 * Copyright (c) 2020 Joshua Fain
 * 
 * Implementation of FAT.H
 */

#include <string.h>
#include <stdlib.h>
#include <avr/io.h>
#include "fat_bpb.h"
#include "fat.h"
#include "prints.h"
#include "usart0.h"
#include "fat_to_disk.h"

/*
 ******************************************************************************
 *                      "PRIVATE" FUNCTION PROTOTYPES (and MACROS)  
 ******************************************************************************
 */

static uint8_t  pvt_CheckName(char *nameStr);
static uint8_t  pvt_SetDirToParent(FatDir *dir, BPB *bpb);
static uint32_t pvt_GetNextClusIndex(uint32_t currClusIndx, BPB *bpb);
static void     pvt_PrintEntFields(uint8_t *byte, uint8_t flags);
static void     pvt_PrintShortName(const uint8_t *byte);
static uint8_t  pvt_PrintFile(uint8_t *fileSec, BPB *bpb);
static void     pvt_LoadLongName(int lnFirstEnt, int lnLastEnt, 
                            uint8_t *secArr, char *lnStr, uint8_t *lnStrIndx);
static void     pvt_UpdateFatEntryState(char *lnStr, uint16_t entPos, 
                            uint8_t snEntSecNumInClus, uint32_t snEntClusIndx,
                            uint16_t snPosCurrSec, uint16_t snPosNextSec,
                            uint8_t lnFlags, uint8_t  *secArr, FatEntry *ent);

// macros used by the local static (private) functions
#define LEGAL   0
#define ILLEGAL 1

/*
 ******************************************************************************
 *                                FUNCTIONS
 ******************************************************************************
 */

/* 
 * ----------------------------------------------------------------------------
 *                                                           SET ROOT DIRECTORY
 *                                        
 * Description : Sets an instance of FatDir to the root direcotry.
 *
 * Arguments   : dir     Pointer to the FatDir instance to be set to the root
 *                       directory.
 *
 *               bpb     Pointer to a valid instance of a BPB struct.
 *
 * Returns     : void
 * ----------------------------------------------------------------------------
 */
void fat_SetDirToRoot(FatDir *dir, BPB *bpb)
{
  strcpy(dir->snStr, "/");
  strcpy(dir->snPathStr, "");
  strcpy(dir->lnStr, "/");
  strcpy(dir->lnPathStr, "");
  dir->fstClusIndx = bpb->rootClus;
}

/*
 * ----------------------------------------------------------------------------
 *                                                         INITIALIZE FAT ENTRY
 *                                      
 * Description : Initializes an entry of FatEntry. Once done this should be 
 *               set to start at the first entry of the ROOT directory.
 * 
 * Arguments   : ent     Pointer to the FatEntry instance to be initialized.
 *            
 *               bpb     Pointer to a valid instance of a BPB struct.
 * 
 * Returns     : void
 * ----------------------------------------------------------------------------
 */
void fat_InitEntry(FatEntry *ent, BPB *bpb)
{
  // set long and short names to empty strings
  ent->lnStr[0] = '\0';
  ent->snStr[0] = '\0';
  
  // fill short name entry array with 0's
  for(uint8_t pos = 0; pos < ENTRY_LEN; pos++)
    ent->snEnt[pos] = 0;

  // set rest of the FatEntry members to 0. 
  ent->snEntSecNumInClus = 0;
  ent->entPos = 0;
  ent->lnFlags = 0;
  ent->snPosCurrSec = 0;
  ent->snPosCurrSec = 0;

  // Set the cluster index to point to the ROOT directory.
  ent->snEntClusIndx = bpb->rootClus;
}

/*
 * ----------------------------------------------------------------------------
 *                                                  SET FAT ENTRY TO NEXT ENTRY 
 *                                      
 * Description : Updates the FatEntry instance, currEnt, to the next entry in 
 *               the current directory (currDir).
 * 
 * Arguments   : currDir     Current directory. Pointer to a FatDir instance.
 * 
 *               currEnt     Current entry. Pointer to a FatEntry instance.
 * 
 *               bpb         Pointer to a valid instance of a BPB struct.
 *
 * Returns     : A FAT Error Flag. If any value other than SUCCESS is returned 
 *               then the function was unable to update the FatEntry.
 * -----------------------------------------------------------------------------
 */
uint8_t fat_SetNextEntry(FatDir *currDir, FatEntry *currEnt, BPB *bpb)
{
  //
  // capture currEnt state in local vars. These will be updated by the function
  // and if successful the corresponding currEnt memebers will be updated.
  //
  uint32_t clusIndx = currEnt->snEntClusIndx;
  uint8_t  lnFlags = currEnt->lnFlags;
  uint16_t snPosCurrSec = currEnt->snPosCurrSec;
  uint16_t snPosNextSec = currEnt->snPosNextSec;

  // Used to initialize loop variables. Set to 0 after first iterations. 
  uint8_t firstIterationFlag = 1;

  //
  // Loop to search for the next entry. This loop will run until reaching the
  // end of the last cluster of the directory, causing the function to exit
  // with END_OF_DIRECTORY, or until a 'next' entry is found.
  // 
  // The process for finding the next entry is:
  // 1) Load the current sector into currSecArr[].
  // 2) Locate the starting entry and check if it is a long name.
  // 3.1) If it is not a long name, skip to the bottom of the loop and load the
  //      curreEnt members with those of this short name entry and return.
  // 3.2) If the entry is a long name, set the LN_EXISTS flag in lnFlags.
  // 4) Read the 'long name order' value in byte 1 of the entry to determine
  //    how many 32-byte entries the long name requires. 
  // 5) Use the length of the long name to determine how the long name, and its
  //    short name, are distributed among adjacent sectors.
  // 6) If it is determined that the long and short name are within the current
  //    sector then simply load the currEnt members with their new values.
  // 7) The other long/short name distribution options are:
  //    A) The long name exists entirely within the current sector, but 
  //       occupies the last entry position causing the short name to be placed
  //       at the first entry position of the next sector. If this is the case 
  //       then the LN_LAST_SEC_ENTRY flag in lnFlags is set.
  //    B) The long name is partially in the current sector and partially in
  //       the next sector, in which case the short name entry would be in the
  //       next sector as well. If this is the case, then the LN_CROSS_SEC flag
  //       is set in lnFlags.
  // 8) If either 7.A or 7.B occurs, then the next sector must be read into 
  //    nextSecArr[] while still holding the current sector in currSecArr[]. 
  // 9) The LN_CROSS_SEC and LN_LAST_SEC_ENTRY situations will have to be 
  //    handled differently in order to read in the long name, but either
  //    way the short name entry is in the next sector. 
  // 10) Once the variables have been set, the currEnt members can be updated
  //     and the function exits. 
  // 11) If no 'next' entry is found, then the function will exit with 
  //     END_OF_DIRECTORY and the FatEnt fields will not be updated.
  //
  // There are several other error checks and entry position adjustments that
  // must be made, but the above lists the main steps.
  //
  do 
  { 
    //
    // Init currSecNumInClus to snEntSecNumInClus member of currEnt on first
    // iteration. All other iterations should init the variable to 0.
    //   
    uint8_t currSecNumInClus = currEnt->snEntSecNumInClus;
    if (firstIterationFlag == 0) 
      currSecNumInClus = 0;

    // Cluster loop. Loop through the sectors in the cluster.
    for (; currSecNumInClus < bpb->secPerClus; currSecNumInClus++)
    {
      uint8_t  currSecArr[bpb->bytesPerSec];// current 512 byte sector
      uint32_t currSecNumPhys;              // physical disk sector number
      uint8_t  err;                         // for returned function errors
      uint16_t entPos = 0;                  // init entry position in sector

      // calculate location of the current sector on the physical (disk)
      currSecNumPhys = currSecNumInClus + bpb->dataRegionFirstSector
                       + (clusIndx - bpb->rootClus) * bpb->secPerClus;

      // load the sector at location calculated above into currSecArr
      err = FATtoDisk_ReadSingleSector(currSecNumPhys, currSecArr);
      if (err == FTD_READ_SECTOR_FAILED)
        return FAILED_READ_SECTOR;

      //
      // Init local entPos variable to the entPos member of currEnt on first
      // iteration. All other iterations should init the variable to 0.
      // 
      if (firstIterationFlag != 0)
      {
        entPos = currEnt->entPos;
        firstIterationFlag = 0;
      }

      // Sector loop. Loop through the entries in the sector.
      for (; entPos < bpb->bytesPerSec; entPos = entPos + ENTRY_LEN)
      {
        // 
        // This block performs some checks to verify entPos is pointing to the
        // correct position in the sector and makes adjustments to it if it is
        // not. This is only needed if the last entry checked had a long name
        // in addition to its short name.
        // 
        if (lnFlags & LN_EXISTS)
        {
          // used for possible negative ENTRY_LEN adjustment
          int tempSecPos = snPosCurrSec; 

          // greater than the value of the last entry position in a sector
          if (snPosCurrSec >= LAST_ENTPOS_IN_SEC)
          {
            // get next sector if entPos does not point to sector's first entry
            if (entPos != 0) 
              break; 
            else 
              tempSecPos = -ENTRY_LEN; 
          }

          if (lnFlags & (LN_CROSS_SEC | LN_LAST_SEC_ENTRY))
          {
            entPos = snPosNextSec + ENTRY_LEN; 
            snPosNextSec = 0;
          }
          else 
          {
            entPos = tempSecPos + ENTRY_LEN;
            snPosCurrSec = 0;
          }
        }

        // reset
        lnFlags = 0;

        // if first value of entry is 0, rest of entries should be empty
        if (currSecArr[entPos] == 0) 
          return END_OF_DIRECTORY;

        // only continue with current entry if it has not marked for deletion
        if (currSecArr[entPos] != DELETED_ENTRY_TOKEN)
        {
          // entry's attribute byte (byte 11)
          uint8_t attrByte = currSecArr[entPos + ATTR_BYTE_OFFSET];

          // entPos points to a long name entry?
          if ((attrByte & LN_ATTR_MASK) == LN_ATTR_MASK)
          {
            // array for long name string. Init to array of nulls.
            char lnStr[LN_STR_LEN_MAX] = {'\0'};
            
            // entPos must be pointing to the last entry of long name here.
            if ( !(currSecArr[entPos] & LN_LAST_ENTRY)) 
              return CORRUPT_FAT_ENTRY;
            
            //
            // locate position of the short name in current sector. This is 
            // based on the number of entries required by the long name. 
            //
            snPosCurrSec = entPos + 
                           ENTRY_LEN * (LN_ORD_MASK & currSecArr[entPos]);
            
            // Set long name flags
            lnFlags |= LN_EXISTS;
            if (snPosCurrSec > bpb->bytesPerSec)              
              lnFlags |= LN_CROSS_SEC;
            else if (snPosCurrSec == SECTOR_LEN) 
              lnFlags |= LN_LAST_SEC_ENTRY;

            // Check if short name is in the next sector
            if (lnFlags & (LN_CROSS_SEC | LN_LAST_SEC_ENTRY))
            {
              // for 'next' 512 byte sector relative to current sector
              uint8_t  nextSecArr[bpb->bytesPerSec]; 
              uint32_t nextSecNumPhys;

              // 
              // locate next sector. If current sec number is the last sec in
              // the cluster (spc - 1) then the next sec is in the next cluster
              // else add 1 to current sec number for locate next sec.
              //
              if (currSecNumInClus >= bpb->secPerClus - 1)
              {
                nextSecNumPhys = bpb->dataRegionFirstSector
                               + (pvt_GetNextClusIndex(clusIndx, bpb) 
                               - bpb->rootClus)
                               * bpb->secPerClus;
              }
              else 
                nextSecNumPhys = 1 + currSecNumPhys;
              
              // load next sector into nextSecArr[].
              err = FATtoDisk_ReadSingleSector(nextSecNumPhys, nextSecArr);
              if (err == FTD_READ_SECTOR_FAILED)
                return FAILED_READ_SECTOR;
              
              // get location of short name entry in the next sector
              snPosNextSec = snPosCurrSec - bpb->bytesPerSec;
              attrByte = nextSecArr[snPosNextSec + ATTR_BYTE_OFFSET];

              // Verify snPosNextSec does not point to long name
              if ((attrByte & LN_ATTR_MASK) == LN_ATTR_MASK)
                return CORRUPT_FAT_ENTRY;
              
              //
              // Handles case when long name crosses sector boundary and the
              // short name is in the next sector.
              //
              if (lnFlags & LN_CROSS_SEC)
              {
                // used to index chars in string array when loading long name 
                uint8_t lnStrIndx = 0; 

                // Entry preceeding short name must be first entry of long name
                if ((nextSecArr[snPosNextSec - ENTRY_LEN] & LN_ORD_MASK) != 1)
                  return CORRUPT_FAT_ENTRY;         
                
                //
                // load long name into lnStr[]. The first function call is to
                // load portion of long name that is in the next sector, should
                // begin at entry 0. The second function call is to load long
                // name portion in the current sector, should begin at entPos.
                //
                pvt_LoadLongName(snPosNextSec - ENTRY_LEN, 0, nextSecArr,
                                 lnStr, &lnStrIndx);
                pvt_LoadLongName(LAST_ENTPOS_IN_SEC, entPos, currSecArr, 
                                 lnStr, &lnStrIndx);

                // update the currEnt members with values for the 'next' entry.
                pvt_UpdateFatEntryState(lnStr, entPos, currSecNumInClus, 
                                        clusIndx, snPosCurrSec, snPosNextSec,
                                        lnFlags, nextSecArr, currEnt);
                return SUCCESS;
              }

              //
              // Handles case when long name is entirely in the current sector 
              // and short name is in the next sector.
              //
              else if (lnFlags & LN_LAST_SEC_ENTRY)
              {
                // used to index chars in string array when loading long name
                uint8_t lnStrIndx = 0;

                // Entry preceeding short name must be first entry of long name
                if ((currSecArr[LAST_ENTPOS_IN_SEC] & LN_ORD_MASK) != 1)
                  return CORRUPT_FAT_ENTRY;

                // load long name into lnStr[]
                pvt_LoadLongName(LAST_ENTPOS_IN_SEC, entPos, currSecArr,      
                                 lnStr, &lnStrIndx);
                
                // update the currEnt members with values for the 'next' entry.
                pvt_UpdateFatEntryState(lnStr, entPos, currSecNumInClus, 
                                        clusIndx, snPosCurrSec, snPosNextSec,
                                        lnFlags, nextSecArr, currEnt);
                return SUCCESS;
              }
              else 
                return CORRUPT_FAT_ENTRY;
            }

            // Long and short name are in the current sector.
            else
            {   
              // used to index chars in string array when loading long name
              uint8_t lnStrIndx = 0;

              attrByte = currSecArr[snPosCurrSec + ATTR_BYTE_OFFSET];
              
              // Verify snPosNextSec does not point to long name
              if ((attrByte & LN_ATTR_MASK) == LN_ATTR_MASK)
                return CORRUPT_FAT_ENTRY;
      
              // entry preceeding short name must be first entry of long name
              if ((currSecArr[snPosCurrSec - ENTRY_LEN] & LN_ORD_MASK) != 1)
                return CORRUPT_FAT_ENTRY;
              
              // load long name into lnStr[]
              pvt_LoadLongName(snPosCurrSec - ENTRY_LEN, entPos, currSecArr,
                               lnStr, &lnStrIndx);
              
              // update the currEnt members with values for the 'next' entry.
              pvt_UpdateFatEntryState(lnStr, entPos, currSecNumInClus, 
                                      clusIndx, snPosCurrSec, snPosNextSec,
                                      lnFlags, currSecArr, currEnt);
              return SUCCESS;                          
            }                   
          }

          // Long name does not exist. Use short name instead.
          else
          {
            // long name not needed, since only short name is found.
            char lnDummyStr[1];

            attrByte = currSecArr[entPos + ATTR_BYTE_OFFSET];

            // update the currEnt members with values for the 'next' entry.
            pvt_UpdateFatEntryState(lnDummyStr, entPos, currSecNumInClus, 
                                    clusIndx, entPos, snPosNextSec, lnFlags,
                                    currSecArr, currEnt);
            return SUCCESS;  
          }
        }
      }
    }
  }
  // get index of the next cluster and continue looping if not last cluster 
  while ((clusIndx = pvt_GetNextClusIndex(clusIndx, bpb)) != END_CLUSTER);

  // return here if the end of the dir was reached without finding next entry.
  return END_OF_DIRECTORY;
}

/*
 * ----------------------------------------------------------------------------
 *                                                            SET FAT DIRECTORY
 *                                       
 * Description : Set a FatDir instance, dir, to the directory specified by 
 *               newDirStr.
 * 
 * Arguments   : dir           Pointer to the FatDir instance to be set to the
 *                             new directory.
 *             
 *               newDirStr     Pointer to a string that specifies the name of 
 *                             the new directory.
 * 
 *               bpb           Pointer to a valid instance of a BPB struct.
 *
 * Returns     : A FAT Error Flag. If any value other than SUCCESS is returned 
 *               then the function was unable to update the FatEntry.
 *  
 * Notes       : 1) This function can only set the directory to a child, or
 *                  the parent of the current instance of FatDir, when the 
 *                  function is called, or reset the instance to the ROOT 
 *                  directory.
 * 
 *               2) Do not include any paths (relative or absolute) in the 
 *                  newDirStr. newDirStr must be only be a directory name which
 *                  must be the name of a child directoy, or the parent, in the
 *                  current directory.
 * 
 *               3) If "." is passed as the newDirStr then the new directory
 *                  will be set to the parent of the current directory.
 *               
 *               4) newDirStr is case-sensitive.
 * 
 *               5) newDirStr must be a long name, unless a long name does not
 *                  exist for a directory. Only in this case can it be a short
 *                  name.
 * ----------------------------------------------------------------------------
 */
uint8_t fat_SetDir(FatDir *dir, char *newDirStr, BPB *bpb)
{
  // For function return error. Used as the loop conditon.
  uint8_t err;                                  

  if (pvt_CheckName(newDirStr) == ILLEGAL)
    return INVALID_DIR_NAME;

  // newDirStr == 'Current Directory'?
  else if (!strcmp(newDirStr, ".")) 
    return SUCCESS;
  
  // newDirStr == 'Parent Directory'?
  else if (!strcmp(newDirStr, ".."))
    // returns either FAILED_READ_SECTOR or SUCCESS
    return pvt_SetDirToParent(dir, bpb);

  // newDirStr == 'Root Directory' ?
  else if (!strcmp(newDirStr, "~"))
  {
    fat_SetDirToRoot(dir, bpb);
    return SUCCESS;
  }

  // 
  // Create and initialize a FatEntry instance. This will set the snEntClusIndx
  // member to the root directory and all other members to 0 or null strings, 
  // then, update the snEntClusIndx to point to the first cluster index of the 
  // FatDir instance (dir).
  //
  FatEntry *ent = malloc(sizeof(FatEntry));
  fat_InitEntry(ent, bpb);
  ent->snEntClusIndx = dir->fstClusIndx;

  // 
  // Search FatDir directory to see if a child directory matches newDirStr.
  // Done by repeatedly calling fat_SetNextEntry() to set a FatEntry instance
  // to the next entry in the directory and then comparing the lnStr member of 
  // the instance to newDirStr. Note that the lnStr member will be the same as
  // the snStr if a lnStr does not exist for the entry, therefore, short names
  // can only be used when a lnStr does not exist for the entry.
  //
  do 
  {
    err = fat_SetNextEntry(dir, ent, bpb);
    if (err != SUCCESS) 
      return err;
    
    //
    // check if long name of next entry matches the desired new directory. If 
    // it does match then set FatDir instance members to those of matching dir.
    //
    if (!strcmp(ent->lnStr, newDirStr) 
        && ent->snEnt[ATTR_BYTE_OFFSET] & DIR_ENTRY_ATTR)
    { 
      //
      // bytes 20, 21, 26 and 27 of a short name entry give the value of the
      // first cluster index in the FAT for that entry.
      //
      dir->fstClusIndx = ent->snEnt[21];
      dir->fstClusIndx <<= 8;
      dir->fstClusIndx |= ent->snEnt[20];
      dir->fstClusIndx <<= 8;
      dir->fstClusIndx |= ent->snEnt[27];
      dir->fstClusIndx <<= 8;
      dir->fstClusIndx |= ent->snEnt[26];
      
      uint8_t snLen = SN_NAME_STR_LEN - 1;
      if (strlen(newDirStr) < SN_NAME_STR_LEN - 1)
        snLen = strlen(newDirStr);

      char sn[SN_NAME_STR_LEN] = {'\0'};                                    
      for (uint8_t strPos = 0; strPos < snLen; strPos++)  
        sn[strPos] = ent->snEnt[strPos];

      // Append current directory name to the path
      strcat (dir->lnPathStr, dir->lnStr);
      strcat (dir->snPathStr, dir->snStr);

      //
      // Update directory name to new directory name. 
      // If current directory is not root then append '/'
      //
      if (dir->lnStr[0] != '/') 
        strcat(dir->lnPathStr, "/"); 
      strcpy(dir->lnStr, newDirStr);
      
      if (dir->snStr[0] != '/') 
        strcat(dir->snPathStr, "/");
      strcpy(dir->snStr, sn);
      
      return SUCCESS;
    }
  }
  while (err != END_OF_DIRECTORY);

  // Did not find matching child directory, so FatDir is not changed.
  return END_OF_DIRECTORY; 
}

/*
 * ----------------------------------------------------------------------------
 *                                          PRINT DIRECTORY ENTRIES TO A SCREEN
 *                                       
 * Description : Prints the contents of a directory, i.e. lists the file and
 *               directory entries of a FatDir instance.
 * 
 * Arguments   : dir         Pointer to a FatDir instance. This directory's
 *                           entries will be printed to the screen.
 *             
 *               entFld      Any combination of the Entry Field Flags. These
 *                           will specify which entry types, and which of their 
 *                           fields, will be printed to the screen.
 *               
 *               bpb         Pointer to a valid instance of a BPB struct.
 *
 * Returns     : A FAT Error Flag. If any value other than END_OF_DIRECTORY is
 *               returned then there was an issue.
 *  
 * Notes       : LONG_NAME and/or SHORT_NAME must be passed in the entFld
 *               argument. If not, then no entries will be printed.
 * ----------------------------------------------------------------------------
 */
uint8_t fat_PrintDir(FatDir *dir, uint8_t entFilt, BPB *bpb)
{
  // 
  // Create and initialize a FatEntry instance. This will set the snEntClusIndx
  // member to the root directory and all other members to 0 or null strings, 
  // then, update the snEntClusIndx to point to the first cluster index of the 
  // FatDir instance (dir).
  //
  FatEntry *ent = malloc(sizeof(FatEntry));
  fat_InitEntry(ent, bpb);
  ent->snEntClusIndx = dir->fstClusIndx;

  // 
  // Call fat_SetNextEntry() to set FatEntry to the next entry in the current
  // directory, dir, then print the entry and fields according to the entFilt
  // setting. Repeat until reaching END_OF_DIRECTORY is returned.
  //
  while (fat_SetNextEntry(dir, ent, bpb) != END_OF_DIRECTORY)
  { 
    // Only print hidden entries if the HIDDEN filter flag is been set.
    if (!(ent->snEnt[ATTR_BYTE_OFFSET] & HIDDEN_ATTR) || 
         (ent->snEnt[ATTR_BYTE_OFFSET] & HIDDEN_ATTR && entFilt & HIDDEN))
    {      
      // Print short names if the SHORT_NAME filter flag is set.
      if ((entFilt & SHORT_NAME) == SHORT_NAME)
      {
        pvt_PrintEntFields(ent->snEnt, entFilt);
        pvt_PrintShortName(ent->snEnt);
      }

      // Print long names if the LONG_NAME filter flag is set.
      if ((entFilt & LONG_NAME) == LONG_NAME)
      {
        pvt_PrintEntFields(ent->snEnt, entFilt);
        print_Str(ent->lnStr);
      }
    }   
  }
  return END_OF_DIRECTORY;
}

/*
 * ----------------------------------------------------------------------------
 *                                                         PRINT FILE TO SCREEN
 *                                       
 * Description : Prints the contents of a PLAIN TEXT file to the screen. 
 * 
 * Arguments   : dir             Pointer to a FatDir instance. This directory
 *                               must contain the entry for the file that will
 *                               be printed.
 *             
 *               fileNameStr     Pointer to a string. This is the name of the 
 *                               file who's contents will be printed.
 *               
 *               bpb             Pointer to a valid instance of a BPB struct.
 *
 * Returns     : A FAT Error Flag. If any value other than END_OF_FILE is 
 *               returned, then an issue occurred.
 *  
 * Notes       : fileNameStr must be a long name unless a long name for a given
 *               entry does not exist, in which case it must be a short name.
 * ----------------------------------------------------------------------------
 */
uint8_t fat_PrintFile(FatDir *dir, char *fileNameStr, BPB *bpb)
{
  // For function return values/errors. Used as the loop conditon.
  uint8_t err;

  if (pvt_CheckName(fileNameStr) == ILLEGAL)
    return INVALID_FILE_NAME;

  // 
  // Create and initialize a FatEntry instance. This will set the snEntClusIndx
  // member to the root directory and all other members to 0 or null strings, 
  // then, update the snEntClusIndx to point to the first cluster index of the 
  // FatDir instance (dir).
  //
  FatEntry *ent = malloc(sizeof(FatEntry));
  fat_InitEntry(ent, bpb);
  ent->snEntClusIndx = dir->fstClusIndx;

  // 
  // Search for the file (fileNameStr) in the current directory (dir). Do this
  // by calling fat_SetNextEntry() to set the FatEntry instance to the next
  // entry in the directory. This is repeated until a file name is returned
  // that matches fileNameStr. Once a match is found then the "private"
  // function, pvt_PrintFile() is called to print this file. If no matching 
  // file is found, then the loop will exit once END_OF_DIRECTORY is returned.
  //
  do 
  {
    // get the next entry
    err = fat_SetNextEntry (dir, ent, bpb);
    if (err != SUCCESS) 
      return err;
    
    // check if name of next entry matches fileNameStr
    if ( !strcmp(ent->lnStr, fileNameStr)
         && !(ent->snEnt[ATTR_BYTE_OFFSET] & DIR_ENTRY_ATTR))
    {                                                        
      print_Str("\n\n\r");
      
      //
      // matching file found. Now printing its contents. 
      // Returns either END_OF_FILE or FAILED_READ_SECTOR.
      //
      return pvt_PrintFile(ent->snEnt, bpb);
    }
  }
  while (err != END_OF_DIRECTORY);

  // no matching file was found. Nothing to print.
  return END_OF_DIRECTORY;
}

/*
 *-----------------------------------------------------------------------------
 *                                                         PRINT FAT ERROR FLAG
 * 
 * Description : Prints the FAT Error Flag passed as the arguement. 
 *
 * Arguments   : err     An error flag returned by one of the FAT functions.
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
    case INVALID_FILE_NAME:
      print_Str("\n\rINVALID_FILE_NAME");
      break;
    case FILE_NOT_FOUND:
      print_Str("\n\rFILE_NOT_FOUND");
      break;
    case INVALID_DIR_NAME:
      print_Str("\n\rINVALID_DIR_NAME");
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
 *                                                (PRIVATE) SET FAT ENTRY STATE
 * 
 * Description : Sets the state of a FatEntry instance according to the values 
 *               of the arguments passed in.
 * 
 * Arguments   : too many. see below.
 * 
 * Returns     : void
 * ----------------------------------------------------------------------------
 */
static void pvt_UpdateFatEntryState(char *lnStr, uint16_t entPos, 
                            uint8_t snEntSecNumInClus, uint32_t snEntClusIndx,
                            uint16_t snPosCurrSec, uint16_t snPosNextSec, 
                            uint8_t lnFlags, uint8_t *secArr, FatEntry *ent)
{
  // top section will set these variables to assist loading of FatEntry members
  char sn[SN_STR_LEN] = {'\0'};
  uint16_t snPos;  
  uint8_t Indx = 0;

  if (lnFlags & (LN_CROSS_SEC | LN_LAST_SEC_ENTRY))
    snPos = snPosNextSec;
  else
    snPos = snPosCurrSec;
 
  for (uint8_t byteNum = 0; byteNum < SN_NAME_STR_LEN - 1; byteNum++)
    if (secArr[snPos + byteNum] != ' ')
      sn[Indx++] = secArr[snPos + byteNum];

  if (secArr[snPos + SN_NAME_STR_LEN - 1] != ' ')
  {
    sn[Indx++] = '.';
    for (uint8_t byteNum = SN_NAME_STR_LEN - 1;
         byteNum < SN_STR_LEN - 2; byteNum++)
      if (secArr[snPos + byteNum] != ' ') 
        sn[Indx++] = secArr[snPos + byteNum];
  }

  // load FatEntry members  
  for (uint8_t byteNum = 0; byteNum < ENTRY_LEN; byteNum++)
    ent->snEnt[byteNum] = secArr[snPos + byteNum];
    
  ent->snEntSecNumInClus = snEntSecNumInClus;
  ent->snEntClusIndx = snEntClusIndx;
  ent->snPosCurrSec = snPosCurrSec;
  ent->snPosNextSec = snPosNextSec;
  ent->lnFlags = lnFlags;
  if (lnFlags & LN_EXISTS) 
    ent->entPos = entPos;
  else 
    ent->entPos = entPos + ENTRY_LEN;

  strcpy(ent->snStr, sn);
  if ( !(lnFlags & LN_EXISTS))
    strcpy(ent->lnStr, sn);
  else
    strcpy(ent->lnStr, lnStr);   
}

/*
 * ----------------------------------------------------------------------------
 *                                               (PRIVATE) CHECK FOR LEGAL NAME
 *  
 * Description : Checks whether a string is a valid/legal FAT entry name. 
 * 
 * Arguments   : nameStr     Pointer to the string to be verified as a legal 
 *                           FAT entry name.
 * 
 * Returns     : LEGAL or ILLEGAL
 * -----------------------------------------------------------------------------
 */
static uint8_t pvt_CheckName(char *nameStr)
{
  // check that long name is not too large for current settings
  if (strlen(nameStr) > LN_STR_LEN_MAX) 
    return ILLEGAL;
  
  // illegal if empty string or begins with a space char
  if (strcmp(nameStr, "") == 0 || nameStr[0] == ' ') 
    return ILLEGAL;

  // illegal if contains an illegal char. Arrary ends in null, treat as string
  char illCharsArr[] = {'\\','/',':','*','?','"','<','>','|','\0'};
  for (uint8_t strPos = 0; strPos < strlen(nameStr); strPos++)
    for (uint8_t iCharPos = 0; iCharPos < strlen(illCharsArr); iCharPos++)
      if (nameStr[strPos] == illCharsArr[iCharPos])
        return ILLEGAL;

  // illegal if all space characters
  for (uint8_t strPos = 0; strPos < strlen(nameStr); strPos++)  
    if (nameStr[strPos] != ' ')
      return LEGAL;                         

  return ILLEGAL;             
}

/*
 * ----------------------------------------------------------------------------
 *                                (PRIVATE) SET CURRENT DIRECTORY TO ITS PARENT
 *  
 *  Description : Sets a FatDir instance to its parent directory. 
 * 
 *  Arguments   : dir     Pointer to a FatDir instance. The members of this
 *                        instance will be set to its parent directory.
 *              
 *                bpb     Pointer to a valid instance of a BPB struct.
 * 
 *  Returns     : SUCCESS or FAILED_READ_SECTOR.
 * ----------------------------------------------------------------------------
 */
static uint8_t pvt_SetDirToParent(FatDir *dir, BPB *bpb)
{
  uint32_t parentDirFirstClus;
  uint32_t currSecNumPhys;
  uint8_t  currSecArr[bpb->bytesPerSec];
  uint8_t  err;

  // physical sector number on disk
  currSecNumPhys = bpb->dataRegionFirstSector 
                   + (dir->fstClusIndx - 2) * bpb->secPerClus;

  // load currSecArr with disk sector at curreSecNumPhys
  err = FATtoDisk_ReadSingleSector(currSecNumPhys, currSecArr);
  if (err == FTD_READ_SECTOR_FAILED)
   return FAILED_READ_SECTOR;

  //
  // byte 52, 53, 58,and 59 of the first sector of a directory contain the
  // value of the first FAT cluster index of its parent directory.
  //  
  parentDirFirstClus = currSecArr[53];
  parentDirFirstClus <<= 8;
  parentDirFirstClus |= currSecArr[52];
  parentDirFirstClus <<= 8;
  parentDirFirstClus |= currSecArr[59];
  parentDirFirstClus <<= 8;
  parentDirFirstClus |= currSecArr[58];

  // 
  // if dir is already the root directory, do nothing, else if root directory 
  // is the parent dir set FatDir members to the root directory, else set 
  // FatDir members to the parent directory.
  //
  if (dir->fstClusIndx == bpb->rootClus); 
  else if (parentDirFirstClus == 0)
    fat_SetDirToRoot(dir, bpb);
  else
  {          
    char tmpShortNamePath[PATH_STRING_LEN_MAX];
    char tmpLongNamePath[PATH_STRING_LEN_MAX];

    // load current path member values to temp strings
    strlcpy(tmpShortNamePath, dir->snPathStr, strlen(dir->snPathStr));
    strlcpy(tmpLongNamePath,  dir->lnPathStr, strlen(dir->lnPathStr));
    
    // create pointer to location in temp strings holding the final '/'
    char *shortNameLastDirInPath = strrchr(tmpShortNamePath, '/');
    char *longNameLastDirInPath  = strrchr(tmpLongNamePath , '/');
    
    // copy last directory in path string to the short/long name strings
    strcpy(dir->snStr, shortNameLastDirInPath + 1);
    strcpy(dir->lnStr, longNameLastDirInPath  + 1);

    // remove the last directory in path strings
    strlcpy(dir->snPathStr, tmpShortNamePath,
           (shortNameLastDirInPath + 2) - tmpShortNamePath);
    strlcpy(dir->lnPathStr,  tmpLongNamePath,  
           (longNameLastDirInPath  + 2) -  tmpLongNamePath);

    dir->fstClusIndx = parentDirFirstClus;
  }
  return SUCCESS;
}

/*
 * ----------------------------------------------------------------------------
 *                               (PRIVATE) LOAD A LONG NAME ENTRY INTO A STRING 
 * 
 * Description : Loads characters of a long name into a string (char array).  
 * 
 * Arguments   : lnFirstEnt     Position of the lowest order entry of the long 
 *                              name in *secArr.
 * 
 *               lnLastEnt      Position of the highest order entry of the long 
 *                              name in *secArr.
 * 
 *               secArr         Pointer to the array holding the contents of a 
 *                              single sector of a directory from a 
 *                              FAT-formatted disk.
 * 
 *               lnStr          Pointer to a string array that will be loaded
 *                              with the long name characters.
 * 
 *               lnStrIndx      Pointer updated by this function. Initial value
 *                              is the position in *lnStr where chars should 
 *                              begin loading.
 * 
 * Returns     : void 
 * 
 * Notes       : Must called twice if long name crosses sector boundary.
 * ----------------------------------------------------------------------------
 */
static void pvt_LoadLongName(int lnFirstEnt, int lnLastEnt, uint8_t *secArr, 
                             char *lnStr, uint8_t *lnStrIndx)
{
  // loop over the entries in the sector containing the long name
  for (int entPos = lnFirstEnt; entPos >= lnLastEnt; entPos -= ENTRY_LEN)
  {                                              
    // multiple loops over single entry to load the long name chars

    for (uint16_t byteNum = entPos + 1; byteNum < entPos + 11; byteNum++)                               
      // if char is zero or > 127 (out of ascii range) discard.
      if (secArr[byteNum] > 0 && secArr[byteNum] <= 127)
        lnStr[(*lnStrIndx)++] = secArr[byteNum];

    for (uint16_t byteNum = entPos + 14; byteNum < entPos + 26; byteNum++)                               
      // if char is zero or > 127 (out of ascii range) discard.
      if (secArr[byteNum] > 0 && secArr[byteNum] <= 127)
        lnStr[(*lnStrIndx)++] = secArr[byteNum];
    
    for (uint16_t byteNum = entPos + 28; byteNum < entPos + 32; byteNum++)                              
      // if char is zero or > 127 (out of ascii range) discard.
      if (secArr[byteNum] > 0 && secArr[byteNum] <= 127)
        lnStr[(*lnStrIndx)++] = secArr[byteNum];     
  }
}

/*
 * ----------------------------------------------------------------------------
 *                              (PRIVATE) GET THE FAT INDEX OF THE NEXT CLUSTER
 * 
 * Description : Finds and returns the next FAT cluster index.
 * 
 * Arguments   : currClusIndx     The current cluster's FAT index.
 *               
 *               bpb              Pointer to a valid instance of a BPB struct.
 * 
 * Returns     : A file or dir's next FAT cluster index. If END_CLUSTER is 
 *               returned, the current cluster is the last of the file or dir.
 * 
 * Notes       : The returned value locates the index in the FAT. The value
 *               must be offset by -2 when locating the cluster in the FAT's
 *               data region.
 * ----------------------------------------------------------------------------
 */
static uint32_t pvt_GetNextClusIndex(uint32_t currClusIndx, BPB *bpb)
{
  uint8_t  bytesPerClusIndx = 4;
  uint16_t numIndxdClusPerSecOfFat = bpb->bytesPerSec / bytesPerClusIndx;
  uint32_t clusIndx = currClusIndx / numIndxdClusPerSecOfFat;
  uint32_t clusIndxStartByte = 4 * (currClusIndx % numIndxdClusPerSecOfFat);
  uint32_t fatSectorToRead = clusIndx + bpb->rsvdSecCnt;
  uint8_t  sectorArr[bpb->bytesPerSec];
  
  FATtoDisk_ReadSingleSector(fatSectorToRead, sectorArr);

  clusIndx  = 0;
  clusIndx  = sectorArr[clusIndxStartByte + 3];
  clusIndx <<= 8;
  clusIndx |= sectorArr[clusIndxStartByte + 2];
  clusIndx <<= 8;
  clusIndx |= sectorArr[clusIndxStartByte + 1];
  clusIndx <<= 8;
  clusIndx |= sectorArr[clusIndxStartByte];

  return clusIndx;
}

/*
 * ----------------------------------------------------------------------------
 *                                      (PRIVATE) PRINT THE FIELDS OF FAT ENTRY
 * 
 * Description : Prints a FatEntry instance's fields according to flags.
 *
 * Arguments   : secArr     Pointer to an array holding the short name entry,
 *                          where the field values are located, to be printed.
 * 
 *               flags      Entry Field Flags specifying which fields to print.
 * 
 * Returns     : void 
 * ----------------------------------------------------------------------------
 */

static void pvt_PrintEntFields(uint8_t *secArr, uint8_t flags)
{
  print_Str ("\n\r");

  // Print creation date and time 
  if (CREATION & flags)
  {
    uint16_t createTime;
    uint16_t createDate;
  
    createTime = secArr[15];
    createTime <<= 8;
    createTime |= secArr[14];
    
    createDate = secArr[17];
    createDate <<= 8;
    createDate |= secArr[16];
    print_Str("    ");

    if ((createDate & 0x01E0) >> 5 < 10) 
      print_Str("0");

    print_Dec((createDate & 0x01E0) >> 5);
    print_Str("/");
    if ((createDate & 0x001F) < 10)
      print_Str("0");
    
    print_Dec(createDate & 0x001F);
    print_Str("/");
    print_Dec(1980 + ((createDate & 0xFE00) >> 9));
    print_Str("  ");
    if ((createTime & 0xF800) >> 11 < 10) 
      print_Str("0");
    
    print_Dec((createTime & 0xF800) >> 11);
    print_Str(":");
    if ((createTime & 0x07E0) >> 5 < 10)
      print_Str("0");
    
    print_Dec((createTime & 0x07E0) >> 5);
    print_Str(":");
    if (2 * (createTime & 0x001F) < 10) 
      print_Str("0");

    print_Dec(2 * (createTime & 0x001F));
  }

  // Print last access date
  if (LAST_ACCESS & flags)
  {
    uint16_t lastAccDate;

    lastAccDate = secArr[19];
    lastAccDate <<= 8;
    lastAccDate |= secArr[18];

    print_Str("     ");
    if ((lastAccDate & 0x01E0) >> 5 < 10)
      print_Str("0");

    print_Dec((lastAccDate & 0x01E0) >> 5);
    print_Str("/");
    if ((lastAccDate & 0x001F) < 10) 
      print_Str("0");

    print_Dec(lastAccDate & 0x001F);
    print_Str("/");
    print_Dec(1980 + ((lastAccDate & 0xFE00) >> 9));
  }

  // Print last modified date / time
  if (LAST_MODIFIED & flags)
  {
    uint16_t writeTime;
    uint16_t writeDate;

    // Load and Print write date
    writeDate = secArr[25];
    writeDate <<= 8;
    writeDate |= secArr[24];
  
    print_Str("     ");
    if ((writeDate & 0x01E0) >> 5 < 10) 
      print_Str("0");

    print_Dec((writeDate & 0x01E0) >> 5);
    print_Str("/");
    if ((writeDate & 0x001F) < 10) 
      print_Str("0");

    print_Dec(writeDate & 0x001F);
    print_Str("/");
    print_Dec(1980 + ((writeDate & 0xFE00) >> 9));

    // Load and Print write time
    writeTime = secArr[23];
    writeTime <<= 8;
    writeTime |= secArr[22];

    print_Str("  ");
    if ((writeTime & 0xF800) >> 11 < 10)
      print_Str("0");

    print_Dec((writeTime & 0xF800) >> 11);
    print_Str(":");      
    if ((writeTime & 0x07E0) >> 5 < 10) 
      print_Str("0");

    print_Dec((writeTime & 0x07E0) >> 5);
    print_Str(":");
    if (2 * (writeTime & 0x001F) < 10) 
      print_Str("0");

    print_Dec(2 * (writeTime & 0x001F));
  }
  
  print_Str("     ");

  // Print file size in bytes
  if (FILE_SIZE & flags)
  {
    uint32_t fileSize;
    uint16_t div = 1;
    
    fileSize = secArr[31];
    fileSize <<= 8;
    fileSize |= secArr[30];
    fileSize <<= 8;
    fileSize |= secArr[29];
    fileSize <<= 8;
    fileSize |= secArr[28];

    // print spaces for formatting printed output
    if (fileSize/div >= 10000000)
      print_Str(" ");
    else if (fileSize/div >= 1000000) 
      print_Str("  ");
    else if (fileSize/div >= 100000) 
      print_Str("   ");
    else if (fileSize/div >= 10000) 
      print_Str("    ");
    else if (fileSize/div >= 1000) 
      print_Str("     ");
    else if (fileSize/div >= 100) 
      print_Str("      ");
    else if (fileSize/div >= 10) 
      print_Str("       "); 
    else
      print_Str("        ");
    
    print_Dec(fileSize / div);
    print_Str("B  ");
  }

  // print entry type
  if (TYPE & flags)
  {
    if (secArr[11] & DIR_ENTRY_ATTR) 
      print_Str(" <DIR>   ");
    else 
      print_Str(" <FILE>  ");
  }
}

/*
 * ----------------------------------------------------------------------------
 *                                                   (PRIVATE) PRINT SHORT NAME
 * 
 * Description : Print the short name (8.3) of a FAT file or directory entry.
 * 
 * Arguments   : secArr     Pointer to an array holding the contents of the FAT
 *                          sector where the short name entry is found.
 *               
 *               flags      Determines if entry TYPE field will be printed.
 * 
 * Returns     : void
 * ----------------------------------------------------------------------------
 */
static void pvt_PrintShortName(const uint8_t *secArr)
{
  char snStr[SN_NAME_STR_LEN] = {'\0'};
  char extStr[SN_EXT_STR_LEN + 1] = {'\0'}; // adding 1 for '.' position

  // If directory entry then assume no extension  
  if (secArr[ATTR_BYTE_OFFSET] & DIR_ENTRY_ATTR)
  {
    // load short name chars in secArr into sn string array
    for (uint8_t bytePos = 0; bytePos < 8; bytePos++)                                                     
      snStr[bytePos] = secArr[bytePos];

    // print directory short name
    print_Str(snStr);

    // print spaces for formatting                         
    print_Str("    ");                      
  }

  // file entries. must handle extension.
  else 
  {
    // load name portion of short name into the snStr
    for (uint8_t bytePos = 0; bytePos < 8; bytePos++)
    {
      snStr[bytePos] = secArr[bytePos];
      if (snStr[bytePos] == ' ') 
      { 
        snStr[bytePos] = '\0'; 
        break; 
      }
    }
    // print short name string
    print_Str(snStr);

    // load extension from sector array. Extension chars begin at byte 7
    strcpy(extStr, ".   ");
    for (uint8_t bytePos = 1; bytePos < SN_EXT_STR_LEN; bytePos++) 
      extStr[bytePos] = secArr[7 + bytePos];

    // if extenstion exists, print it.
    if (strcmp(extStr, ".   ")) 
      print_Str(extStr);
  }
}

/*
 * ----------------------------------------------------------------------------
 *                                                   (PRIVATE) PRINT A FAT FILE
 * 
 * Description : Performs the print file operation. This will output plain text
 *               to the screen.
 * 
 * Arguments   : fileSec     Pointer to array holding the contents of the FAT 
 *                           sector where the file-to-be-printed's short name 
 *                           entry is located.
 *               
 *               bpb         Pointer to a valid instance of a BPB struct.
 * 
 * Returns     : END_OF_FILE (success) or FAILED_READ_SECTOR fat error flag.
 * ----------------------------------------------------------------------------
 */
static uint8_t pvt_PrintFile(uint8_t *fileSec, BPB *bpb)
{
  //get FAT index for file's first cluster
  uint32_t clus = fileSec[21];
  clus <<= 8;
  clus |= fileSec[20];
  clus <<= 8;
  clus |= fileSec[27];
  clus <<= 8;
  clus |= fileSec[26];

  // loop over clusters to read in and print file
  do
  {
    // loop over sectors in the cluster to read in and print file
    for (uint32_t currSecNumInClus = 0; 
         currSecNumInClus < bpb->secPerClus; 
         currSecNumInClus++) 
    {
      // calculate address of the sector on the physical disk to read in
      uint32_t currSecNumPhys = currSecNumInClus + bpb->dataRegionFirstSector
                              + (clus - 2) * bpb->secPerClus;

      // read disk sector in the sector array
      uint8_t secArr[SECTOR_LEN];
      if (FATtoDisk_ReadSingleSector(currSecNumPhys, secArr) 
          == FTD_READ_SECTOR_FAILED)  
        return FAILED_READ_SECTOR;

      for (uint16_t byteNum = 0; byteNum < bpb->bytesPerSec; byteNum++)
      {
        // end of file flag. Set to 1 if eof is detected.
        uint8_t eof = 0;

        // 
        // for output formatting. Currently using terminal that requires "\n\r"
        // to go to the start of the next line. Therefore is '\n' is detected
        // then need to print "\n\r".
        //
        if (secArr[byteNum] == '\n') 
          print_Str ("\n\r");
        
        // else just print the character directly to the screen if not 0. 
        else if (secArr[byteNum] != 0) 
          usart_Transmit(secArr[byteNum]);
       
        // else character is zero likely indicatin the eof.
        else 
        {
          // assume eof and set flag
          eof = 1;
          
          // confirm rest of bytes in the current sector are 0
          for (byteNum++; byteNum < bpb->bytesPerSec; byteNum++)
          {
            // if any byte is not 0 then not at eof so reset eof to 0
            if (secArr[byteNum] != 0) 
            { 
              byteNum--;
              eof = 0;
              break;
            }
          }
         }
        // if eof flag is set here the return. else continue.
        if (eof)
          return END_OF_FILE;
      }
    }
  } 
  while ((clus = pvt_GetNextClusIndex(clus, bpb)) != END_CLUSTER);
  
  return END_OF_FILE;
}

