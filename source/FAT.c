/*
*******************************************************************************
*                              AVR-FAT MODULE
*
* File    : FAT.C
* Version : 0.0.0.2 (Previous version commit - Sat Dec 5 22:26:05 2020)
* Author  : Joshua Fain
* Target  : ATMega1280
* License : MIT LICENSE
* Copyright (c) 2020 Joshua Fain
* 
*
* DESCRIPTION: 
* Defines functions declared in FAT.H for accessing contents of a FAT32 
* formatted volume using an AVR microconstroller. The fuctions defined here 
* only provide READ access to the volume's contents (i.e. print file, print 
* directory), no WRITE access is currently possible.
*******************************************************************************
*/



#include <string.h>
#include <stdlib.h>
#include <avr/io.h>
#include "../includes/fat_bpb.h"
#include "../includes/fat.h"
#include "../includes/prints.h"
#include "../includes/usart.h"
#include "../includes/fattodisk_interface.h"


/*
*******************************************************************************
|                       "PRIVATE" FUNCTION DECLARATIONS
*******************************************************************************
*/

uint8_t pvt_check_valid_name (char *nameStr, FatDir * Dir);
uint8_t pvt_set_directory_to_parent (FatDir *Dir, BPB *bpb);
void pvt_load_long_name (int lnFirstEnt, int lnLastEnt, uint8_t *secArr, 
                         char *lnStr, uint8_t *lnStrIndx);
uint32_t pvt_get_next_cluster_index (uint32_t currClusIndx, BPB *bpb);
void pvt_print_entry_fields (uint8_t *byte, uint16_t entPos, uint8_t entFilt);
void pvt_print_short_name (uint8_t *byte, uint16_t entPos, uint8_t entFilt);
uint8_t pvt_print_fat_file (uint16_t entPos, uint8_t * fileSec, BPB *bpb);
uint8_t pvt_get_next_sector (uint8_t * nextSecArr, uint32_t currSecNumInClus, 
                         uint32_t currSecNumPhys, uint32_t clusIndx, BPB *bpb);



/*
*******************************************************************************
|                           FUNCTION DECLARATIONS
*******************************************************************************
*/



/* 
------------------------------------------------------------------------------
| SET ROOT DIRECTORY
|                                        
| Description : Sets an instance of FatDir to the root direcotry.
|
| Arguments   : *Dir    - Pointer to the instance of FatDir that will be set to
|                         the root directory.
|             : *bpb    - Pointer to a valid instance of a BPB struct.
-------------------------------------------------------------------------------
*/

void
fat_set_directory_to_root(FatDir * Dir, BPB * bpb)
{
  for (uint8_t i = 0; i < LN_STRING_LEN_MAX; i++)
    Dir->longName[i] = '\0';
  for (uint8_t i = 0; i < PATH_STRING_LEN_MAX; i++)
    Dir->longParentPath[i] = '\0';
  for (uint8_t i = 0; i < 9; i++)
    Dir->shortName[i] = '\0';
  for (uint8_t i = 0; i < PATH_STRING_LEN_MAX; i++)
    Dir->shortParentPath[i] = '\0';
  
  Dir->longName[0] = '/';
  Dir->shortName[0] = '/';
  Dir->FATFirstCluster = bpb->rootClus;
}



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
fat_init_entry(FatEntry * ent, BPB * bpb)
{
  for(uint8_t i = 0; i < LN_STRING_LEN_MAX; i++)
    ent->longName[i] = '\0';

  for(uint8_t i = 0; i < 13; i++)
    ent->shortName[i] = '\0';

  for(uint8_t i = 0; i < 32; i++)
    ent->shortNameEntry[i] = 0;

  ent->longNameEntryCount = 0;
  ent->shortNameEntryClusIndex = bpb->rootClus;
  ent->shortNameEntrySecNumInClus = 0;
  ent->entryPos = 0;
  ent->lnFlags = 0;
  ent->snPosCurrSec = 0;
  ent->snPosCurrSec = 0;
}



/*
-------------------------------------------------------------------------------
|                               SET TO NEXT ENTRY 
|                                        
| Description : Updates the currEntry, a FatEntry instance, to the next entry
|               in the current directory (currDir).
|
| Arguments   : *currDir        - Current directory. Pointer to an instance
|                                 of FatDir.
|             : *currEntry      - Current etnry. Pointer to an instance of 
|                                 FatEntry
|             : *bpb            - Pointer to a valid instance of a BPB struct.
| 
| Return      : FAT Error Flag  - If any value other than SUCCESS is returned 
|                                 the function was unable to update the
|                                 FatEntry. To read, pass to fat_print_error().
-------------------------------------------------------------------------------
*/

uint8_t 
fat_next_entry (FatDir * currDir, FatEntry * currEntry, BPB * bpb)
{
  const uint16_t bps  = bpb->bytesPerSec;
  const uint8_t  spc  = bpb->secPerClus;
  const uint32_t drfs = bpb->dataRegionFirstSector;

  uint8_t  currSecArr[bps]; 
  uint8_t  nextSecArr[bps];
  uint8_t  attrByte;       // attribute byte
  uint32_t currSecNumPhys; // physical (disk) sector number
  char     lnStr[LN_STRING_LEN_MAX];

  uint8_t  lnStrIndx = 0;
  uint8_t  err;

  uint32_t clusIndx = currEntry->shortNameEntryClusIndex;
  uint8_t  currSecNumInClus = currEntry->shortNameEntrySecNumInClus;
  uint16_t entryPos = currEntry->entryPos;

  uint8_t  lnMask = currEntry->lnFlags;
  uint16_t snPosCurrSec = currEntry->snPosCurrSec;
  uint16_t snPosNextSec = currEntry->snPosNextSec;

  uint8_t  currSecNumInClusStart = 1;
  uint8_t  entryPosStart = 1;

  uint8_t longNameOrder;

  char sn[13];
  uint8_t ndx = 0;

  do 
    {
      if (currSecNumInClusStart == 0) currSecNumInClus = 0; 
      for (; currSecNumInClus < spc; currSecNumInClus++)
        {
          currSecNumInClusStart = 0;

          // load sector bytes into currSecArr[]
          currSecNumPhys = currSecNumInClus + drfs + ((clusIndx - 2) * spc);
          err = fat_to_disk_read_single_sector (currSecNumPhys, currSecArr);
          if (err == 1) return FAILED_READ_SECTOR;
          
          if (entryPosStart == 0) entryPos = 0; 
          for (; entryPos < bps; entryPos = entryPos + ENTRY_LEN)
            {
              entryPosStart = 0;

              // check if adjust entryPos is needed.
              if (lnMask & LN_EXISTS)
                {
                  if (snPosCurrSec >= SECTOR_LEN - ENTRY_LEN)
                    {
                      if (entryPos != 0) //get next sector
                        break; 
                      else 
                        snPosCurrSec = -ENTRY_LEN;
                    }

                  if (lnMask & (LN_CROSS_SEC | LN_LAST_SEC_ENTRY))
                    {
                      entryPos = snPosNextSec + ENTRY_LEN; 
                      snPosNextSec = 0;
                    }
                  else 
                    {
                      entryPos = snPosCurrSec + ENTRY_LEN;
                      snPosCurrSec = 0;
                    }
                }

              // reset long name flags
              lnMask = 0;

              // If first value of entry is 0, rest of entries are empty
              if (currSecArr[entryPos] == 0) 
                return END_OF_DIRECTORY;

              // Skip and go to next entry if current entry is "deleted"
              if (currSecArr[entryPos] != 0xE5)
                {
                  attrByte = currSecArr[entryPos + 11];
                  
                  if ((attrByte & LN_ATTR_MASK) == LN_ATTR_MASK)
                    {
                      if ( !(currSecArr[entryPos] & LN_LAST_ENTRY)) 
                        return CORRUPT_FAT_ENTRY;

                      for (uint8_t k = 0; k < LN_STRING_LEN_MAX; k++) 
                        lnStr[k] = '\0';
 
                      lnStrIndx = 0;

                      lnMask |= LN_EXISTS;

                      // number of entries required by the long name
                      longNameOrder = LN_ORD_MASK & currSecArr[entryPos];              
                      snPosCurrSec = entryPos + (ENTRY_LEN * longNameOrder);
                      
                      // Set long name flags if short
                      // name is in the next sector.
                      if (snPosCurrSec > bps) 
                        lnMask |= LN_CROSS_SEC;
                      else if (snPosCurrSec == SECTOR_LEN) 
                        lnMask |= LN_LAST_SEC_ENTRY;

                      // if short name is in the next sector
                      if (lnMask & (LN_CROSS_SEC | LN_LAST_SEC_ENTRY))
                        {
                          err = pvt_get_next_sector(
                                  nextSecArr, currSecNumInClus, 
                                  currSecNumPhys, clusIndx, bpb);

                          if (err == FAILED_READ_SECTOR) 
                            return FAILED_READ_SECTOR;

                          snPosNextSec = snPosCurrSec - bps;
                          attrByte = nextSecArr[snPosNextSec + 11];

                          // Verify snPosNextSec does not point to long name
                          if ((attrByte & LN_ATTR_MASK) == LN_ATTR_MASK)
                            return CORRUPT_FAT_ENTRY;
                          
                          // Here if long name crosses sector boundary
                          if (lnMask & LN_CROSS_SEC)
                            {
                              // Entry preceeding short name must
                              // be first entry of the long name.
                              if ((nextSecArr[snPosNextSec - ENTRY_LEN] 
                                   & LN_ORD_MASK) != 1) 
                                return CORRUPT_FAT_ENTRY;         

                              // load long name entryPos into lnStr[]
                              pvt_load_long_name(snPosNextSec - ENTRY_LEN,
                                                 0, nextSecArr, 
                                                 lnStr, &lnStrIndx);
                              pvt_load_long_name(SECTOR_LEN - ENTRY_LEN, 
                                                 entryPos, currSecArr, 
                                                 lnStr, &lnStrIndx);

                              for (uint8_t i = 0; i < LN_STRING_LEN_MAX; i++)
                                {
                                  currEntry->longName[i] = lnStr[i];
                                  if (lnStr == '\0') 
                                    break;
                                }

                              currEntry->entryPos = entryPos;
                              currEntry->shortNameEntrySecNumInClus 
                                            = currSecNumInClus;
                              currEntry->shortNameEntryClusIndex = clusIndx;
                              currEntry->snPosCurrSec = snPosCurrSec;
                              currEntry->snPosNextSec = snPosNextSec;
                              currEntry->lnFlags = lnMask;
                              currEntry->longNameEntryCount 
                                            = currSecArr[entryPos] & 0x3F;

                              for (uint8_t k = 0; k < 32; k++)
                                {
                                  currEntry->shortNameEntry[k] 
                                                = nextSecArr[snPosNextSec + k];
                                }

                              for (uint8_t k = 0; k < 13; k++)
                                sn[k] = '\0';
                              
                              ndx = 0;
                              for (uint8_t k = 0; k < 8; k++)
                                {
                                  if (nextSecArr[snPosNextSec + k] != ' ')
                                    { 
                                      sn[ndx] = nextSecArr[snPosNextSec + k];
                                      ndx++;
                                    }
                                }
                              if (nextSecArr[snPosNextSec + 8] != ' ')
                                {
                                  sn[ndx] = '.';
                                  ndx++;
                                  for (uint8_t k = 8; k < 11; k++)
                                    {
                                      if (nextSecArr[snPosNextSec + k] != ' ')
                                        { 
                                          sn[ndx] 
                                            = nextSecArr[snPosNextSec + k];
                                          ndx++;
                                        }
                                    }
                                }

                              strcpy(currEntry->shortName, sn);

                              return SUCCESS;
                            }

                          else if (lnMask & LN_LAST_SEC_ENTRY)
                            {
                              
                              // Entry immediately preceeding short name 
                              // must be the long names's first entry.
                              if ((currSecArr[SECTOR_LEN - ENTRY_LEN] 
                                   & LN_ORD_MASK) != 1) 
                                return CORRUPT_FAT_ENTRY;

                              // load long name entryPos into lnStr[]
                              pvt_load_long_name(SECTOR_LEN - ENTRY_LEN, 
                                                 entryPos, currSecArr, 
                                                 lnStr, &lnStrIndx);
                              
                              for (uint8_t i = 0; i < LN_STRING_LEN_MAX; i++)
                                {
                                  currEntry->longName[i] = lnStr[i];
                                  if (lnStr == '\0') break;
                                }
                              
                              currEntry->entryPos = entryPos;
                              currEntry->shortNameEntrySecNumInClus 
                                          = currSecNumInClus;
                              currEntry->shortNameEntryClusIndex = clusIndx;
                              currEntry->snPosCurrSec = snPosCurrSec;
                              currEntry->snPosNextSec = snPosNextSec;
                              currEntry->lnFlags = lnMask;
                              currEntry->longNameEntryCount 
                                          = currSecArr[entryPos] & 0x3F;

                              for (uint8_t k = 0; k < 32; k++)
                                {
                                  currEntry->shortNameEntry[k] 
                                              = nextSecArr[snPosNextSec + k];
                                }
                              for (uint8_t k = 0; k < 13; k++)
                                sn[k] = '\0';

                              ndx = 0;
                              for (uint8_t k = 0; k < 8; k++)
                                {
                                  if (nextSecArr[snPosNextSec + k] != ' ')
                                    { 
                                      sn[ndx] = nextSecArr[snPosNextSec + k];
                                      ndx++;
                                    }
                                }
                              if (nextSecArr[snPosNextSec + 8] != ' ')
                                {
                                  sn[ndx] = '.';
                                  ndx++;
                                  for (uint8_t k = 8; k < 11; k++)
                                    {
                                      if (nextSecArr[snPosNextSec + k] != ' ')
                                        { 
                                          sn[ndx] 
                                            = nextSecArr[snPosNextSec + k];
                                          ndx++;
                                        }
                                    }
                                }

                              strcpy(currEntry->shortName, sn);

                              return SUCCESS;
                            }
                          else return CORRUPT_FAT_ENTRY;
                        }

                      // Long name exists and is entirely in the
                      // current sector along with the short name
                      else
                        {   
                          attrByte = currSecArr[snPosCurrSec + 11];
                          
                          // Verify snPosNextSec does not point to long name
                          if ((attrByte & LN_ATTR_MASK) == LN_ATTR_MASK)
                            return CORRUPT_FAT_ENTRY;
                 
                          // Confirm entry preceding short name 
                          // is first entryPos of a long name.
                          if ((currSecArr[snPosCurrSec - ENTRY_LEN] 
                               & LN_ORD_MASK) != 1)
                            return CORRUPT_FAT_ENTRY;
                          
                          // load long name entry into lnStr[]
                          pvt_load_long_name(snPosCurrSec - ENTRY_LEN, 
                                             entryPos, currSecArr, 
                                             lnStr, &lnStrIndx);

                          for (uint8_t i = 0; i < LN_STRING_LEN_MAX; i++)
                            {
                              currEntry->longName[i] = lnStr[i];
                              if (lnStr == '\0') break;
                            }

                          currEntry->entryPos = entryPos;
                          currEntry->shortNameEntrySecNumInClus 
                                      = currSecNumInClus;
                          currEntry->shortNameEntryClusIndex = clusIndx;
                          currEntry->snPosCurrSec = snPosCurrSec;
                          currEntry->snPosNextSec = snPosNextSec;
                          currEntry->lnFlags = lnMask;
                          currEntry->longNameEntryCount
                                      = currSecArr[entryPos] & 0x3F;

                          for (uint8_t k = 0; k < 32; k++)
                            {
                              currEntry->shortNameEntry[k] 
                                          = currSecArr[snPosCurrSec + k];
                            }
                          
                          ndx = 0;
                          for (uint8_t k = 0; k < 8; k++)
                            {
                              if (currSecArr[snPosCurrSec + k] != ' ')
                                { 
                                  sn[ndx] = currSecArr[snPosCurrSec + k];
                                  ndx++;
                                }
                            }
                          if (currSecArr[snPosCurrSec + 8] != ' ')
                            {
                              sn[ndx] = '.';
                              ndx++;
                              for (uint8_t k = 8; k < 11; k++)
                                {
                                  if (currSecArr[snPosCurrSec + k] != ' ')
                                    { 
                                      sn[ndx] = currSecArr[snPosCurrSec + k];
                                      ndx++;
                                    }
                                }
                            }

                          strcpy(currEntry->shortName, sn);

                          return SUCCESS;                                                             
                        }                   
                    }

                  // Long name entry does not exist, use short name instead.
                  else
                    {
                      snPosCurrSec = entryPos;

                      attrByte = currSecArr[snPosCurrSec + 11];
                      
                      // must adjust entryPos here by 32.
                      currEntry->entryPos = snPosCurrSec + ENTRY_LEN; 
                      currEntry->shortNameEntrySecNumInClus = currSecNumInClus;
                      currEntry->shortNameEntryClusIndex = clusIndx;
                      currEntry->snPosCurrSec = snPosCurrSec;
                      currEntry->snPosNextSec = snPosNextSec;
                      currEntry->lnFlags = lnMask;
                      currEntry->longNameEntryCount = 0;
                      for (uint8_t k = 0; k < 32; k++)
                        {
                          currEntry->shortNameEntry[k] 
                                      = currSecArr[snPosCurrSec + k];
                        }

                      for (uint8_t k = 0; k < 13; k++)
                        sn[k] = '\0';

                      uint8_t j = 0;
                      for (uint8_t k = 0; k < 8; k++)
                        {
                          if (currSecArr[snPosCurrSec + k] != ' ')
                            { 
                              sn[j] = currSecArr[snPosCurrSec + k];
                              j++;
                            }
                        }
                      if (currSecArr[snPosCurrSec + 8] != ' ')
                        {
                          sn[j] = '.';
                          j++;
                          for (uint8_t k = 8; k < 11; k++)
                            {
                              if (currSecArr[snPosCurrSec + k] != ' ')
                                { 
                                  sn[j] = currSecArr[snPosCurrSec + k];
                                  j++;
                                }
                            }
                        }
                      strcpy(currEntry->shortName, sn);
                      strcpy(currEntry->longName, sn);
                      return SUCCESS;  
                    }
                }
            }
        }
    }
  while ((clusIndx 
            = pvt_get_next_cluster_index(clusIndx, bpb)) != END_CLUSTER);

  return END_OF_DIRECTORY;
}



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
|                                 FatEntry. To read, pass to fat_print_error().
|  
| Notes       : - Can only set the directory to a child or parent of the  
|                 current FatDir directory when the function is called.
|               - newDirStr must be a directory name. Will not work with paths.  
|               - newDirStr is case-sensitive.
-------------------------------------------------------------------------------
*/

uint8_t 
fat_set_directory (FatDir * Dir, char * newDirStr, BPB * bpb)
{
  if (pvt_check_valid_name (newDirStr, Dir)) 
    return INVALID_DIR_NAME;

  // newDirStr == 'Current Directory' ?
  if (!strcmp (newDirStr,  ".")) 
    return SUCCESS;
  
  // newDirStr == 'Parent Directory' ?
  if (!strcmp (newDirStr, ".."))
  {
    // returns either FAILED_READ_SECTOR or SUCCESS
    return pvt_set_directory_to_parent (Dir, bpb);
  }

  FatEntry * ent = malloc(sizeof * ent);
  fat_init_entry(ent, bpb);
  ent->shortNameEntryClusIndex = Dir->FATFirstCluster;

  uint8_t err = 0;
  do 
    {
      err = fat_next_entry(Dir, ent, bpb);
      if (err != SUCCESS) return err;
      
      if (!strcmp(ent->longName, newDirStr) 
           && (ent->shortNameEntry[11] & DIR_ENTRY_ATTR))
        {                                                        
          Dir->FATFirstCluster = ent->shortNameEntry[21];
          Dir->FATFirstCluster <<= 8;
          Dir->FATFirstCluster |= ent->shortNameEntry[20];
          Dir->FATFirstCluster <<= 8;
          Dir->FATFirstCluster |= ent->shortNameEntry[27];
          Dir->FATFirstCluster <<= 8;
          Dir->FATFirstCluster |= ent->shortNameEntry[26];
          
          uint8_t snLen;
          if (strlen(newDirStr) < 8) snLen = strlen(newDirStr);
          else snLen = 8; 

          char sn[9];                                    
          for (uint8_t k = 0; k < snLen; k++)  
            sn[k] = ent->shortNameEntry[k];
          sn[snLen] = '\0';

          strcat (Dir->longParentPath,  Dir->longName );
          strcat (Dir->shortParentPath, Dir->shortName);

          // if current directory is not root then append '/'
          if (Dir->longName[0] != '/') 
            strcat(Dir->longParentPath, "/"); 
          strcpy(Dir->longName, newDirStr);
          
          if (Dir->shortName[0] != '/') 
            strcat(Dir->shortParentPath, "/");
          strcpy(Dir->shortName, sn);
          
          return SUCCESS;
        }
    }
  while (err != END_OF_DIRECTORY);

  return END_OF_DIRECTORY;
}



/*
-------------------------------------------------------------------------------
|                  PRINT ENTRIES IN A DIRECTORY TO A SCREEN
|
| 
| Description : Prints the entries (files / dirs) in the FatDir directory. 
|  
| Arguments   : *Dir         - Pointer to a FatDir struct. This directory's
|                              entries will be printed to the screen.
|             : entFilt    - Byte of Entry Filter Flags. This specifies which
|                              entries and fields will be printed.
|             : *bpb         - Pointer to a valid instance of a BPB struct.
| 
| Return      : FAT Error Flag   - Returns END_OF_DIRECTORY if the function 
|                                  completes without issue. Any other value
|                                  indicates an error. To read, pass the value
|                                  to fat_print_error().
| 
| Notes       : If neither LONG_NAME or SHORT_NAME are passed to entFilt
|               then no entries will be printed to the screen.   
-------------------------------------------------------------------------------
*/

uint8_t 
fat_print_directory (FatDir * dir, uint8_t entFilt, BPB * bpb)
{
  // Prints column headers according to entFilt
  print_str("\n\n\r");
  if (CREATION & entFilt) print_str(" CREATION DATE & TIME,");
  if (LAST_ACCESS & entFilt) print_str(" LAST ACCESS DATE,");
  if (LAST_MODIFIED & entFilt) print_str(" LAST MODIFIED DATE & TIME,");
  if (FILE_SIZE & entFilt) print_str(" SIZE,");
  if (TYPE & entFilt) print_str(" TYPE,");
  
  print_str(" NAME");
  print_str("\n\r");

  FatEntry * ent = malloc(sizeof * ent);
  fat_init_entry(ent, bpb);
  ent->shortNameEntryClusIndex = dir->FATFirstCluster;

  while ( fat_next_entry(dir, ent, bpb) != END_OF_DIRECTORY)
    { 
      if ((  !(ent->shortNameEntry[11] & HIDDEN_ATTR)) 
           ||((ent->shortNameEntry[11] & HIDDEN_ATTR) 
           && (entFilt & HIDDEN)))
        {      
          if ((entFilt & SHORT_NAME) == SHORT_NAME)
            {
              pvt_print_entry_fields (ent->shortNameEntry, 0, entFilt);
              pvt_print_short_name(ent->shortNameEntry, 0, entFilt);
            }

          if ((entFilt & LONG_NAME) == LONG_NAME)
            {
              pvt_print_entry_fields (ent->shortNameEntry, 0, entFilt);
              if (ent->shortNameEntry[11] & DIR_ENTRY_ATTR) 
                { 
                  if ((entFilt & TYPE) == TYPE) 
                    print_str (" <DIR>   ");
                }
              else 
                { 
                  if ((entFilt & TYPE) == TYPE) 
                    print_str (" <FILE>  ");
                }
              print_str(ent->longName);
            }
        }        
    }
  return END_OF_DIRECTORY;
}


/*
-------------------------------------------------------------------------------
|                                               PRINT FILE TO SCREEN
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
|                                  to fat_print_error().
|                               
| Notes       : fileNameStr must be a long name unless a long name for a given
|               entry does not exist, in which case it must be a short name.
-------------------------------------------------------------------------------
*/

uint8_t 
fat_print_file(FatDir * Dir, char * fileNameStr, BPB * bpb)
{
  if (pvt_check_valid_name (fileNameStr, Dir)) 
    return INVALID_DIR_NAME;

  FatEntry * ent = malloc(sizeof * ent);
  fat_init_entry(ent, bpb);
  ent->shortNameEntryClusIndex = Dir->FATFirstCluster;

  uint8_t err = 0;
  do 
    {
      err = fat_next_entry(Dir, ent, bpb);
      if (err != SUCCESS) return err;

      if (!strcmp(ent->longName, fileNameStr)
          && !(ent->shortNameEntry[11] & DIR_ENTRY_ATTR))
        {                                                        
          print_str("\n\n\r");
          return pvt_print_fat_file (0, ent->shortNameEntry, bpb);
        }
    }
  while (err != END_OF_DIRECTORY);

  return END_OF_DIRECTORY;
}



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
fat_print_error (uint8_t err)
{  
  switch(err)
  {
    case SUCCESS: 
      print_str("\n\rSUCCESS");
      break;
    case END_OF_DIRECTORY:
      print_str("\n\rEND_OF_DIRECTORY");
      break;
    case INVALID_FILE_NAME:
      print_str("\n\rINVALID_FILE_NAME");
      break;
    case FILE_NOT_FOUND:
      print_str("\n\rFILE_NOT_FOUND");
      break;
    case INVALID_DIR_NAME:
      print_str("\n\rINVALID_DIR_NAME");
      break;
    case DIR_NOT_FOUND:
      print_str("\n\rDIR_NOT_FOUND");
      break;
    case CORRUPT_FAT_ENTRY:
      print_str("\n\rCORRUPT_FAT_ENTRY");
      break;
    case END_OF_FILE:
      print_str("\n\rEND_OF_FILE");
      break;
    case FAILED_READ_SECTOR:
      print_str("\n\rFAILED_READ_SECTOR");
      break;
    default:
      print_str("\n\rUNKNOWN_ERROR");
      break;
  }
}



/*
*******************************************************************************
 *                      "PRIVATE" FUNCTION DEFINITIONS
*******************************************************************************
*/



/*
-------------------------------------------------------------------------------
|                       (PRIVATE) CHECK FOR LEGAL NAME
| 
| Description : Checks if a string is a valid FAT entry name (short or long). 
|
| Argument    : *nameStr - string to be verified as a legal FAT name.
| Return      : 0 if name is LEGAL, 1 if name is ILLEGAL.
-------------------------------------------------------------------------------
*/

uint8_t
pvt_check_valid_name (char *nameStr, FatDir *Dir)
{
  // check that long name and path size are 
  // not too large for current settings.
  if (strlen (nameStr) > LN_STRING_LEN_MAX) return 1;
  if (( strlen (nameStr) + strlen (Dir->longParentPath)) > PATH_STRING_LEN_MAX) return 1;
  
  // nameStr is illegal if it is an empty string or begins with a space character 
  if ((strcmp (nameStr, "") == 0) || (nameStr[0] == ' ')) return 1;

  // nameStr is illegal if it contains an illegal character
  char illegalCharacters[] = {'\\','/',':','*','?','"','<','>','|'};
  for (uint8_t k = 0; k < strlen (nameStr); k++)
    {       
      for (uint8_t j = 0; j < 9; j++)
        {
          if (nameStr[k] == illegalCharacters[j]) 
            return 1;
        }
    }

  // nameStr is illegal if it is all space characters.
  for (uint8_t k = 0; k < strlen (nameStr); k++)  
    {
      if (nameStr[k] != ' ') 
        return 0; // name is legal
    }  
  return 1; // illegal name
}



/*
-------------------------------------------------------------------------------
|               (PRIVATE) SET CURRENT DIRECTORY TO ITS PARENT
| 
| Description : Sets a FatDir instance to its parent directory. 
|
| Argument    : *Dir  - Ptr to FatDir instance. The members of this instance
|                       will be set to its parent directory.
|             : *bpb  - Ptr to a valid instance of a BPB struct.
|
| Return      : SUCCESS or FAILED_READ_SECTOR.
-------------------------------------------------------------------------------
*/

uint8_t
pvt_set_directory_to_parent (FatDir * Dir, BPB * bpb)
{
  uint32_t parentDirFirstClus;
  uint32_t currSecNumPhys;
  uint8_t  currSecArr[bpb->bytesPerSec];
  uint8_t  err;

  currSecNumPhys = bpb->dataRegionFirstSector + ((Dir->FATFirstCluster - 2) * bpb->secPerClus);

  // function returns either 0 for success for 1 for failed.
  err = fat_to_disk_read_single_sector (currSecNumPhys, currSecArr);
  if (err != 0) return FAILED_READ_SECTOR;

  parentDirFirstClus = currSecArr[53];
  parentDirFirstClus <<= 8;
  parentDirFirstClus |= currSecArr[52];
  parentDirFirstClus <<= 8;
  parentDirFirstClus |= currSecArr[59];
  parentDirFirstClus <<= 8;
  parentDirFirstClus |= currSecArr[58];

  // If Dir is pointing to the root directory, do nothing.
  if (Dir->FATFirstCluster == bpb->rootClus); 

  // If parent of Dir is root directory
  else if (parentDirFirstClus == 0)
    {
      strcpy (Dir->shortName, "/");
      strcpy (Dir->shortParentPath, "");
      strcpy (Dir->longName, "/");
      strcpy (Dir->longParentPath, "");
      Dir->FATFirstCluster = bpb->rootClus;
    }

  // parent directory is not root directory
  else
    {          
      char tmpShortNamePath[PATH_STRING_LEN_MAX];
      char tmpLongNamePath[PATH_STRING_LEN_MAX];

      strlcpy (tmpShortNamePath, Dir->shortParentPath, strlen (Dir->shortParentPath));
      strlcpy (tmpLongNamePath, Dir->longParentPath,   strlen (Dir->longParentPath ));
      
      char *shortNameLastDirInPath = strrchr (tmpShortNamePath, '/');
      char *longNameLastDirInPath  = strrchr (tmpLongNamePath , '/');
      
      strcpy (Dir->shortName, shortNameLastDirInPath + 1);
      strcpy (Dir->longName , longNameLastDirInPath  + 1);

      strlcpy (Dir->shortParentPath, tmpShortNamePath, (shortNameLastDirInPath + 2) - tmpShortNamePath);
      strlcpy (Dir->longParentPath,  tmpLongNamePath,  (longNameLastDirInPath  + 2) -  tmpLongNamePath);

      Dir->FATFirstCluster = parentDirFirstClus;
    }
    return SUCCESS;
}






/*
***********************************************************************************************************************
 *                                  (PRIVATE) LOAD A LONG NAME ENTRY INTO A C-STRING
 * 
 * Description : This function is called by a FAT function that must read in a long name from a FAT directory into a
 *               C-string. The function is called twice if a long name crosses a sector boundary, and *lnStrIndx will
 *               point to the position in the string to begin loading the next characters
 * 
 * Arguments   : lnFirstEnt          - Integer that points to the position in *secArr that is the lowest order entry
 *                                     of the long name in the sector array.
 *             : lnLastEnt           - Integer that points to the position in *secArr that is the highest order
 *                                     entry of the long name in the sector array.
 *             : *secArr             - Pointer to an array that holds the physical sector's contents containing the 
 *                                     the entries of the long name that will be loaded into the char arry *lnStr.
 *             : *lnStr              - Pointer to a null terminated char array (C-string) that will be loaded with the
 *                                     long name characters from the sector. Usually starts as an array of NULLs.
 *             : *lnStrIndx          - Pointer to an integer that specifies the position in *lnStr where the first
 *                                     character will be loaded when this function is called. This function will update
 *                                     this value as the characters are loaded. If a subsequent call to this function 
 *                                     is made, as required when a single long name crosses a sector boundary, then 
 *                                     this value will be its final value from the previous call.
***********************************************************************************************************************
*/

/*
-------------------------------------------------------------------------------
|             (PRIVATE) LOAD A LONG NAME ENTRY INTO A STRING 
| 
| Description  : Loads the characters of a long name into a string char array.  
|
| Arguments   : lnFirstEnt   - int specifying position of the lowest order
|                              entry in *secArr of the long name.
|             : lnLastEnt    - int specifying position of the highest order
|                              entry in *secArr of the long name.
|             : *secArr      - ptr to an array holding the contents of a single
|                              sector of a directory from a FAT-formatted disk.
|             : *lnStr       - ptr to a string array that will be loaded with
|                              the long name chars.
|             : *lnStrIndx   - ptr to an int specifying the position in *lnStr
|                              to begin loading chars when this function is 
|                              called. Typically 0, unless the long name 
|                              crosses a sector boundary which requires calling
|                              this function twice to finish loading *lnStr.
|
| Notes       : Must called twice if long name crosses sector boundary.
-------------------------------------------------------------------------------
*/

void
pvt_load_long_name (int lnFirstEnt, int lnLastEnt, uint8_t * secArr, 
                    char *lnStr, uint8_t *lnStrIndx)
{
  for (int i = lnFirstEnt; i >= lnLastEnt; i = i - ENTRY_LEN)
    {                                              
      for (uint16_t n = i + 1; n < i + 11; n++)
        {                                  
          if ((secArr[n] == 0) || (secArr[n] > 126));
          else 
            { 
              lnStr[*lnStrIndx] = secArr[n];
              (*lnStrIndx)++;  
            }
        }

      for (uint16_t n = i + 14; n < i + 26; n++)
        {                                  
          if ((secArr[n] == 0) || (secArr[n] > 126));
          else 
            { 
              lnStr[*lnStrIndx] = secArr[n];
              (*lnStrIndx)++;  
            }
        }
      
      for (uint16_t n = i + 28; n < i + 32; n++)
        {                                  
          if ((secArr[n] == 0) || (secArr[n] > 126));
          else 
            { 
              lnStr[*lnStrIndx] = secArr[n];  
              (*lnStrIndx)++;  
            }
        }        
    }
}



/*
***********************************************************************************************************************
 *                                   (PRIVATE) GET THE FAT INDEX OF THE NEXT CLUSTER 
 *
 * Description : Used by the FAT functions to get the location of the next cluster in a directory or file. The value  
 *               returned is an integer that specifies to the cluster's index in the FAT. This value is offset by two 
 *               when counting the clusters in a FATs Data Region. Therefore, to get the cluster number in the data 
 *               region the value returned by this function must be subtracted by 2.
 * 
 * Arguments   : currClusIndx      - A cluster's FAT index. The value at this index in the FAT is the index of the
 *                                   the next cluster of the file or directory.
 *             : *bpb              - Pointer to a valid instance of a BPB struct.
 * 
 * Returns     : The FAT index of the next cluster of a file or Dir. This is the value at the currClusIndx's location  
 *               in the FAT. If a value of 0x0FFFFFFF is returned then the current cluster is the last cluster of the 
 *               file or directory.
***********************************************************************************************************************
*/

uint32_t 
pvt_get_next_cluster_index (uint32_t currClusIndx, BPB * bpb)
{
  uint8_t  bytesPerClusIndx = 4; // for FAT32
  uint16_t numOfIndexedClustersPerSecOfFat = bpb->bytesPerSec / bytesPerClusIndx; // = 128

  uint32_t clusIndx = currClusIndx / numOfIndexedClustersPerSecOfFat;
  uint32_t clusIndxStartByte = 4 * (currClusIndx % numOfIndexedClustersPerSecOfFat);
  uint32_t cluster = 0;

  uint32_t fatSectorToRead = clusIndx + bpb->rsvdSecCnt;

  uint8_t sectorArr[bpb->bytesPerSec];
  
  fat_to_disk_read_single_sector (fatSectorToRead, sectorArr);

  cluster = sectorArr[clusIndxStartByte+3];
  cluster <<= 8;
  cluster |= sectorArr[clusIndxStartByte+2];
  cluster <<= 8;
  cluster |= sectorArr[clusIndxStartByte+1];
  cluster <<= 8;
  cluster |= sectorArr[clusIndxStartByte];

  return cluster;
}



/*
***********************************************************************************************************************
 *                                        (PRIVATE) PRINTS THE FIELDS OF FAT ENTRY
 *
 * Description : Used by fat_print_directory() to print the fields associated with an entry (e.g. creation/last 
 *               modified date/time, file size, etc...). Which fields are printed is determined by the entFilt 
 *               flag set.
 * 
 * Arguments   : *secArr        - Pointer to an array that holds the short name of the entry that is being printed to
 *                                the screen. Only the short name entry of a short name/long name combination holds
 *                                the values of these fields.
 *             : entPos         - Integer specifying the location in *secArr of the first byte of the short name 
 *                                entry whose fields should be printed to the screen.
 *             : entFilt      - Byte specifying the entryFlag settings. This is used to determined which fields of 
 *                                the entry will be printed.
***********************************************************************************************************************
*/

void 
pvt_print_entry_fields (uint8_t *secArr, uint16_t entPos, uint8_t entFilt)
{
  uint16_t creationTime;
  uint16_t creationDate;
  uint16_t lastAccessDate;
  uint16_t writeTime;
  uint16_t writeDate;
  uint32_t fileSize;

  // Load fields with values from secArr

  if (CREATION & entFilt)
    {
      creationTime = secArr[entPos + 15];
      creationTime <<= 8;
      creationTime |= secArr[entPos + 14];
      
      creationDate = secArr[entPos + 17];
      creationDate <<= 8;
      creationDate |= secArr[entPos + 16];
    }

  if (LAST_ACCESS & entFilt)
    {
      lastAccessDate = secArr[entPos + 19];
      lastAccessDate <<= 8;
      lastAccessDate |= secArr[entPos + 18];
    }

  if (LAST_MODIFIED & entFilt)
    {
      writeTime = secArr[entPos + 23];
      writeTime <<= 8;
      writeTime |= secArr[entPos + 22];

      writeDate = secArr[entPos + 25];
      writeDate <<= 8;
      writeDate |= secArr[entPos + 24];
    }

  fileSize = secArr[entPos + 31];
  fileSize <<= 8;
  fileSize |= secArr[entPos + 30];
  fileSize <<= 8;
  fileSize |= secArr[entPos + 29];
  fileSize <<= 8;
  fileSize |= secArr[entPos + 28];

  print_str ("\n\r");

  // Print fields 

  if (CREATION & entFilt)
    {
      print_str ("    ");
      if (((creationDate & 0x01E0) >> 5) < 10) 
        {
          print_str ("0");
        }
      print_dec ((creationDate & 0x01E0) >> 5);
      print_str ("/");
      if ((creationDate & 0x001F) < 10)
        {
          print_str ("0");
        }
      print_dec (creationDate & 0x001F);
      print_str ("/");
      print_dec (1980 + ((creationDate & 0xFE00) >> 9));

      print_str ("  ");
      if (((creationTime & 0xF800) >> 11) < 10) 
        {
          print_str ("0");
        }
      print_dec (((creationTime & 0xF800) >> 11));
      print_str (":");
      if (((creationTime & 0x07E0) >> 5) < 10)
        {
          print_str ("0");
        }
      print_dec ((creationTime & 0x07E0) >> 5);
      print_str (":");
      if ((2 * (creationTime & 0x001F)) < 10) 
        {
          print_str ("0");
        }
      print_dec (2 * (creationTime & 0x001F));
    }

  if (LAST_ACCESS & entFilt)
    {
      print_str ("     ");
      if (((lastAccessDate & 0x01E0) >> 5) < 10)
        {
          print_str ("0");
        }
      print_dec ((lastAccessDate & 0x01E0) >> 5);
      print_str ("/");
      if ((lastAccessDate & 0x001F) < 10) 
        {
          print_str("0");
        }
      print_dec (lastAccessDate & 0x001F);
      print_str ("/");
      print_dec (1980 + ((lastAccessDate & 0xFE00) >> 9));
    }


  if (LAST_MODIFIED & entFilt)
    {
      print_str ("     ");
      if (((writeDate & 0x01E0) >> 5) < 10) 
        {
          print_str ("0");
        }
      print_dec ((writeDate & 0x01E0) >> 5);
      print_str ("/");
      if ((writeDate & 0x001F) < 10) 
        {
          print_str ("0");
        }
      print_dec (writeDate & 0x001F);
      print_str ("/");
      print_dec (1980 + ((writeDate & 0xFE00) >> 9));

      print_str ("  ");

      if (((writeTime & 0xF800) >> 11) < 10)
       {
         print_str ("0");
       }
      print_dec (((writeTime & 0xF800) >> 11));
      print_str (":");
      
      if (((writeTime & 0x07E0) >> 5) < 10) 
        {
          print_str ("0");
        }
      print_dec ((writeTime & 0x07E0) >> 5);
      print_str (":");
      if ((2 * (writeTime & 0x001F)) < 10) 
        {
          print_str ("0");
        }
      print_dec (2 * (writeTime & 0x001F));
    }

  uint16_t div = 1000;
  print_str ("     ");
  if ((entFilt & FILE_SIZE) == FILE_SIZE)
  {
         if ((fileSize / div) >= 10000000) { print_str(" "); print_dec(fileSize / div); }
    else if ((fileSize / div) >= 1000000) { print_str("  "); print_dec(fileSize / div); }
    else if ((fileSize / div) >= 100000) { print_str("   "); print_dec(fileSize / div); }
    else if ((fileSize / div) >= 10000) { print_str("    "); print_dec(fileSize / div); }
    else if ((fileSize / div) >= 1000) { print_str("     "); print_dec(fileSize / div); }
    else if ((fileSize / div) >= 100) { print_str("      "); print_dec(fileSize / div); }
    else if ((fileSize / div) >= 10) { print_str("       "); print_dec(fileSize / div); }
    else                             { print_str("        ");print_dec(fileSize / div);}        
  
  print_str("kB  ");
  }

}



/*
***********************************************************************************************************************
 *                                              (PRIVATE) PRINT SHORT NAME
 *
 * Description : Used by fat_print_directory to print the short name of a FAT file or directory.
 * 
 * Arguments   : *secArr     - Pointer to an array that holds the short name of the entry that is to be printed to
 *                                the screen.
 *             : entPos       - Location in *secArr of the first byte of the short name entry that is to be
 *                                printed to the screen.
 *             : entFilt    - Byte specifying the entryFlag settings. This is used to determined here if the TYPE 
 *                                flag has been passed
***********************************************************************************************************************
*/

void 
pvt_print_short_name (uint8_t *secArr, uint16_t entPos, uint8_t entFilt)
{
  char sn[9];
  char ext[5];

  for (uint8_t k = 0; k < 8; k++) sn[k] = ' ';
  sn[8] = '\0';

  uint8_t attr = secArr[entPos + 11];
  if (attr & 0x10)
    {
      if ((entFilt & TYPE) == TYPE) print_str (" <DIR>   ");
      for (uint8_t k = 0; k < 8; k++) sn[k] = secArr[entPos + k];
      print_str (sn);
      print_str ("    ");
    }
  else 
    {
      if ((entFilt & TYPE) == TYPE) print_str (" <FILE>  ");
      // initialize extension char array
      strcpy (ext, ".   ");
      for (uint8_t k = 1; k < 4; k++) ext[k] = secArr[entPos + 7 + k];
      for (uint8_t k = 0; k < 8; k++) 
        {
          sn[k] = secArr[k + entPos];
          if (sn[k] == ' ') 
            { 
              sn[k] = '\0'; 
              break; 
            };
        }

      print_str (sn);
      if (strcmp (ext, ".   ")) print_str (ext);
      for (uint8_t p = 0; p < 10 - (strlen (sn) + 2); p++) print_str (" ");
    }
}



/*
***********************************************************************************************************************
 *                                              (PRIVATE) PRINT A FAT FILE
 *
 * Description : Used by fat_print_file() to perform that actual print operation.
 * 
 * Arguments   : entPos         - Integer that points to the location in *fileSec of the first byte of the short 
 *                                name entPos of the file whose contents will be printed to the screen. This is 
 *                                required as the first cluster index of the file is located in the short name.
 *             : *fileSec       - pointer to an array that holds the short name entPos of the file to be printed to 
 *                                the screen.
 *             : *bpb           - Pointer to a valid instance of a BPB struct.
***********************************************************************************************************************
*/

uint8_t 
pvt_print_fat_file (uint16_t entPos, uint8_t *fileSec, BPB * bpb)
  {
    uint32_t currSecNumPhys;
    uint32_t cluster;
    uint8_t  err;
    uint8_t  eof = 0; // end of file flag

    //get FAT index for file's first cluster
    cluster =  fileSec[entPos + 21];
    cluster <<= 8;
    cluster |= fileSec[entPos + 20];
    cluster <<= 8;
    cluster |= fileSec[entPos + 27];
    cluster <<= 8;
    cluster |= fileSec[entPos + 26];

    // read in contents of file starting at relative sector 0 in 'cluster' and print contents to the screen.
    do
      {
        for (uint32_t currSecNumInClus = 0; currSecNumInClus < bpb->secPerClus; currSecNumInClus++) 
          {
            if (eof == 1) break; // end-of-file reached.
            currSecNumPhys = currSecNumInClus + bpb->dataRegionFirstSector + ((cluster - 2) * bpb->secPerClus);

            // function returns either 0 for success for 1 for failed.
            err = fat_to_disk_read_single_sector (currSecNumPhys, fileSec);
            if (err == 1) return FAILED_READ_SECTOR;

            for (uint16_t k = 0; k < bpb->bytesPerSec; k++)
              {
                // for formatting how this shows up on the screen.
                if (fileSec[k] == '\n') print_str ("\n\r");
                else if (fileSec[k] != 0) usart_transmit (fileSec[k]);

                // checks if end of file by checking to see if remaining bytes in sector are zeros.
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
    while (((cluster = pvt_get_next_cluster_index(cluster,bpb)) != END_CLUSTER));
    return END_OF_FILE;
  }



/*
***********************************************************************************************************************
 *                             (PRIVATE) LOAD THE CONTENTS OF THE NEXT SECTOR INTO AN ARRAY
 *
 * Description : Used by the FAT functions to load the contents of the next file or directory sector into the 
 *               *nextSecArr array if it is found that a long/short name combo crosses the sector boundary.
 * 
 * Arguments   : *nextSecArr             - Pointer to an array that will be loaded with the contents of the next sector
 *                                         of a file or directory.
 *             : currSecNumInClus        - Integer that specifies the current sector number relative to the current 
 *                                         cluster. This value is used to determine if the next sector is in the 
 *                                         current or the next cluster.
 *             : currSecNumPhys          - Integer specifying the current sector's physical sector number on the disk 
 *                                         hosting the FAT volume.
 *             : clusIndx                - Integer specifying the current cluster's FAT index. This is required if it
 *                                         is determined that the next sector is in the next cluster.
 *             : *bpb                    - Pointer to a valid instance of a BPB struct.
***********************************************************************************************************************
*/

uint8_t
pvt_get_next_sector (uint8_t * nextSecArr, uint32_t currSecNumInClus, 
                     uint32_t currSecNumPhys, uint32_t clusIndx, BPB * bpb)
{
  uint32_t nextSecNumPhys;
  uint8_t  err = 0;
  
  if (currSecNumInClus >= (bpb->secPerClus - 1)) 
    nextSecNumPhys = bpb->dataRegionFirstSector + ((pvt_get_next_cluster_index (clusIndx, bpb) - 2) * bpb->secPerClus);
  else 
    nextSecNumPhys = 1 + currSecNumPhys;

  // function returns either 0 for success or 1 for failed.
  err = fat_to_disk_read_single_sector (nextSecNumPhys, nextSecArr);
  if (err == 1) 
    return FAILED_READ_SECTOR;
  else 
    return SUCCESS;
}






/*
***********************************************************************************************************************
 *                           (PRIVATE) SET DIRECTORY TO A CHILD DIRECTORY
 * 
 * Description : This function is called by fat_set_directory() if that function has been asked to set an instance of
 *               FatDir to a child directory and a valid matching entry was found in the directory currently pointed at
 *               by the FatDir instance. This function will only update the struct's members to that of the matching 
 *               entry. It does not perform any of the search/compare required to find the matching entry.
 * 
 * Argument    : *Dir            - Pointer to an instance of a FatDir whose members will be updated to point to the 
 *                                 directory whose name matches *childDirStr.
 *             : *sectorArr      - Pointer to an array that holds the physical sector's contents containing the short
 *                                 name of the entry whose name matches *childDirStr.
 *             : snPos           - Integer that specifies the position of the first byte of the 32-byte short name 
 *                                 entry in *sectorArr.
 *             : *childDirStr    - Pointer to a C-string whose name matches an entry in the current directory FatDir is
 *                                 set to. This is the name of the directory FatDir will be set to by this function. 
 *                                 This is a long name, unless a long name does not exist for a given entry. In that
 *                                 case the longName and shortName members of the FatDir instance will both be set to 
 *                                 the short name of the entry.  
 *             : *bpb            - Pointer to a valid instance of a BPB struct.
***********************************************************************************************************************
*/
/*
void
pvt_set_directory_to_child (FatDir * Dir, uint8_t * sectorArr, uint16_t snPos, char * childDirStr, BPB * bpb)
{
  uint32_t dirFirstClus;
  dirFirstClus = sectorArr[snPos + 21];
  dirFirstClus <<= 8;
  dirFirstClus |= sectorArr[snPos + 20];
  dirFirstClus <<= 8;
  dirFirstClus |= sectorArr[snPos + 27];
  dirFirstClus <<= 8;
  dirFirstClus |= sectorArr[snPos + 26];

  Dir->FATFirstCluster = dirFirstClus;
  
  uint8_t snLen;
  if (strlen(childDirStr) < 8) snLen = strlen(childDirStr);
  else snLen = 8; 

  char sn[9];                                    
  for (uint8_t k = 0; k < snLen; k++)  
    sn[k] = sectorArr[snPos + k];
  sn[snLen] = '\0';

  strcat (Dir->longParentPath,  Dir->longName );
  strcat (Dir->shortParentPath, Dir->shortName);

  // if current directory is not root then append '/'
  if (Dir->longName[0] != '/') 
    strcat(Dir->longParentPath, "/"); 
  strcpy(Dir->longName, childDirStr);
  
  if (Dir->shortName[0] != '/') 
    strcat(Dir->shortParentPath, "/");
  strcpy(Dir->shortName, sn);
}
*/


/*
***********************************************************************************************************************
 *                                       (PRIVATE) SET THE LONG NAME FLAGS
 *
 * Description : Used by the FAT functions to set the long name flags if it was determined that a long name exists for 
 *               the current entry begin checked/read-in. This also sets the variable snPosCurrSec.
 * 
 * Arguments   : *lnFlags           - Pointer to a byte that will hold the long name flag settings: LN_EXISTS,
 *                                    LN_CROSS_SEC, and LN_LAST_SEC_ENTRY. This function
 *                                    will determine which flags should be set and then set this byte accordingly.
 *             : entryPos           - Integer that specifies the location in the current sector that will be checked 
 *                                    during the subsequent execution.
 *             : *snPosCurrSec      - This value is set by this function. It is an integer pointer that specifies the 
 *                                    position of the short name relative to the position of the first byte of the 
 *                                    current sector. NOTE: If this is greater than 511 then the short name is in the
 *                                    next sector.
 *             : *snPosNextSec      - Integer pointer. This value is set by this function if it is determined that the
 *                                    short name entry is in the next sector compared to the sector where the last 
 *                                    entry of its corresponding long name resides.
 *             : *bpb               - Pointer to a valid instance of a BPB struct.
***********************************************************************************************************************
*/
/*
void
pvt_set_long_name_flags (uint8_t * lnFlags, uint16_t entryPos, uint16_t * snPosCurrSec,
                         uint8_t * currSecArr, BPB * bpb)
{
  *lnFlags |= LN_EXISTS;

  // number of entries required by the long name
  uint8_t longNameOrder = LN_ORD_MASK & currSecArr[entryPos];                 
  *snPosCurrSec = entryPos + (ENTRY_LEN * longNameOrder);
  
  // If short name position is greater than 511 then the short name is in the next sector.
  if ((*snPosCurrSec) >= bpb->bytesPerSec)
    {
      if ((*snPosCurrSec) > bpb->bytesPerSec) *lnFlags |= LN_CROSS_SEC;
      else if (*snPosCurrSec == SECTOR_LEN)   *lnFlags |= LN_LAST_SEC_ENTRY;
    }
}
*/


/*
***********************************************************************************************************************
 *                                    (PRIVATE) CORRECTION TO ENTRY POSITON POINTER
 *
 * Description : Used by the FAT functions when searching within a directory. Often a correction to entryPos needs to 
 *               be made to ensure it is pointing at the correct location in the sector array and this function will 
 *               ensure this. If this function determines that the actual correct location of the entry is not in the 
 *               current sector being searched when this function is called, then this function will return a 1 to
 *               signal the calling function to break and get the next sector before proceeding. This function will
 *               then be called again to ensure that entryPos is indicating the correct location in the new sector.
 * 
 * Arguments   : lnFlags            - Byte that is holding the setting of the three long name flags: LN_EXISTS,
 *                                    LN_CROSS_SEC, and LN_LAST_SEC_ENTRY. The long 
 *                                    name flags should be cleared (set to 0) by the calling function after this 
 *                                    function returns with 0.
 *             : *entryPos          - Pointer to an integer that specifies the location in the current sector that will
 *                                    be checked/read-in by the calling function. This value will be updated by this
 *                                    function if a correction to it is required. 
 *             : *snPosCurrSec      - Pointer to an integer that specifies the final value of this variable after
 *                                    the previous entry was checked. Stands for Short Name Position In Current Sector.
 *             : *snPosNextSec      - Pointer to an integer that specifies the final value of this variable after
 *                                    the previous entry was checked. Stands for Short Name Position In Next Sector.
 * 
 * Returns     : 1 if this function determines correct entry is in the next sector when this function is called.
 *             : 0 if the correct entry is in the current sector when this function is called.
***********************************************************************************************************************
*/
/*
uint8_t 
pvt_correct_entry_check (uint8_t lnFlags, uint16_t * entryPos, uint16_t * snPosCurrSec, uint16_t * snPosNextSec)
{  
  if (lnFlags & LN_EXISTS)
    {
      if ((*snPosCurrSec) >= (SECTOR_LEN - ENTRY_LEN))
        {
          if ((*entryPos) != 0) return 1; // need to get the next sector
          else (*snPosCurrSec) = -ENTRY_LEN;
        }

      if (lnFlags & (LN_CROSS_SEC | LN_LAST_SEC_ENTRY))
        {
          *entryPos = (*snPosNextSec) + ENTRY_LEN; 
          *snPosNextSec = 0;
        }
      else 
        {
          *entryPos = (*snPosCurrSec) + ENTRY_LEN;
          *snPosCurrSec = 0;
        }
    }
  return 0;
}
*/