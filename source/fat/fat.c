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
 *                      "PRIVATE" FUNCTION PROTOTYPES    
 ******************************************************************************
 */

static uint8_t  pvt_CheckName(char *nameStr);
static uint8_t  pvt_SetDirToParent(FatDir *dir, BPB *bpb);
static uint32_t pvt_GetNextClusIndex(uint32_t currClusIndx, BPB *bpb);
static void     pvt_PrintEntFields(uint8_t *byte, uint8_t flags);
static void     pvt_PrintShortName(uint8_t *byte);
static uint8_t  pvt_PrintFile(uint8_t *fileSec, BPB *bpb);
static void     pvt_LoadLongName(int lnFirstEnt, int lnLastEnt, 
                            uint8_t *secArr, char *lnStr, uint8_t *lnStrIndx);
static void     pvt_UpdateFatEntryState(char *lnStr, uint16_t entPos, 
                            uint8_t snEntSecNumInClus, uint32_t snEntClusIndx,
                            uint16_t snPosCurrSec, uint16_t snPosNextSec,
                            uint8_t lnFlags, uint8_t  *secArr, FatEntry *ent);

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
  for (uint8_t i = 0; i < LN_STRING_LEN_MAX; i++)
    dir->lnStr[i] = '\0';
  for (uint8_t i = 0; i < PATH_STRING_LEN_MAX; i++)
    dir->lnPathStr[i] = '\0';
  for (uint8_t i = 0; i < 9; i++)
    dir->snStr[i] = '\0';
  for (uint8_t i = 0; i < PATH_STRING_LEN_MAX; i++)
    dir->snPathStr[i] = '\0';
  
  dir->lnStr[0] = '/';
  dir->snStr[0] = '/';
  dir->fstClusIndx = bpb->rootClus;
}

/*
 * ----------------------------------------------------------------------------
 *                                                         INITIALIZE FAT ENTRY
 *                                      
 * Description : Initializes an entry of FatEntry.
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
  for(uint8_t i = 0; i < LN_STRING_LEN_MAX; i++)
    ent->lnStr[i] = '\0';

  for(uint8_t i = 0; i < 13; i++)
    ent->snStr[i] = '\0';

  for(uint8_t i = 0; i < 32; i++)
    ent->snEnt[i] = 0;

  ent->snEntClusIndx = bpb->rootClus;
  ent->snEntSecNumInClus = 0;
  ent->entPos = 0;
  ent->lnFlags = 0;
  ent->snPosCurrSec = 0;
  ent->snPosCurrSec = 0;
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
  // store required bpb member values in local constant variables.
  const uint16_t bps  = bpb->bytesPerSec;
  const uint8_t  spc  = bpb->secPerClus;
  const uint32_t drfs = bpb->dataRegionFirstSector;

  //
  // capture currEnt state in local vars. These variables will be updated by
  // this function. If the function is successful and is able to find a 'next'
  // entry then the updated values of the variables will be used to update the 
  // currEnt members.
  //
  uint32_t clusIndx = currEnt->snEntClusIndx;
  uint8_t  currSecNumInClus = currEnt->snEntSecNumInClus;
  uint16_t entPos = currEnt->entPos;
  uint8_t  lnFlags = currEnt->lnFlags;
  uint16_t snPosCurrSec = currEnt->snPosCurrSec;
  uint16_t snPosNextSec = currEnt->snPosNextSec;

  // 
  // These flags are set to 1 for the first cluster and sector loop when this 
  // function is called. After which they are set to zero. They are used for 
  // ensuring the loops start at the correct positions/values, and not zero,
  // during the initial iteration. Subsequent iterations during a single
  // function call will then start at 0.
  //
  uint8_t  currSecNumInClusStartFlag = 1;
  uint8_t  entryPosStartFlag = 1;

  // physical (disk) sector numbers
  uint32_t currSecNumPhys; 
  uint32_t nextSecNumPhys;

  // other local variable defs
  uint8_t  err;
  uint8_t  currSecArr[bps];                 // current 512 byte sector
  uint8_t  nextSecArr[bps];                 // next 512 byte sector
  uint8_t  attrByte;                        // entry attribute byte. (byte 11)
  char     lnStr[LN_STRING_LEN_MAX];        // array for long name string
  uint8_t  lnStrIndx = 0;                   // for loading long name into lnStr

  //
  // Loop to search for the next entry. This loop will run until reaching the
  // end of the last cluster of the directory, causing the function to exit
  // with END_OF_DIRECTORY, or until a 'next' entry is found.
  // 
  // The process for finding the next entry is to
  // 1) Load the current sector into the currSecArr[].
  // 2) Locate the starting entry and check if it is a long name.
  // 3.1) If it is not a long name, skip to the bottom and load the currEnt
  //      members with those of this short name entry and return.
  // 3.2) If the entry is a long name, set the LN_EXISTS flag in lnFlags.
  // 4) Read the long name order value in byte 1 of the entry to determine how
  //    many 32-byte entries the long name requires. 
  // 5) Use the length of the long name to determine how the long name and its
  //    short name are distributed among adjacent sectors.
  // 6) If it is determined that the long and short name are within the current
  //    sector then can simply load the currEnt members with their new values.
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
  // 11) If no next entry is found, then the function will exit with 
  //     END_OF_DIRECTORY
  //
  // There are several other error checks and entry position adjustments that
  // must be made, but the above lists the main steps.
  
  // loop through the directory clusters
  do 
  { 
    if (currSecNumInClusStartFlag == 0) 
      currSecNumInClus = 0;

    // loop through the sectors in the cluster
    for (; currSecNumInClus < spc; currSecNumInClus++)
    {
      // only set to 1 on first iteration.
      currSecNumInClusStartFlag = 0;

      // load the current sector into currSecArr[]
      currSecNumPhys = currSecNumInClus + drfs + (clusIndx - 2) * spc;
      err = FATtoDisk_ReadSingleSector (currSecNumPhys, currSecArr);
      if (err == 1) 
        return FAILED_READ_SECTOR;
      
      if (entryPosStartFlag == 0) 
        entPos = 0;

      for (; entPos < bps; entPos = entPos + ENTRY_LEN)
      {
        // only set to 1 on first iteration.
        entryPosStartFlag = 0;

        // adjust entPos if previous entry included a long name
        if (lnFlags & LN_EXISTS)
        {
          // used for possible negative ENTRY_LEN adjustment
          int tempSecPos = snPosCurrSec; 

          // greater than the value of the last entry position in a sector
          if (snPosCurrSec >= SECTOR_LEN - ENTRY_LEN)
          {
            // get next sector
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

        // If first value of entry is 0, rest of entries are empty
        if (currSecArr[entPos] == 0) 
          return END_OF_DIRECTORY;

        // Confirm entry is not "deleted" 
        if (currSecArr[entPos] != 0xE5)
        {
          attrByte = currSecArr[entPos + 11];

          // entPos points to a long name entry
          if ((attrByte & LN_ATTR_MASK) == LN_ATTR_MASK)
          {
            // Here, entPos must point to last entry of long name. 
            if ( !(currSecArr[entPos] & LN_LAST_ENTRY)) 
              return CORRUPT_FAT_ENTRY;

            // reset lnStr as array of nulls.
            for (uint8_t k = 0; k < LN_STRING_LEN_MAX; k++) 
              lnStr[k] = '\0';
            lnStrIndx = 0;

            // locate position of the short name in current sector.
            // Based on number of entries required by the long name. 
            snPosCurrSec = entPos + 
                           ENTRY_LEN * (LN_ORD_MASK & currSecArr[entPos]);
            
            // Set long name flags
            lnFlags |= LN_EXISTS;
            if (snPosCurrSec > bps) 
              lnFlags |= LN_CROSS_SEC;
            else if (snPosCurrSec == SECTOR_LEN) 
              lnFlags |= LN_LAST_SEC_ENTRY;

            // Check if short name is in the next sector
            if (lnFlags & (LN_CROSS_SEC | LN_LAST_SEC_ENTRY))
            {
              // locate and load next sector into nextSecArr 
              if (currSecNumInClus >= spc - 1)
              {
                uint32_t temp;              // using just to make more readable
                temp = pvt_GetNextClusIndex (clusIndx, bpb);
                nextSecNumPhys = drfs + (temp - 2) * spc;
              }
              else 
                nextSecNumPhys = 1 + currSecNumPhys;
              
              err = FATtoDisk_ReadSingleSector (nextSecNumPhys, nextSecArr);
              if (err == 1)
                return FAILED_READ_SECTOR;

              snPosNextSec = snPosCurrSec - bps;
              attrByte = nextSecArr[snPosNextSec + 11];

              // Verify snPosNextSec does not point to long name
              if ((attrByte & LN_ATTR_MASK) == LN_ATTR_MASK)
                return CORRUPT_FAT_ENTRY;
              
              // Long name crosses sector boundary.
              // Short name is in the next sector.
              if (lnFlags & LN_CROSS_SEC)
              {
                // Entry preceeding short name must
                // be first entry of the long name.
                if ((nextSecArr[snPosNextSec - ENTRY_LEN] & LN_ORD_MASK) != 1)
                  return CORRUPT_FAT_ENTRY;         

                // load long name into lnStr[]
                pvt_LoadLongName (snPosNextSec - ENTRY_LEN, 0,
                                  nextSecArr, lnStr, &lnStrIndx);
                pvt_LoadLongName (SECTOR_LEN - ENTRY_LEN, entPos, 
                                  currSecArr, lnStr, &lnStrIndx);

                pvt_UpdateFatEntryState (lnStr, entPos, currSecNumInClus, 
                                         clusIndx, snPosCurrSec, snPosNextSec,
                                         lnFlags, nextSecArr, currEnt);
                return SUCCESS;
              }

              // Long name is in the current sector. 
              // Short name is in the next sector.
              else if (lnFlags & LN_LAST_SEC_ENTRY)
              {
                // Entry preceeding short name must
                // be first entry of the long name.
                if ((currSecArr[SECTOR_LEN - ENTRY_LEN] & LN_ORD_MASK) != 1)
                  return CORRUPT_FAT_ENTRY;

                // load long name into lnStr[]
                pvt_LoadLongName (SECTOR_LEN - ENTRY_LEN, entPos, 
                                  currSecArr, lnStr, &lnStrIndx);
                
                pvt_UpdateFatEntryState (lnStr, entPos, currSecNumInClus, 
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
              attrByte = currSecArr[snPosCurrSec + 11];
              
              // Verify snPosNextSec does not point to long name
              if ((attrByte & LN_ATTR_MASK) == LN_ATTR_MASK)
                return CORRUPT_FAT_ENTRY;
      
              // Same as above
              if ((currSecArr[snPosCurrSec - ENTRY_LEN] & LN_ORD_MASK) != 1)
                return CORRUPT_FAT_ENTRY;
              
              // load long name into lnStr[]
              pvt_LoadLongName (snPosCurrSec - ENTRY_LEN, entPos, currSecArr, 
                                lnStr, &lnStrIndx);

              pvt_UpdateFatEntryState (lnStr, entPos, currSecNumInClus, 
                                       clusIndx, snPosCurrSec, snPosNextSec,
                                       lnFlags, currSecArr, currEnt);
              return SUCCESS;                                                             
            }                   
          }

          // Long name does not exist. Use short name instead.
          else
          {
            attrByte = currSecArr[entPos + 11];
            pvt_UpdateFatEntryState (lnStr, entPos, currSecNumInClus, clusIndx,
                                     entPos, snPosNextSec, lnFlags, currSecArr,
                                     currEnt);
            return SUCCESS;  
          }
        }
      }
    }
  }
  while ((clusIndx = pvt_GetNextClusIndex (clusIndx, bpb)) != END_CLUSTER);

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
  if (pvt_CheckName (newDirStr))
    return INVALID_DIR_NAME;

  // newDirStr == 'Current Directory' ?
  else if (!strcmp (newDirStr, ".")) 
    return SUCCESS;
  
  // newDirStr == 'Parent Directory' ?
  else if (!strcmp (newDirStr, ".."))
  {
    // returns either FAILED_READ_SECTOR or SUCCESS
    return pvt_SetDirToParent (dir, bpb);
  }

  // newDirStr == 'Root Directory' ?
  else if (!strcmp (newDirStr, "~"))
  {
    fat_SetDirToRoot(dir, bpb);
    return SUCCESS;
  }

  // 
  // Create and initialize a FatEntry instance. This will set the snEntClusIndx
  // member to the root directory and all other members to 0 or null strings.
  // Afterwards, update the snEntClusIndx to point to the first cluster index of 
  // the FatDir instance (dir).
  //
  FatEntry * ent = malloc(sizeof(FatEntry));
  fat_InitEntry (ent, bpb);
  ent->snEntClusIndx = dir->fstClusIndx;

  // 
  // Search FatDir directory to see if a child directory matches newDirStr.
  // This is done by repeatedly calling fat_SetNextEntry() to set a FatEntry
  // instance to the next entry in the directory and then comparing the 
  // lnStr member of the instance to newDirStr. Note that the lnStr
  // member will be the same as the snStr if a lnStr does not exist
  // for the entry. Therefore, shortNames can only be used when a lnStr
  // does not exist for the entry. 
  //
  uint8_t err;
  do 
  {
    err = fat_SetNextEntry(dir, ent, bpb);
    if (err != SUCCESS) 
      return err;
    
    // check if long name of next entry matches the desired new directory.
    if (!strcmp(ent->lnStr, newDirStr) && (ent->snEnt[11] & DIR_ENTRY_ATTR))
    {                                                        
      // If match, set FatDir instance members to those of the matching dir.

      dir->fstClusIndx = ent->snEnt[21];
      dir->fstClusIndx <<= 8;
      dir->fstClusIndx |= ent->snEnt[20];
      dir->fstClusIndx <<= 8;
      dir->fstClusIndx |= ent->snEnt[27];
      dir->fstClusIndx <<= 8;
      dir->fstClusIndx |= ent->snEnt[26];
      
      uint8_t snLen;
      if (strlen(newDirStr) < 8) 
        snLen = strlen(newDirStr);
      else 
        snLen = 8; 

      char sn[9];                                    
      for (uint8_t k = 0; k < snLen; k++)  
        sn[k] = ent->snEnt[k];
      sn[snLen] = '\0';

      // append current directory name to the path
      strcat (dir->lnPathStr,  dir->lnStr );
      strcat (dir->snPathStr, dir->snStr);

      // update directory name to new directory name. 
      // if current directory is not root then append '/'
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
  // member to the root directory and all other members to 0 or null strings.
  // Afterwards, immediately update the snEntClusIndx to point to the first 
  // cluster index of the FatDir instance (dir).
  //
  FatEntry * ent = malloc(sizeof(FatEntry));
  fat_InitEntry (ent, bpb);
  ent->snEntClusIndx = dir->fstClusIndx;

  // 
  // Call fat_SetNextEntry() to set FatEntry to the next entry in the current
  // directory, dir, then print the entry and fields according to the entFilt
  // setting. Repeat until reaching END_OF_DIRECTORY is returned.
  //
  while (fat_SetNextEntry (dir, ent, bpb) != END_OF_DIRECTORY)
  { 
    // only print hidden entries if the HIDDEN filter flag is been set.
    if (!(ent->snEnt[11] & HIDDEN_ATTR) || 
        (ent->snEnt[11] & HIDDEN_ATTR && entFilt & HIDDEN))
    {      
      // Print short names if the SHORT_NAME filter flag is set.
      if ((entFilt & SHORT_NAME) == SHORT_NAME)
      {
        pvt_PrintEntFields (ent->snEnt, entFilt);
        pvt_PrintShortName (ent->snEnt);
      }

      // Print long names if the LONG_NAME filter flag is set.
      if ((entFilt & LONG_NAME) == LONG_NAME)
      {
        pvt_PrintEntFields (ent->snEnt, entFilt);
        print_Str (ent->lnStr);
      }
    }   
  }
  return END_OF_DIRECTORY;
}

/*
 * ----------------------------------------------------------------------------
 *                                                         PRINT FILE TO SCREEN
 *                                       
 * Description : Prints the contents of a file to the screen.
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
  if (pvt_CheckName (fileNameStr))
    return INVALID_FILE_NAME;

  // 
  // Create and initialize a FatEntry instance. This will set the snEntClusIndx
  // member to the root directory and all other members to 0 or null strings.
  // Afterwards, immediately update the snEntClusIndx to point to the first 
  // cluster index of the FatDir instance (dir).
  //
  FatEntry * ent = malloc(sizeof(FatEntry));
  fat_InitEntry(ent, bpb);
  ent->snEntClusIndx = dir->fstClusIndx;

  // 
  // Search for the file (fileNameStr) in the current directory (dir). Do this
  // by calling fat_SetNextEntry() to set the FatEntry instance to the next
  // entry in the directory. This is repeated until the a file name is returned
  // that matches fileNameStr. Once a match is found then the "private" 
  // function, pvt_PrintFile() is called to print this file. If no matching 
  // file is found, then the loop will exit once END_OF_DIRECTORY is returned.
  //
  uint8_t err = 0;
  do 
  {
    // get the next entry
    err = fat_SetNextEntry (dir, ent, bpb);
    if (err != SUCCESS) 
      return err;
    
    // check if name of next entry matches fileNameStr
    if ( !strcmp (ent->lnStr, fileNameStr)
         && !(ent->snEnt[11] & DIR_ENTRY_ATTR))
    {                                                        
      // matching file found. Now printing its contents.
      print_Str("\n\n\r");
      return pvt_PrintFile (ent->snEnt, bpb);
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
      break;
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
  char     sn[13];
  uint8_t  Indx = 0;

  uint16_t snPos;
  if (lnFlags & (LN_CROSS_SEC | LN_LAST_SEC_ENTRY))
    snPos = snPosNextSec;
  else
    snPos = snPosCurrSec;

  if (lnFlags & LN_EXISTS) 
    ent->entPos = entPos;
  else 
    ent->entPos = entPos + ENTRY_LEN;

  ent->snEntSecNumInClus = snEntSecNumInClus;
  ent->snEntClusIndx = snEntClusIndx;
  ent->snPosCurrSec = snPosCurrSec;
  ent->snPosNextSec = snPosNextSec;
  ent->lnFlags = lnFlags;

  for (uint8_t k = 0; k < 32; k++)
    ent->snEnt[k] = secArr[snPos + k];

  for (uint8_t k = 0; k < 13; k++)
    sn[k] = '\0';
  
  Indx = 0;
  for (uint8_t k = 0; k < 8; k++)
  {
    if (secArr[snPos + k] != ' ')
    { 
      sn[Indx] = secArr[snPos + k];
      Indx++;
    }
  }
  if (secArr[snPos + 8] != ' ')
  {
    sn[Indx] = '.';
    Indx++;
    for (uint8_t k = 8; k < 11; k++)
    {
      if (secArr[snPos + k] != ' ')
      { 
        sn[Indx] = secArr[snPos + k];
        Indx++;
      }
    }
  }

  strcpy(ent->snStr, sn);
  
  if ( !(lnFlags & LN_EXISTS))
    strcpy(ent->lnStr, sn);
  else
  {
    for (uint8_t i = 0; i < LN_STRING_LEN_MAX; i++)
    {
      ent->lnStr[i] = lnStr[i];
      if (*lnStr == '\0') 
        break;
    }
  }      
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
 * Returns     : 0 if LEGAL.
 *               1 if ILLEGAL.
 * -----------------------------------------------------------------------------
 */
static uint8_t pvt_CheckName(char *nameStr)
{
  // check that long name is not too large for current settings.
  if (strlen (nameStr) > LN_STRING_LEN_MAX) 
    return 1;
  
  // illegal if empty string or begins with a space char
  if ((strcmp (nameStr, "") == 0) || (nameStr[0] == ' ')) 
    return 1;

  // illegal if contains an illegal char
  char illegalCharacters[] = {'\\','/',':','*','?','"','<','>','|'};
  for (uint8_t k = 0; k < strlen (nameStr); k++)
  {       
    for (uint8_t j = 0; j < 9; j++)
    {
      if (nameStr[k] == illegalCharacters[j])
        return 1;
    }
  }

  // illegal if all space characters.
  for (uint8_t k = 0; k < strlen (nameStr); k++)  
  {
    if (nameStr[k] != ' ')
      // LEGAL
      return 0;                                  
  }
  // ILLEGAL
  return 1;                                      
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

  currSecNumPhys = bpb->dataRegionFirstSector 
                   + (dir->fstClusIndx - 2) * bpb->secPerClus;

  // function returns either 0 for success for 1 for failed.
  err = FATtoDisk_ReadSingleSector (currSecNumPhys, currSecArr);
  if (err != 0) 
   return FAILED_READ_SECTOR;

  parentDirFirstClus = currSecArr[53];
  parentDirFirstClus <<= 8;
  parentDirFirstClus |= currSecArr[52];
  parentDirFirstClus <<= 8;
  parentDirFirstClus |= currSecArr[59];
  parentDirFirstClus <<= 8;
  parentDirFirstClus |= currSecArr[58];

  // dir is already the root directory, do nothing.
  if (dir->fstClusIndx == bpb->rootClus); 

  // parent of dir is root directory ?
  else if (parentDirFirstClus == 0)
  {
    strcpy (dir->snStr, "/");
    strcpy (dir->snPathStr, "");
    strcpy (dir->lnStr, "/");
    strcpy (dir->lnPathStr, "");
    dir->fstClusIndx = bpb->rootClus;
  }

  // parent directory is not root directory
  else
  {          
    char tmpShortNamePath[PATH_STRING_LEN_MAX];
    char tmpLongNamePath[PATH_STRING_LEN_MAX];

    strlcpy (tmpShortNamePath, dir->snPathStr, 
              strlen (dir->snPathStr));
    strlcpy (tmpLongNamePath, dir->lnPathStr,
              strlen (dir->lnPathStr));
    
    char *shortNameLastDirInPath = strrchr (tmpShortNamePath, '/');
    char *longNameLastDirInPath  = strrchr (tmpLongNamePath , '/');
    
    strcpy (dir->snStr, shortNameLastDirInPath + 1);
    strcpy (dir->lnStr , longNameLastDirInPath  + 1);

    strlcpy (dir->snPathStr, tmpShortNamePath, 
            (shortNameLastDirInPath + 2) - tmpShortNamePath);
    strlcpy (dir->lnPathStr,  tmpLongNamePath,  
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
  for (int i = lnFirstEnt; i >= lnLastEnt; i = i - ENTRY_LEN)
  {                                              
    for (uint16_t n = i + 1; n < i + 11; n++)
    {                                  
      if (secArr[n] == 0 || secArr[n] > 126);
      else 
      { 
        lnStr[*lnStrIndx] = secArr[n];
        (*lnStrIndx)++;  
      }
    }

    for (uint16_t n = i + 14; n < i + 26; n++)
    {                                  
      if (secArr[n] == 0 || secArr[n] > 126);
      else 
      { 
        lnStr[*lnStrIndx] = secArr[n];
        (*lnStrIndx)++;
      }
    }
    
    for (uint16_t n = i + 28; n < i + 32; n++)
    {                                  
      if (secArr[n] == 0 || secArr[n] > 126);
      else 
      { 
        lnStr[*lnStrIndx] = secArr[n];  
        (*lnStrIndx)++;  
      }
    }        
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
  
  FATtoDisk_ReadSingleSector (fatSectorToRead, sectorArr);

  clusIndx = 0;
  clusIndx = sectorArr[clusIndxStartByte+3];
  clusIndx <<= 8;
  clusIndx |= sectorArr[clusIndxStartByte+2];
  clusIndx <<= 8;
  clusIndx |= sectorArr[clusIndxStartByte+1];
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
    print_Str ("    ");

    if (((createDate & 0x01E0) >> 5) < 10) 
      print_Str ("0");

    print_Dec ((createDate & 0x01E0) >> 5);
    print_Str ("/");
    if ((createDate & 0x001F) < 10)
      print_Str ("0");
    
    print_Dec (createDate & 0x001F);
    print_Str ("/");
    print_Dec (1980 + ((createDate & 0xFE00) >> 9));
    print_Str ("  ");
    if (((createTime & 0xF800) >> 11) < 10) 
      print_Str ("0");
    
    print_Dec (((createTime & 0xF800) >> 11));
    print_Str (":");
    if (((createTime & 0x07E0) >> 5) < 10)
      print_Str ("0");
    
    print_Dec ((createTime & 0x07E0) >> 5);
    print_Str (":");
    if ((2 * (createTime & 0x001F)) < 10) 
      print_Str ("0");

    print_Dec (2 * (createTime & 0x001F));
  }

  // Print last access date
  if (LAST_ACCESS & flags)
  {
    uint16_t lastAccDate;

    lastAccDate = secArr[19];
    lastAccDate <<= 8;
    lastAccDate |= secArr[18];

    print_Str ("     ");
    if (((lastAccDate & 0x01E0) >> 5) < 10)
      print_Str ("0");

    print_Dec ((lastAccDate & 0x01E0) >> 5);
    print_Str ("/");
    if ((lastAccDate & 0x001F) < 10) 
      print_Str("0");

    print_Dec (lastAccDate & 0x001F);
    print_Str ("/");
    print_Dec (1980 + ((lastAccDate & 0xFE00) >> 9));
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
  
    print_Str ("     ");
    if (((writeDate & 0x01E0) >> 5) < 10) 
      print_Str ("0");

    print_Dec ((writeDate & 0x01E0) >> 5);
    print_Str ("/");
    if ((writeDate & 0x001F) < 10) 
      print_Str ("0");

    print_Dec (writeDate & 0x001F);
    print_Str ("/");
    print_Dec (1980 + ((writeDate & 0xFE00) >> 9));

    // Load and Print write time
    writeTime = secArr[23];
    writeTime <<= 8;
    writeTime |= secArr[22];

    print_Str ("  ");
    if (((writeTime & 0xF800) >> 11) < 10)
      print_Str ("0");

    print_Dec (((writeTime & 0xF800) >> 11));
    print_Str (":");      
    if (((writeTime & 0x07E0) >> 5) < 10) 
      print_Str ("0");

    print_Dec ((writeTime & 0x07E0) >> 5);
    print_Str (":");
    if ((2 * (writeTime & 0x001F)) < 10) 
      print_Str ("0");

    print_Dec (2 * (writeTime & 0x001F));
  }
  
  print_Str ("     ");

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
      print_Str (" ");
    else if (fileSize/div >= 1000000) 
      print_Str ("  ");
    else if (fileSize/div >= 100000) 
      print_Str ("   ");
    else if (fileSize/div >= 10000) 
      print_Str ("    ");
    else if (fileSize/div >= 1000) 
      print_Str ("     ");
    else if (fileSize/div >= 100) 
      print_Str ("      ");
    else if (fileSize/div >= 10) 
      print_Str ("       "); 
    else
      print_Str ("        ");
    
    print_Dec (fileSize/div);
    print_Str ("B  ");
  }

  // print entry type
  if (TYPE & flags)
  {
    if (secArr[11] & DIR_ENTRY_ATTR) 
      print_Str (" <DIR>   ");
    else 
      print_Str (" <FILE>  ");
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
static void pvt_PrintShortName(uint8_t *secArr)
{
  char sn[9];
  char ext[5];
  uint8_t attr = secArr[11];

  // initialize sn to all spaces string
  for (uint8_t k = 0; k < 8; k++) 
    sn[k] = ' ';
  sn[8] = '\0';

  // no extension if dir entry  
  if (attr & DIR_ENTRY_ATTR)
  {
    for (uint8_t k = 0; k < 8; k++) 
      sn[k] = secArr[k];
    
    print_Str (sn);
    print_Str ("    ");
  }

  // file entries
  else 
  {
    strcpy (ext, ".   ");
    for (uint8_t k = 1; k < 4; k++) 
      ext[k] = secArr[7 + k];
    for (uint8_t k = 0; k < 8; k++) 
    {
      sn[k] = secArr[k];
      if (sn[k] == ' ') 
      { 
        sn[k] = '\0'; 
        break; 
      }
    }
    print_Str (sn);
    if (strcmp (ext, ".   ")) 
      print_Str (ext);
    for (uint8_t p = 0; p < 10 - (strlen (sn) + 2); p++) 
      print_Str (" ");
  }
}

/*
 * ----------------------------------------------------------------------------
 *                                                   (PRIVATE) PRINT A FAT FILE
 * 
 * Description : performs the print file operation.
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
  uint32_t currSecNumPhys;
  uint32_t clus;
  uint8_t  err;
  uint8_t  eof = 0; // end of file flag

  //get FAT index for file's first cluster
  clus =  fileSec[21];
  clus <<= 8;
  clus |= fileSec[20];
  clus <<= 8;
  clus |= fileSec[27];
  clus <<= 8;
  clus |= fileSec[26];

  // read in and print file contents to the screen.
  do
  {
    uint32_t currSecNumInClus = 0;
    for (; currSecNumInClus < bpb->secPerClus; currSecNumInClus++) 
    {
      if (eof == 1) 
        break;
      currSecNumPhys = currSecNumInClus + bpb->dataRegionFirstSector
                        + (clus - 2) * bpb->secPerClus;

      // function returns either 0 for success for 1 for failed.
      err = FATtoDisk_ReadSingleSector (currSecNumPhys, fileSec);
      if (err == 1) 
        return FAILED_READ_SECTOR;

      for (uint16_t k = 0; k < bpb->bytesPerSec; k++)
      {
        if (fileSec[k] == '\n') 
          print_Str ("\n\r");
        else if (fileSec[k] != 0) 
          usart_Transmit (fileSec[k]);
        // check if end of file.
        else 
        {
          eof = 1;
          for (uint16_t i = k+1; i < bpb->bytesPerSec; i++)
          {
            if (fileSec[i] != 0) 
            { 
              eof = 0;
              break;
            }
          }
        }
      }
    }
  } 
  while ((clus = pvt_GetNextClusIndex (clus,bpb)) != END_CLUSTER);
  
  return END_OF_FILE;
}

