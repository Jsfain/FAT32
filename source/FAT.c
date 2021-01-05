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
void
pvt_update_fat_entry_state (char *lnStr, uint16_t entPos, uint8_t snEntSecNumInClus, uint32_t snEntClusIndx,
                            uint16_t snPosCurrSec, uint16_t snPosNextSec, uint8_t lnMask, uint8_t *secArr,
                            FatEntry *ent);



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
    ent->snEnt[i] = 0;

  ent->snEntClusIndx = bpb->rootClus;
  ent->snEntSecNumInClus = 0;
  ent->entPos = 0;
  ent->lnFlags = 0;
  ent->snPosCurrSec = 0;
  ent->snPosCurrSec = 0;
}



/*
-------------------------------------------------------------------------------
|                               SET TO NEXT ENTRY 
|                                        
| Description : Updates the currEnt, a FatEntry instance, to the next entry
|               in the current directory (currDir).
|
| Arguments   : *currDir        - Current directory. Pointer to an instance
|                                 of FatDir.
|             : *currEnt      - Current etnry. Pointer to an instance of 
|                                 FatEntry
|             : *bpb            - Pointer to a valid instance of a BPB struct.
| 
| Return      : FAT Error Flag  - If any value other than SUCCESS is returned 
|                                 the function was unable to update the
|                                 FatEntry. To read, pass to fat_print_error().
-------------------------------------------------------------------------------
*/

uint8_t 
fat_next_entry (FatDir * currDir, FatEntry * currEnt, BPB * bpb)
{
  const uint16_t bps  = bpb->bytesPerSec;
  const uint8_t  spc  = bpb->secPerClus;
  const uint32_t drfs = bpb->dataRegionFirstSector;

  // capture currEnt state in local vars. These will be updated. If successful,
  // the new (next) state will be stored back in the FatEntry members.
  uint32_t clusIndx = currEnt->snEntClusIndx;
  uint8_t  currSecNumInClus = currEnt->snEntSecNumInClus;
  uint16_t entPos = currEnt->entPos;
  uint8_t  lnMask = currEnt->lnFlags;
  uint16_t snPosCurrSec = currEnt->snPosCurrSec;
  uint16_t snPosNextSec = currEnt->snPosNextSec;

  // flags to signal corresponding vars should not be set to zero on the
  // first run of the loops where, they are used, when the function is called.
  uint8_t  currSecNumInClusStart = 1;
  uint8_t  entryPosStart = 1;

  // physical (disk) sector numbers
  uint32_t currSecNumPhys; 
  uint32_t nextSecNumPhys;

  // other local variable defs
  uint8_t  currSecArr[bps]; 
  uint8_t  nextSecArr[bps];
  uint8_t  attrByte; 
  char     lnStr[LN_STRING_LEN_MAX];
  uint8_t  lnStrIndx = 0;
  uint8_t  err;


  // Loop to search for the next entry. Runs until reaching the end of the last 
  // cluster of the directory. Returns to calling function if error or the 
  // next entry in the directory was found.
  do 
    {
      if (currSecNumInClusStart == 0) currSecNumInClus = 0; 
      for (; currSecNumInClus < spc; currSecNumInClus++)
        {
          currSecNumInClusStart = 0;

          // load sector into currSecArr[]
          currSecNumPhys = currSecNumInClus + drfs + ((clusIndx - 2) * spc);
          err = fat_to_disk_read_single_sector (currSecNumPhys, currSecArr);
          if (err == 1) return FAILED_READ_SECTOR;
          
          if (entryPosStart == 0) entPos = 0;
          for (; entPos < bps; entPos = entPos + ENTRY_LEN)
            {
              entryPosStart = 0;

              // adjust entPos if needed, based on long name flags.
              if (lnMask & LN_EXISTS)
                {
                  if (snPosCurrSec >= SECTOR_LEN - ENTRY_LEN)
                    {
                      if (entPos != 0) //get next sector
                        break; 
                      else 
                        snPosCurrSec = -ENTRY_LEN;
                    }

                  if (lnMask & (LN_CROSS_SEC | LN_LAST_SEC_ENTRY))
                    {
                      entPos = snPosNextSec + ENTRY_LEN; 
                      snPosNextSec = 0;
                    }
                  else 
                    {
                      entPos = snPosCurrSec + ENTRY_LEN;
                      snPosCurrSec = 0;
                    }
                }

              // reset long name flags
              lnMask = 0;

              // If first value of entry is 0, rest of entries are empty
              if (currSecArr[entPos] == 0) 
                return END_OF_DIRECTORY;

              // print_str("\n\r entPos = "); print_dec(entPos);
              // Skip and go to next entry if current entry is "deleted"
               if (currSecArr[entPos] != 0xE5)
                {
                  attrByte = currSecArr[entPos + 11];
                  
                  // entry pos points to a long name entry.
                  if ((attrByte & LN_ATTR_MASK) == LN_ATTR_MASK)
                    {
                      // here, entry position must point to the last entry of
                      // a long name. If it doesn't then something is wrong. 
                      if ( !(currSecArr[entPos] & LN_LAST_ENTRY)) 
                        return CORRUPT_FAT_ENTRY;

                      // reset lnStr as array of nulls.
                      for (uint8_t k = 0; k < LN_STRING_LEN_MAX; k++) 
                        lnStr[k] = '\0';
 
                      // Reset. Used for loading long name chars into lnStr.
                      lnStrIndx = 0;

                      // set long name exists flag.
                      lnMask |= LN_EXISTS;

                      // locate position of the short name in current sector.
                      // Based on number of entries required by the long name. 
                      snPosCurrSec = entPos
                        + (ENTRY_LEN * (LN_ORD_MASK & currSecArr[entPos]));
                      
                      // Set long name flags if short name is in next sector.
                      if (snPosCurrSec > bps) 
                        lnMask |= LN_CROSS_SEC;
                      else if (snPosCurrSec == SECTOR_LEN) 
                        lnMask |= LN_LAST_SEC_ENTRY;

                      // short name is in the next sector?
                      if (lnMask & (LN_CROSS_SEC | LN_LAST_SEC_ENTRY))
                        {
                          // locate and load next sector into nextSecArr 
                          if (currSecNumInClus >= (spc - 1))
                            {
                              nextSecNumPhys = drfs + ((
                                pvt_get_next_cluster_index (clusIndx, bpb) - 2
                                ) * spc);
                            }
                          else 
                            nextSecNumPhys = 1 + currSecNumPhys;
                          
                          err = fat_to_disk_read_single_sector (nextSecNumPhys,
                                                                nextSecArr);
                          if (err == 1)
                            return FAILED_READ_SECTOR;

                          snPosNextSec = snPosCurrSec - bps;
                          attrByte = nextSecArr[snPosNextSec + 11];

                          // Verify snPosNextSec does not point to long name
                          if ((attrByte & LN_ATTR_MASK) == LN_ATTR_MASK)
                            return CORRUPT_FAT_ENTRY;
                          
                          // Long name crosses sector boundary?
                          if (lnMask & LN_CROSS_SEC)
                            {
                              // Entry preceeding short name must
                              // be first entry of the long name.
                              if ((nextSecArr[snPosNextSec - ENTRY_LEN] 
                                   & LN_ORD_MASK) != 1) 
                                return CORRUPT_FAT_ENTRY;         

                              // load long name entPos into lnStr[]
                              pvt_load_long_name(snPosNextSec - ENTRY_LEN,
                                                 0, nextSecArr, 
                                                 lnStr, &lnStrIndx);
                              pvt_load_long_name(SECTOR_LEN - ENTRY_LEN, 
                                                 entPos, currSecArr, 
                                                 lnStr, &lnStrIndx);

                              
                              pvt_update_fat_entry_state ( lnStr, entPos,
                                currSecNumInClus, clusIndx, snPosCurrSec, 
                                snPosNextSec, lnMask, nextSecArr, currEnt);
                              
                              return SUCCESS;
                            }

                          else if (lnMask & LN_LAST_SEC_ENTRY)
                            {
                              
                              // Entry immediately preceeding short name 
                              // must be the long names's first entry.
                              if ((currSecArr[SECTOR_LEN - ENTRY_LEN] 
                                   & LN_ORD_MASK) != 1) 
                                return CORRUPT_FAT_ENTRY;

                              // load long name entPos into lnStr[]
                              pvt_load_long_name(SECTOR_LEN - ENTRY_LEN, 
                                                 entPos, currSecArr, 
                                                 lnStr, &lnStrIndx);
                              
                              pvt_update_fat_entry_state (lnStr, entPos,
                                currSecNumInClus, clusIndx, snPosCurrSec, 
                                snPosNextSec, lnMask, nextSecArr, currEnt);
                              
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
                          // is first entPos of a long name.
                          if ((currSecArr[snPosCurrSec - ENTRY_LEN] 
                               & LN_ORD_MASK) != 1)
                            return CORRUPT_FAT_ENTRY;
                          
                          // load long name entry into lnStr[]
                          pvt_load_long_name(snPosCurrSec - ENTRY_LEN, 
                                             entPos, currSecArr, 
                                             lnStr, &lnStrIndx);

                          pvt_update_fat_entry_state (lnStr, entPos,
                              currSecNumInClus, clusIndx, snPosCurrSec, 
                              snPosNextSec, lnMask, currSecArr, currEnt);

                          return SUCCESS;                                                             
                        }                   
                    }

                  // Long name entry does not exist, use short name instead.
                  else
                    {
                      snPosCurrSec = entPos;

                      attrByte = currSecArr[snPosCurrSec + 11];
                      
                      pvt_update_fat_entry_state (lnStr, entPos,
                              currSecNumInClus, clusIndx, snPosCurrSec, 
                              snPosNextSec, lnMask, currSecArr, currEnt);
        
                      return SUCCESS;  
                    }
                //}
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
  ent->snEntClusIndx = Dir->FATFirstCluster;

  uint8_t err = 0;
  do 
    {
      err = fat_next_entry(Dir, ent, bpb);
      if (err != SUCCESS) return err;
      
      if (!strcmp(ent->longName, newDirStr) 
           && (ent->snEnt[11] & DIR_ENTRY_ATTR))
        {                                                        
          Dir->FATFirstCluster = ent->snEnt[21];
          Dir->FATFirstCluster <<= 8;
          Dir->FATFirstCluster |= ent->snEnt[20];
          Dir->FATFirstCluster <<= 8;
          Dir->FATFirstCluster |= ent->snEnt[27];
          Dir->FATFirstCluster <<= 8;
          Dir->FATFirstCluster |= ent->snEnt[26];
          
          uint8_t snLen;
          if (strlen(newDirStr) < 8) snLen = strlen(newDirStr);
          else snLen = 8; 

          char sn[9];                                    
          for (uint8_t k = 0; k < snLen; k++)  
            sn[k] = ent->snEnt[k];
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
|             : entFilt      - Byte of Entry Filter Flags. This specifies which
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
  ent->snEntClusIndx = dir->FATFirstCluster;

  while ( fat_next_entry(dir, ent, bpb) != END_OF_DIRECTORY)
    { 
      //if (ent->snEnt[0] != 0xE5)
      // {
          //print_str("\n\r entPos = "); print_dec(ent->entPos);
          if ((  !(ent->snEnt[11] & HIDDEN_ATTR)) 
              || ((ent->snEnt[11] & HIDDEN_ATTR) 
              && (entFilt & HIDDEN)))
            {      
              if ((entFilt & SHORT_NAME) == SHORT_NAME)
                {
                  pvt_print_entry_fields (ent->snEnt, 0, entFilt);
                  pvt_print_short_name(ent->snEnt, 0, entFilt);
                }

              if ((entFilt & LONG_NAME) == LONG_NAME)
                {
                  pvt_print_entry_fields (ent->snEnt, 0, entFilt);
                  if (ent->snEnt[11] & DIR_ENTRY_ATTR) 
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
      //  }        
    }
  return END_OF_DIRECTORY;
}



/*
-------------------------------------------------------------------------------
|                            PRINT FILE TO SCREEN
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
  ent->snEntClusIndx = Dir->FATFirstCluster;

  uint8_t err = 0;
  do 
    {
      err = fat_next_entry(Dir, ent, bpb);
      if (err != SUCCESS) return err;

      if (!strcmp(ent->longName, fileNameStr)
          && !(ent->snEnt[11] & DIR_ENTRY_ATTR))
        {                                                        
          print_str("\n\n\r");
          return pvt_print_fat_file (0, ent->snEnt, bpb);
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


void
pvt_update_fat_entry_state (char *lnStr, uint16_t entPos, 
                            uint8_t snEntSecNumInClus, uint32_t snEntClusIndx,
                            uint16_t snPosCurrSec, uint16_t snPosNextSec, 
                            uint8_t lnMask, uint8_t *secArr, FatEntry *ent)
{
  char sn[13];
  uint8_t ndx = 0;

  uint16_t snPos;
  if (lnMask & (LN_CROSS_SEC | LN_LAST_SEC_ENTRY))
    snPos = snPosNextSec;
  else
    snPos = snPosCurrSec;

  if (lnMask & LN_EXISTS) 
    ent->entPos = entPos;
  else 
    ent->entPos = entPos + ENTRY_LEN;

  ent->snEntSecNumInClus = snEntSecNumInClus;
  ent->snEntClusIndx = snEntClusIndx;
  ent->snPosCurrSec = snPosCurrSec;
  ent->snPosNextSec = snPosNextSec;
  ent->lnFlags = lnMask;

  for (uint8_t k = 0; k < 32; k++)
    ent->snEnt[k] = secArr[snPos + k];

  for (uint8_t k = 0; k < 13; k++)
    sn[k] = '\0';
  
  ndx = 0;
  for (uint8_t k = 0; k < 8; k++)
    {
      if (secArr[snPos + k] != ' ')
        { 
          sn[ndx] = secArr[snPos + k];
          ndx++;
        }
    }
  if (secArr[snPos + 8] != ' ')
    {
      sn[ndx] = '.';
      ndx++;
      for (uint8_t k = 8; k < 11; k++)
        {
          if (secArr[snPos + k] != ' ')
            { 
              sn[ndx] = secArr[snPos + k];
              ndx++;
            }
        }
    }

  strcpy(ent->shortName, sn);
  
  if ( !(lnMask & LN_EXISTS) )
    strcpy(ent->longName, sn);
  
  else
    {
      for (uint8_t i = 0; i < LN_STRING_LEN_MAX; i++)
      {
        ent->longName[i] = lnStr[i];
        if (*lnStr == '\0') 
          break;
      }
    }      
}

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
  if (strlen (nameStr) > LN_STRING_LEN_MAX) 
    return 1;
  if (( strlen (nameStr) + strlen (Dir->longParentPath)) > PATH_STRING_LEN_MAX)
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
  uint16_t createTime;
  uint16_t createDate;
  uint16_t lastAccDate;
  uint16_t writeTime;
  uint16_t writeDate;
  uint32_t fileSize;

  print_str ("\n\r");

  // Print creation date and time 
  if (CREATION & entFilt)
    {
      createTime = secArr[entPos + 15];
      createTime <<= 8;
      createTime |= secArr[entPos + 14];
      
      createDate = secArr[entPos + 17];
      createDate <<= 8;
      createDate |= secArr[entPos + 16];
      print_str ("    ");

      if (((createDate & 0x01E0) >> 5) < 10) 
        print_str ("0");

      print_dec ((createDate & 0x01E0) >> 5);
      print_str ("/");
      if ((createDate & 0x001F) < 10)
        print_str ("0");
      
      print_dec (createDate & 0x001F);
      print_str ("/");
      print_dec (1980 + ((createDate & 0xFE00) >> 9));
      print_str ("  ");
      if (((createTime & 0xF800) >> 11) < 10) 
        print_str ("0");
      
      print_dec (((createTime & 0xF800) >> 11));
      print_str (":");
      if (((createTime & 0x07E0) >> 5) < 10)
        print_str ("0");
      
      print_dec ((createTime & 0x07E0) >> 5);
      print_str (":");
      if ((2 * (createTime & 0x001F)) < 10) 
        print_str ("0");

      print_dec (2 * (createTime & 0x001F));
    }

  // Print last access date
  if (LAST_ACCESS & entFilt)
    {
      lastAccDate = secArr[entPos + 19];
      lastAccDate <<= 8;
      lastAccDate |= secArr[entPos + 18];

      print_str ("     ");
      if (((lastAccDate & 0x01E0) >> 5) < 10)
        print_str ("0");

      print_dec ((lastAccDate & 0x01E0) >> 5);
      print_str ("/");
      if ((lastAccDate & 0x001F) < 10) 
        print_str("0");

      print_dec (lastAccDate & 0x001F);
      print_str ("/");
      print_dec (1980 + ((lastAccDate & 0xFE00) >> 9));
    }

  // Print last modified date / time
  if (LAST_MODIFIED & entFilt)
    {
      // Load and Print write date
      writeDate = secArr[entPos + 25];
      writeDate <<= 8;
      writeDate |= secArr[entPos + 24];
   
      print_str ("     ");
      if (((writeDate & 0x01E0) >> 5) < 10) 
        print_str ("0");

      print_dec ((writeDate & 0x01E0) >> 5);
      print_str ("/");
      if ((writeDate & 0x001F) < 10) 
        print_str ("0");

      print_dec (writeDate & 0x001F);
      print_str ("/");
      print_dec (1980 + ((writeDate & 0xFE00) >> 9));

      // Load and Print write time
      writeTime = secArr[entPos + 23];
      writeTime <<= 8;
      writeTime |= secArr[entPos + 22];

      print_str ("  ");
      if (((writeTime & 0xF800) >> 11) < 10)
        print_str ("0");

      print_dec (((writeTime & 0xF800) >> 11));
      print_str (":");      
      if (((writeTime & 0x07E0) >> 5) < 10) 
        print_str ("0");

      print_dec ((writeTime & 0x07E0) >> 5);
      print_str (":");
      if ((2 * (writeTime & 0x001F)) < 10) 
        print_str ("0");

      print_dec (2 * (writeTime & 0x001F));
    }
  
  print_str ("     ");

  // Print file size in KB
  if (entFilt & FILE_SIZE)
    {
      uint16_t div = 10000;
      
      fileSize = secArr[entPos + 31];
      fileSize <<= 8;
      fileSize |= secArr[entPos + 30];
      fileSize <<= 8;
      fileSize |= secArr[entPos + 29];
      fileSize <<= 8;
      fileSize |= secArr[entPos + 28];

          if ((fileSize / div) >= 10000000) print_str(" ");
      else if ((fileSize / div) >= 1000000) print_str("  ");
      else if ((fileSize / div) >= 100000) print_str("   ");
      else if ((fileSize / div) >= 10000) print_str("    ");
      else if ((fileSize / div) >= 1000) print_str("     ");
      else if ((fileSize / div) >= 100) print_str("      ");
      else if ((fileSize / div) >= 10) print_str("       "); 
      else                            print_str("        ");
      
      print_dec(fileSize / div);
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
