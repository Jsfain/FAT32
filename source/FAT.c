/*
***********************************************************************************************************************
*                                                       AVR-FAT
*                                        
*                                           Copyright (c) 2020 Joshua Fain
*                                                 All Rights Reserved
*
* File : FAT.C
* By   : Joshua Fain
***********************************************************************************************************************
*/



#include <string.h>
#include <avr/io.h>
#include "../includes/fattosd.h"
#include "../includes/fat.h"
#include "../includes/prints.h"
#include "../includes/usart.h"



/*
***********************************************************************************************************************
 *                                          "PRIVATE" FUNCTION DECLARATIONS
***********************************************************************************************************************
*/

// Private function descriptions are only included with their definitions at the bottom of this file.

uint8_t pvt_CorrectEntryCheck (uint8_t* lnFlags, uint16_t* entryPos,
                               uint16_t* snPosCurrSec, uint16_t* snPosNextSec); 
void pvt_SetLongNameFlags (uint8_t* lnFlags, uint16_t entryPos, uint16_t* snPosCurrSec,
                           uint8_t* currSecArr, BPB * bpb);
void pvt_LoadLongName (int longNameFirstEntry, int longNameLastEntry, 
                       uint8_t* sector, char* lnStr, uint8_t* lnStrIndx);
void pvt_GetNextSector (uint8_t* nextSecArr, uint32_t currSecNumInClus, 
                        uint32_t currSecNumPhys, uint32_t clusIndx, BPB* bpb);

void pvt_SetCurrentDirectoryToChild (FatDir* Dir, uint8_t* sector, uint16_t snPos, char* nameStr, BPB* bpb);
void pvt_SetCurrentDirectoryToParent (FatDir* Dir, BPB* bpb);
uint32_t pvt_GetNextClusterIndex (uint32_t currentClusterIndex, BPB* bpb);
uint8_t pvt_CheckLegalName (char* nameStr);
void pvt_PrintEntryFields (uint8_t* byte, uint16_t entryPos, uint8_t entryFilter);
void pvt_PrintShortNameAndType (uint8_t* byte, uint16_t entryPos);
void pvt_PrintFatFile (uint16_t entryPos, uint8_t* fileSector, BPB* bpb);



/*
***********************************************************************************************************************
 *                                            "PUBLIC" FUNCTION DEFINITIONS
***********************************************************************************************************************
*/


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
FAT_SetBiosParameterBlock (BPB * bpb)
{
  uint8_t BootSector[SECTOR_LEN];

  bpb->bootSecAddr = fat_FindBootSector();
  
  if (bpb->bootSecAddr != 0xFFFFFFFF)
    fat_ReadSingleSector (bpb->bootSecAddr, BootSector);
  else
    return BOOT_SECTOR_NOT_FOUND;

  // Confirm signature bytes
  if ((BootSector[SECTOR_LEN - 2] == 0x55) && (BootSector[SECTOR_LEN - 1] == 0xAA))
    {
      bpb->bytesPerSec = BootSector[12];
      bpb->bytesPerSec <<= 8;
      bpb->bytesPerSec |= BootSector[11];
      
      if (bpb->bytesPerSec != SECTOR_LEN) 
        return INVALID_BYTES_PER_SECTOR;

      // secPerClus
      bpb->secPerClus = BootSector[13];

      if ((  bpb->secPerClus != 1 ) && (bpb->secPerClus != 2 ) && (bpb->secPerClus != 4 ) && (bpb->secPerClus != 8) 
         && (bpb->secPerClus != 16) && (bpb->secPerClus != 32) && (bpb->secPerClus != 64) && (bpb->secPerClus != 128))
        {
          return INVALID_SECTORS_PER_CLUSTER;
        }

      bpb->rsvdSecCnt = BootSector[15];
      bpb->rsvdSecCnt <<= 8;
      bpb->rsvdSecCnt |= BootSector[14];

      bpb->numOfFats = BootSector[16];

      bpb->fatSize32 =  BootSector[39];
      bpb->fatSize32 <<= 8;
      bpb->fatSize32 |= BootSector[38];
      bpb->fatSize32 <<= 8;
      bpb->fatSize32 |= BootSector[37];
      bpb->fatSize32 <<= 8;
      bpb->fatSize32 |= BootSector[36];

      bpb->rootClus =  BootSector[47];
      bpb->rootClus <<= 8;
      bpb->rootClus |= BootSector[46];
      bpb->rootClus <<= 8;
      bpb->rootClus |= BootSector[45];
      bpb->rootClus <<= 8;
      bpb->rootClus |= BootSector[44];

      bpb->dataRegionFirstSector = bpb->bootSecAddr + bpb->rsvdSecCnt + (bpb->numOfFats * bpb->fatSize32);
      
      return BOOT_SECTOR_VALID;
    }
  else 
    return NOT_BOOT_SECTOR;
}



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
FAT_PrintBootSectorError (uint8_t err)
{  
  switch(err)
  {
    case BOOT_SECTOR_VALID : 
      print_str("BOOT_SECTOR_VALID ");
      break;
    case CORRUPT_BOOT_SECTOR :
      print_str("CORRUPT_BOOT_SECTOR ");
      break;
    case NOT_BOOT_SECTOR :
      print_str("NOT_BOOT_SECTOR ");
      break;
    case INVALID_BYTES_PER_SECTOR:
      print_str("INVALID_BYTES_PER_SECTOR");
      break;
    case INVALID_SECTORS_PER_CLUSTER:
      print_str("INVALID_SECTORS_PER_CLUSTER");
      break;
    case BOOT_SECTOR_NOT_FOUND:
      print_str("BOOT_SECTOR_NOT_FOUND");
      break;
    default:
      print_str("UNKNOWN_ERROR");
      break;
  }
}



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
FAT_SetDirectory (FatDir * Dir, char * newDirStr, BPB * bpb)
{
  if (pvt_CheckLegalName (newDirStr)) 
    return INVALID_DIR_NAME;

  // newDirStr == 'Current Directory' ?
  if (!strcmp (newDirStr,  ".")) 
    return SUCCESS;
  
  // newDirStr == 'Parent Directory' ?
  if (!strcmp (newDirStr, ".."))
    {
      pvt_SetCurrentDirectoryToParent (Dir, bpb);
      return SUCCESS;
    }

  uint8_t  newDirStrLen = strlen (newDirStr);
  uint32_t clusIndx = Dir->FATFirstCluster;
  uint8_t  currSecArr[bpb->bytesPerSec]; 
  uint8_t  nextSecArr[bpb->bytesPerSec];
  uint8_t  attrByte; // attribute byte
  uint32_t currSecNumPhys; // physical (disk) sector number
  uint8_t  entryCorrectionFlag = 0;
  // sn -> short name
  uint16_t snPosCurrSec = 0; 
  uint16_t snPosNextSec = 0;
  // ln -> long name
  char     lnStr[LONG_NAME_LEN_MAX];
  uint8_t  lnStrIndx = 0;
  uint8_t  lnFlags = 0;

  do
    {
      for (uint32_t currSecNumInClus = 0; currSecNumInClus < bpb->secPerClus; currSecNumInClus++)
        {         
          // load sector data into currSecArr
          currSecNumPhys = currSecNumInClus + bpb->dataRegionFirstSector + ((clusIndx - 2) * bpb->secPerClus);
          fat_ReadSingleSector (currSecNumPhys, currSecArr);

          for (uint16_t entryPos = 0; entryPos < SECTOR_LEN; entryPos += ENTRY_LEN)
            {
              entryCorrectionFlag = pvt_CorrectEntryCheck ( &lnFlags, &entryPos, &snPosCurrSec, &snPosNextSec);
              if (entryCorrectionFlag == 1)
                {
                  entryCorrectionFlag = 0;
                  break;    
                }

              // reset long name flags
              lnFlags = 0;

              // If first value of entry is 0 then all subsequent entries are empty.
              if (currSecArr[entryPos] == 0) return END_OF_DIRECTORY;

              // If 0xE5 then entry is marked for deletion.
              if (currSecArr[entryPos] != 0xE5)
                {                 
                  attrByte = currSecArr[entryPos + 11];

                  // If entry being checked is a long name entryPos
                  if ((attrByte & LONG_NAME_ATTR_MASK) == LONG_NAME_ATTR_MASK)
                    {
                      if ( !(currSecArr[entryPos] & LONG_NAME_LAST_ENTRY)) return CORRUPT_FAT_ENTRY;
                      
                      for (uint8_t k = 0; k < LONG_NAME_LEN_MAX; k++) lnStr[k] = '\0';

                      lnStrIndx = 0;
                      pvt_SetLongNameFlags ( &lnFlags, entryPos, &snPosCurrSec, currSecArr, bpb);

                      if (lnFlags & (LONG_NAME_CROSSES_SECTOR_BOUNDARY_FLAG | LONG_NAME_LAST_SECTOR_ENTRY_FLAG))
                        {
                          pvt_GetNextSector (nextSecArr, currSecNumInClus, currSecNumPhys, clusIndx, bpb);
                          snPosNextSec = snPosCurrSec - bpb->bytesPerSec;
                          attrByte = nextSecArr[snPosNextSec + 11];

                          // If snPosNextSec points to long name entry then something is wrong.
                          if ((attrByte & LONG_NAME_ATTR_MASK) == LONG_NAME_ATTR_MASK) return CORRUPT_FAT_ENTRY;

                          // Only continue for current entry if it is a directory.
                          if (attrByte & DIRECTORY_ENTRY_ATTR)
                            {                                                           
                              if (lnFlags & LONG_NAME_CROSSES_SECTOR_BOUNDARY_FLAG)
                                {
                                  // Entry immediately preceeding short name must be the long names's first entry.
                                  if ((nextSecArr[snPosNextSec - ENTRY_LEN] & LONG_NAME_ORDINAL_MASK) != 1)
                                    return CORRUPT_FAT_ENTRY;

                                  // Load long name entry into lnStr[]
                                  pvt_LoadLongName (snPosNextSec - ENTRY_LEN, 0, nextSecArr, lnStr, &lnStrIndx);
                                  pvt_LoadLongName (SECTOR_LEN - ENTRY_LEN, entryPos, currSecArr, lnStr, &lnStrIndx);
                                  if ( !strcmp (newDirStr, lnStr)) 
                                    {                                                        
                                      pvt_SetCurrentDirectoryToChild (Dir, nextSecArr, snPosNextSec, newDirStr, bpb);
                                      return SUCCESS;
                                    }
                                }

                              // All entries for long name are in current sector but short name is in next sector
                              else if (lnFlags & LONG_NAME_LAST_SECTOR_ENTRY_FLAG)
                               {
                                  // Entry immediately preceeding short name must be the long names's first entry.
                                  if ((currSecArr[SECTOR_LEN - ENTRY_LEN] & LONG_NAME_ORDINAL_MASK) != 1)
                                    return CORRUPT_FAT_ENTRY;

                                  // Load long name entry into lnStr[]
                                  pvt_LoadLongName(SECTOR_LEN - ENTRY_LEN, entryPos, currSecArr, lnStr, &lnStrIndx);
                                  if ( !strcmp (newDirStr, lnStr)) 
                                    { 
                                      pvt_SetCurrentDirectoryToChild (Dir, nextSecArr, snPosNextSec, newDirStr, bpb);
                                      return SUCCESS;
                                    }
                                }
                              else return CORRUPT_FAT_ENTRY;
                            }
                        }

                      // Long name exists and is entirely in current sector along with the short name
                      else
                        {   
                          // Confirm entry preceding short name is first entry of a long name.
                          if ((currSecArr[snPosCurrSec - ENTRY_LEN] & LONG_NAME_ORDINAL_MASK) != 1)                            
                            return CORRUPT_FAT_ENTRY;
        
                          attrByte = currSecArr[snPosCurrSec + 11];

                          // Only continue for current entry if it is a directory.
                          if ((attrByte & DIRECTORY_ENTRY_ATTR))
                            {
                              // load long name entry into lnStr[]
                              pvt_LoadLongName (snPosCurrSec - ENTRY_LEN, entryPos, currSecArr, lnStr, &lnStrIndx);
                              if  (!strcmp (newDirStr, lnStr)) 
                                { 
                                  pvt_SetCurrentDirectoryToChild (Dir, currSecArr, snPosCurrSec, newDirStr, bpb);
                                  return SUCCESS;
                                }                                       
                            }
                        }                      
                    }                   

                  // Long name entry does not exist
                  else if ((newDirStrLen < 9) && (attrByte & DIRECTORY_ENTRY_ATTR))
                    {                 
                      char sn[9];
                      for (uint8_t k = 0; k < newDirStrLen; k++) sn[k] = currSecArr[k + entryPos];
                      sn[newDirStrLen] = '\0';
                      if ( !strcmp (newDirStr, sn))
                        { 
                          pvt_SetCurrentDirectoryToChild (Dir, currSecArr, entryPos, newDirStr, bpb);
                          return SUCCESS;
                        }
                    }              
                }
            }
        }
    } 
  while ((clusIndx = pvt_GetNextClusterIndex(clusIndx, bpb)) != END_OF_CLUSTER);
  
  return END_OF_DIRECTORY;
}



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
FAT_PrintCurrentDirectory (FatDir * Dir, uint8_t entryFilter, BPB * bpb)
{
  uint32_t clusIndx = Dir->FATFirstCluster;
  uint8_t  currSecArr[bpb->bytesPerSec]; 
  uint8_t  nextSecArr[bpb->bytesPerSec];
  uint8_t  attrByte; // attribute byte
  uint32_t currSecNumPhys; // physical (disk) sector number
  uint8_t  entryCorrectionFlag = 0;
  // sn -> short name
  uint16_t snPosCurrSec = 0; 
  uint16_t snPosNextSec = 0;
  // ln -> long name
  char     lnStr[LONG_NAME_LEN_MAX];
  uint8_t  lnStrIndx = 0;
  uint8_t  lnFlags = 0;

  // Prints column headers according to entryFilter
  print_str("\n\n\r");
  if (CREATION & entryFilter) print_str(" CREATION DATE & TIME,");
  if (LAST_ACCESS & entryFilter) print_str(" LAST ACCESS DATE,");
  if (LAST_MODIFIED & entryFilter) print_str(" LAST MODIFIED DATE & TIME,");
  print_str(" SIZE, TYPE, NAME");
  print_str("\n\n\r");

  do 
    {
      for (uint32_t currSecNumInClus = 0; currSecNumInClus < bpb->secPerClus; currSecNumInClus++)
        {
          // load sector bytes into currSecArr[]
          currSecNumPhys = currSecNumInClus + bpb->dataRegionFirstSector + ((clusIndx - 2) * bpb->secPerClus);
          fat_ReadSingleSector (currSecNumPhys, currSecArr);

          for (uint16_t entryPos = 0; entryPos < bpb->bytesPerSec; entryPos = entryPos + ENTRY_LEN)
            {
              entryCorrectionFlag = pvt_CorrectEntryCheck ( &lnFlags, &entryPos, &snPosCurrSec, &snPosNextSec);
              if (entryCorrectionFlag == 1)
                {
                  entryCorrectionFlag = 0;
                  break;    
                }

              // reset long name flags
              lnFlags = 0;

              // If first value of entry is 0 then all subsequent entries are empty.
              if (currSecArr[entryPos] == 0) return END_OF_DIRECTORY;

              // Only continue with current entry if it has not been marked for deletion
              if (currSecArr[entryPos] != 0xE5)
                {
                  attrByte = currSecArr[entryPos + 11];
                  
                  if ((attrByte & LONG_NAME_ATTR_MASK) == LONG_NAME_ATTR_MASK)
                    {
                      if ( !(currSecArr[entryPos] & LONG_NAME_LAST_ENTRY)) return CORRUPT_FAT_ENTRY;

                      for (uint8_t k = 0; k < LONG_NAME_LEN_MAX; k++) lnStr[k] = '\0';
 
                      lnStrIndx = 0;

                      pvt_SetLongNameFlags ( &lnFlags, entryPos, &snPosCurrSec, currSecArr, bpb);

                      if (lnFlags & (LONG_NAME_CROSSES_SECTOR_BOUNDARY_FLAG | LONG_NAME_LAST_SECTOR_ENTRY_FLAG))
                        {
                          pvt_GetNextSector ( nextSecArr, currSecNumInClus, currSecNumPhys, clusIndx, bpb );
                          snPosNextSec = snPosCurrSec - bpb->bytesPerSec;
                          attrByte = nextSecArr[snPosNextSec + 11];
                          
                          // If snPosNextSec points to a long name entry then something is wrong.
                          if ((attrByte & LONG_NAME_ATTR_MASK) == LONG_NAME_ATTR_MASK) 
                            return CORRUPT_FAT_ENTRY;
                            
                          // Print entry if hidden attribute is not set OR if it AND the hidden filter flag are set.
                          if ( (!(attrByte & HIDDEN_ATTR)) || ((attrByte & HIDDEN_ATTR) && (entryFilter & HIDDEN)))
                            {                                                           
                              // print long name's associated short name if filter flag is set.
                              if (entryFilter & SHORT_NAME)
                                {
                                  pvt_PrintEntryFields(nextSecArr, snPosNextSec, entryFilter);
                                  pvt_PrintShortNameAndType(nextSecArr, snPosNextSec);
                                }
                              // print long name if filter flag is set.
                              if (entryFilter & LONG_NAME)
                                {
                                  if (lnFlags & LONG_NAME_CROSSES_SECTOR_BOUNDARY_FLAG)
                                    {
                                      // Entry immediately preceeding short name must be the long names's first entry.
                                      if ((nextSecArr[snPosNextSec - ENTRY_LEN] & LONG_NAME_ORDINAL_MASK) != 1) 
                                        return CORRUPT_FAT_ENTRY;                                              

                                      pvt_PrintEntryFields (nextSecArr, snPosNextSec, entryFilter);

                                      if (attrByte & DIRECTORY_ENTRY_ATTR) 
                                        print_str("    <DIR>    ");
                                      else 
                                        print_str("   <FILE>    ");
                                      
                                      // load long name entryPos into lnStr[]
                                      pvt_LoadLongName (snPosNextSec - ENTRY_LEN, 0, 
                                                        nextSecArr, lnStr, &lnStrIndx);
                                      pvt_LoadLongName (SECTOR_LEN - ENTRY_LEN, entryPos, 
                                                        currSecArr, lnStr, &lnStrIndx);
                                      print_str(lnStr);
                                    }

                                  else if (lnFlags & LONG_NAME_LAST_SECTOR_ENTRY_FLAG)
                                    {
                                      // Entry immediately preceeding short name must be the long names's first entry.
                                      if ((currSecArr[ SECTOR_LEN - ENTRY_LEN] & LONG_NAME_ORDINAL_MASK) != 1) 
                                        return CORRUPT_FAT_ENTRY;
                            
                                      pvt_PrintEntryFields (nextSecArr, snPosNextSec, entryFilter);

                                      if (attrByte & DIRECTORY_ENTRY_ATTR) 
                                        print_str("    <DIR>    ");
                                      else 
                                        print_str("   <FILE>    ");
                                      
                                      // load long name entryPos into lnStr[]
                                      pvt_LoadLongName (SECTOR_LEN - ENTRY_LEN, entryPos, 
                                                        currSecArr, lnStr, &lnStrIndx);
                                      print_str(lnStr); 
                                    }
                                  else return CORRUPT_FAT_ENTRY;
                                }
                            }
                        }

                      // Long name exists and is entirely in current sector along with the short name
                      else
                        {   
                          attrByte = currSecArr[snPosCurrSec + 11];
                          
                          // if snPosCurrSec points to long name entry, then somethine is wrong.
                          if ((attrByte & LONG_NAME_ATTR_MASK) == LONG_NAME_ATTR_MASK) return CORRUPT_FAT_ENTRY;

                          // Print entry if hidden attribute is not set OR if it AND the hidden filter flag are set.
                          if ( (!(attrByte & HIDDEN_ATTR)) || ((attrByte & HIDDEN_ATTR) && (entryFilter & HIDDEN)))
                            {                   
                              // print long name's associated short name if filter flag is set.
                              if (entryFilter & SHORT_NAME)
                                {
                                  pvt_PrintEntryFields (currSecArr, snPosCurrSec, entryFilter);
                                  pvt_PrintShortNameAndType (currSecArr, snPosCurrSec);
                                }
                              // print long name if filter flag is set.
                              if (entryFilter & LONG_NAME)
                                {
                                  // Confirm entry preceding short name is first entryPos of a long name.
                                  if ((currSecArr[snPosCurrSec - ENTRY_LEN] & LONG_NAME_ORDINAL_MASK) != 1) 
                                    return CORRUPT_FAT_ENTRY;

                                  pvt_PrintEntryFields(currSecArr, snPosCurrSec, entryFilter);
                                  
                                  if(attrByte & DIRECTORY_ENTRY_ATTR) 
                                    print_str("    <DIR>    ");
                                  else 
                                    print_str("   <FILE>    ");

                                  // load long name entry into lnStr[]
                                  pvt_LoadLongName (snPosCurrSec - ENTRY_LEN, entryPos, 
                                                    currSecArr, lnStr, &lnStrIndx);

                                  print_str(lnStr);                                       
                                }
                            }
                        }                   
                    }

                  // Long name entry does not exist, use short name instead regardless of SHORT_NAME entryFilter.
                  else
                    {
                      attrByte = currSecArr[entryPos + 11];

                      // Print entry if hidden attribute is not set OR if it AND the hidden filter flag are set.
                      if ( (!(attrByte & HIDDEN_ATTR)) || ((attrByte & HIDDEN_ATTR) && (entryFilter & HIDDEN)))
                        {
                          pvt_PrintEntryFields(currSecArr, entryPos, entryFilter);
                          pvt_PrintShortNameAndType(currSecArr, entryPos);
                        }
                    }
                }          
            }
        }
    }
  while ((clusIndx = pvt_GetNextClusterIndex( clusIndx, bpb )) != END_OF_CLUSTER);

  return END_OF_DIRECTORY;
}



/*
***********************************************************************************************************************
 *                                               PRINT FILE TO SCREEN
 * 
 * Description : Prints the contents of a file from the current Dir to a terminal/screen.
 * 
 * Arguments   : *Dir   pointer to a FatDir struct whose members must be associated with a 
 *                                   valid FAT32 Dir.
 *             : *fileNameStr        ptr to C-string that is the name of the file to be printed to the screen. This
 *                                   must be a long name, unless there is no associated long name with an entryPos, in 
 *                                   which case it can be a short name.
 *             : *bpb            - Pointer to a valid instance of a BPB struct.
 * 
 * Return      : FAT Error Flag     Returns END_OF_FILE if the function completed successfully and was able to
 *                                  read in and print a file's contents to the screen. Any other value returned 
 *                                  indicates an error. Pass the returned value to FAT_PrintError(ErrorFlag).
***********************************************************************************************************************
*/

uint8_t 
FAT_PrintFile (FatDir * Dir, char * fileNameStr, BPB * bpb)
{
  if (pvt_CheckLegalName (fileNameStr)) 
    return INVALID_DIR_NAME;
  

  uint8_t  fileNameStrLen = strlen(fileNameStr);
  uint32_t clusIndx = Dir->FATFirstCluster;
  uint8_t  currSecArr[ bpb->bytesPerSec ];
  uint32_t currSecNumPhys;
  uint16_t snPosCurrSec = 0;
  uint8_t  nextSecArr[ bpb->bytesPerSec ];
  uint16_t snPosNextSec = 0;
  uint8_t  attrByte;
  char     lnStr[LONG_NAME_LEN_MAX];
  uint8_t  lnStrIndx = 0;
  uint8_t lnFlags = 0;
  uint8_t entryCorrectionFlag = 0;

  // ***     Search files in current Dir for match to fileNameStr and print if found    *** /
  
  // loop through the current Dir's clusters
  int clusCnt = 0;    
  do
    {
      clusCnt++;
      // loop through sectors in current cluster.
      for (uint32_t currSecNumInClus = 0; currSecNumInClus < bpb->secPerClus; currSecNumInClus++)
        {     
          // load sector bytes into currSecArr[]
          currSecNumPhys = currSecNumInClus + bpb->dataRegionFirstSector + ((clusIndx - 2) * bpb->secPerClus);
          fat_ReadSingleSector (currSecNumPhys, currSecArr );
          
          // loop through entries in the current sector.
          for (uint16_t entryPos = 0; entryPos < bpb->bytesPerSec; entryPos = entryPos + ENTRY_LEN)
            { 
              entryCorrectionFlag = pvt_CorrectEntryCheck ( &lnFlags, &entryPos, &snPosCurrSec, &snPosNextSec);
              if (entryCorrectionFlag == 1)
                {
                  entryCorrectionFlag = 0;
                  break;    
                }
              
              // reset long name flags
              lnFlags = 0;

              // If first value of entryPos is 0 then all subsequent entries are empty.
              if (currSecArr[ entryPos ] == 0) 
                return END_OF_DIRECTORY;

              // entryPos is marked for deletion. Do nothing.
              if (currSecArr[ entryPos ] != 0xE5)
                {
                  attrByte = currSecArr[ (entryPos) + 11 ];
                  
                  // if entryPos being checked is a long name entryPos
                  if ((attrByte & LONG_NAME_ATTR_MASK) == LONG_NAME_ATTR_MASK)
                    {
                      if ( !(currSecArr[ entryPos ] & LONG_NAME_LAST_ENTRY))
                        return CORRUPT_FAT_ENTRY;
                      
                      lnStrIndx = 0;

                      for (uint8_t k = 0; k < LONG_NAME_LEN_MAX; k++) 
                        lnStr[k] = '\0';

                      pvt_SetLongNameFlags ( &lnFlags, entryPos, &snPosCurrSec, currSecArr, bpb);
                      if (lnFlags & (LONG_NAME_CROSSES_SECTOR_BOUNDARY_FLAG | LONG_NAME_LAST_SECTOR_ENTRY_FLAG))
                        {
                          pvt_GetNextSector (nextSecArr, currSecNumInClus, currSecNumPhys, clusIndx, bpb );
                          snPosNextSec = snPosCurrSec - bpb->bytesPerSec;
                          attrByte = nextSecArr[ snPosNextSec + 11 ];
                          
                          // If snPosNextSec points to long name entryPos then something is wrong.
                          if ((attrByte & LONG_NAME_ATTR_MASK) == LONG_NAME_ATTR_MASK) 
                            return CORRUPT_FAT_ENTRY;

                          // Only proceed if entryPos points to a file (i.e. Dir flag is not set)
                          if ( !(attrByte & DIRECTORY_ENTRY_ATTR))
                            {                                                           
                              if (lnFlags & LONG_NAME_CROSSES_SECTOR_BOUNDARY_FLAG)
                                {
                                  // if entryPos immediately before the short name entryPos is not the ORDER = 1 entryPos of a long name then something is wrong
                                  if ((nextSecArr[ snPosNextSec - ENTRY_LEN ] & LONG_NAME_ORDINAL_MASK) != 1) 
                                    return CORRUPT_FAT_ENTRY; 
                                  
                                  // load long name entryPos into lnStr[]
                                  pvt_LoadLongName (snPosNextSec - ENTRY_LEN, 0, nextSecArr, lnStr, &lnStrIndx);
                                  pvt_LoadLongName (SECTOR_LEN - ENTRY_LEN, (int)entryPos, currSecArr, lnStr, &lnStrIndx);
                                  
                                  // print file contents if a matching entryPos was found
                                  if(!strcmp (fileNameStr,lnStr))
                                    { 
                                      pvt_PrintFatFile (snPosNextSec, nextSecArr, bpb); 
                                      return END_OF_FILE;
                                    }
                                }

                              // Long name is entirely in the current sector, but its short name is the first entryPos of the next sector
                              else if (lnFlags & LONG_NAME_LAST_SECTOR_ENTRY_FLAG)
                                {
                                  lnStrIndx = 0;

                                  // confirm last entryPos of current sector is the first entryPos of a long name
                                  if( (currSecArr[ SECTOR_LEN - ENTRY_LEN ] & LONG_NAME_ORDINAL_MASK) != 1 ) 
                                    return CORRUPT_FAT_ENTRY;                                                                  
                                      
                                  // read long name into lnStr
                                  pvt_LoadLongName (SECTOR_LEN - ENTRY_LEN, (int)entryPos, currSecArr, lnStr, &lnStrIndx);

                                  // print file contents if a matching entryPos was found
                                  if ( !strcmp(fileNameStr, lnStr))
                                    { 
                                      pvt_PrintFatFile (snPosNextSec, nextSecArr, bpb); 
                                      return END_OF_FILE;
                                    }                                            
                                }
                              else 
                                return CORRUPT_FAT_ENTRY;
                            }
                        }

                      // Long name exists and long and short name are entirely in the current Dir.
                      else 
                        {   
                          attrByte = currSecArr[snPosCurrSec+11];
                          
                          // confirm snPosCurrSec points to a short name entryPos in the current sector
                          if ((attrByte & LONG_NAME_ATTR_MASK) == LONG_NAME_ATTR_MASK) 
                            return CORRUPT_FAT_ENTRY;
                          
                          // proceed if entryPos is a file
                          if ( !(attrByte & DIRECTORY_ENTRY_ATTR))
                            {                   
                              // confirm entryPos immediatedly preceding the short name entryPos the first entryPos of a long name
                              if ((currSecArr[ snPosCurrSec - ENTRY_LEN ] & LONG_NAME_ORDINAL_MASK ) != 1 ) 
                                return CORRUPT_FAT_ENTRY;

                              // read long name into lnStr
                              pvt_LoadLongName (snPosCurrSec - ENTRY_LEN, (int)entryPos, currSecArr, lnStr, &lnStrIndx);

                              // print file contents if a matching entryPos was found
                              if ( !strcmp (fileNameStr, lnStr))
                                { 
                                  pvt_PrintFatFile(snPosCurrSec, currSecArr, bpb); 
                                  return END_OF_FILE;
                                }                                                                                    
                            }
                        }           
                    }

                  // Long name does not exist for current entryPos so
                  // check if fileNameStr matches the short name.
                  else if ((fileNameStrLen < 13) && !(attrByte & DIRECTORY_ENTRY_ATTR))
                    {                   
                      char sn[9];
                      char ext[4];

                      for (uint8_t k = 0; k < 9; k++) 
                        sn[k] = '\0';
                      for (uint8_t k = 0; k < 4; k++)
                        ext[k] = '\0'; 
              
                      // search for location of '.', if it exists, in fileNameStr. Exclude first position.
                      int pt = fileNameStrLen;
                      uint8_t fileNameExtExistsFlag = 0;
                      for(uint8_t k = 1; k < pt; k++)
                        {
                          if( k+1 >= fileNameStrLen ) 
                            break;
                          if( fileNameStr[k] == '.' )  
                            {   
                              fileNameExtExistsFlag = 1;
                              pt = k; 
                              break; 
                            }
                        }

                      char tempFileName[9];
                      for (uint8_t k = 0; k < pt; k++)
                        tempFileName[k] = fileNameStr[k];
                      for (uint8_t k = pt; k < 8; k++)
                        tempFileName[k] = ' ';
                      tempFileName[8] = '\0';
                      
                      for (uint8_t k = 0; k < 8; k++)
                        sn[k] = currSecArr[k + entryPos];

                      // if name portion of short name matches then check that extension matches.
                      if (!strcmp(tempFileName,sn))
                        {                                
                          uint8_t match = 0;
                          int entryExtExistsFlag = 0;

                          for (int k = 0; k < 3; k++)  
                            {
                              ext[k] = currSecArr[entryPos + 8 + k]; 
                              entryExtExistsFlag = 1;
                            }

                          if (strcmp (ext,"   ") && fileNameExtExistsFlag ) 
                            entryExtExistsFlag = 1;

                          if ( !entryExtExistsFlag && !fileNameExtExistsFlag) 
                            match = 1;
                          else if ((entryExtExistsFlag && !fileNameExtExistsFlag) || (!entryExtExistsFlag && fileNameExtExistsFlag)) 
                            match = 0;
                          else if (entryExtExistsFlag && fileNameExtExistsFlag)
                            {
                              char tempEXT[4];
                              for (uint8_t k = 0; k < 3; k++) 
                                tempEXT[k] = ' ';
                              tempEXT[3] = '\0'; 

                              for (uint8_t k = 0; k < 3; k++)
                                {
                                  if (fileNameStr[k+pt+1] == '\0') 
                                    break;
                                  tempEXT[k] = fileNameStr[k+pt+1];
                                }

                              // if extensions match set to match to 1
                              if ( !strcmp (ext,tempEXT)) 
                                match = 1;
                            }

                          if(match)
                            {
                              pvt_PrintFatFile (entryPos, currSecArr, bpb);
                              return END_OF_FILE;
                            }
                        }
                    }
                }
            }
        }
    } 
  while ( ((clusIndx = pvt_GetNextClusterIndex (clusIndx, bpb)) != END_OF_CLUSTER) && (clusCnt < 5));

  return FILE_NOT_FOUND; 
}



/*
***********************************************************************************************************************
 *                                        PRINT ERROR RETURNED BY A FAT FUNCTION
 * 
 * Description : Call this function to print an error flag returned by one of the FAT functions to the screen. 
 * 
 * Argument    : err    This should be an error flag returned by one of the FAT functions. If it is not then this
 *                      output is meaningless. 
***********************************************************************************************************************
*/

void 
FAT_PrintError (uint8_t err)
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
    default:
      print_str("\n\rUNKNOWN_ERROR");
      break;
  }
}



/*
***********************************************************************************************************************
 *                                          "PRIVATE" FUNCTION DEFINITIONS
***********************************************************************************************************************
*/



/*
***********************************************************************************************************************
 *                                          (PRIVATE) CHECK FOR LEGAL NAME
 * 
 * Description : This function is called by any FAT function that must match a 'name' string argument to a FAT entryPos 
 *               name (e.g. FAT_PrintFile or FAT_SetDirectory). This is used to confirm the name is a legal
 * 
 * Argument    : *nameStr    c-string that the calling function is requesting be verified as a legal FAT name.
 *            
 * Return      : 0 if name is LEGAL.
 *               1 if name is ILLEGAL.
***********************************************************************************************************************
*/

uint8_t
pvt_CheckLegalName (char * nameStr)
{
  // illegal if name string is empty or begins with a space character 
  if ((strcmp (nameStr, "") == 0) || (nameStr[0] == ' ')) 
    {
      return 1;
    }

  // illegal if name string contains an illegal character
  char illegalCharacters[] = {'\\','/',':','*','?','"','<','>','|'};
  for (uint8_t k = 0; k < strlen (nameStr); k++)
    {       
      for (uint8_t j = 0; j < 9; j++)
        {
          if (nameStr[k] == illegalCharacters[j])
          {
            return 1;
          }
        }
    }

  // illegal if name string is all space characters.
  for (uint8_t k = 0; k < strlen (nameStr); k++)  
    {
      if (nameStr[k] != ' ') 
        { 
          return 0;
        }
    }  

  return 1; // legal name
}



/*
***********************************************************************************************************************
 *                                  (PRIVATE) SET CURRENT DIRECTORY TO ITS PARENT
 * 
 * Description : This function is called by FAT_SetDirectory() if that function has been asked to set an 
 *               instance of a Dir's struct members to it's parent Dir.
 * 
 * Argument    : *Dir     pointer to an instance of a FatDir struct whose members will be
 *                                     updated to point to the parent of the Dir currently pointed to by the
 *                                     struct's members.
 *             : *bpb                  pointer to the BPB struct instance.
***********************************************************************************************************************
*/

void 
pvt_SetCurrentDirectoryToParent (FatDir * Dir, BPB * bpb)
{
  uint32_t parentDirectoryFirstCluster;
  uint32_t currSecNumPhys;
  uint8_t  currSecArr[bpb->bytesPerSec];

  //currSecNumPhys = bpb->dataRegionFirstSector + ((cluster - 2) * bpb->secPerClus);
  currSecNumPhys = bpb->dataRegionFirstSector + ((Dir->FATFirstCluster - 2) * bpb->secPerClus);

  fat_ReadSingleSector (currSecNumPhys, currSecArr);

  parentDirectoryFirstCluster = currSecArr[53];
  parentDirectoryFirstCluster <<= 8;
  parentDirectoryFirstCluster |= currSecArr[52];
  parentDirectoryFirstCluster <<= 8;
  parentDirectoryFirstCluster |= currSecArr[59];
  parentDirectoryFirstCluster <<= 8;
  parentDirectoryFirstCluster |= currSecArr[58];

  // if current Dir is root Dir, do nothing.
  if (Dir->FATFirstCluster == bpb->rootClus); 

  // parent Dir is root Dir
  else if (parentDirectoryFirstCluster == 0)
    {
      strcpy (Dir->shortName, "/");
      strcpy (Dir->shortParentPath, "");
      strcpy (Dir->longName, "/");
      strcpy (Dir->longParentPath, "");
      Dir->FATFirstCluster = bpb->rootClus;
    }
  else // parent Dir is not root Dir
    {          
      char tmpShortNamePath[64];
      char tmpLongNamePath[64];

      strlcpy (tmpShortNamePath, Dir->shortParentPath, strlen (Dir->shortParentPath));
      strlcpy (tmpLongNamePath, Dir->longParentPath,   strlen (Dir->longParentPath ));
      
      char *shortNameLastDirectoryInPath = strrchr (tmpShortNamePath, '/');
      char *longNameLastDirectoryInPath  = strrchr (tmpLongNamePath , '/');
      
      strcpy (Dir->shortName, shortNameLastDirectoryInPath + 1);
      strcpy (Dir->longName , longNameLastDirectoryInPath  + 1);

      strlcpy (Dir->shortParentPath, tmpShortNamePath, 
                (shortNameLastDirectoryInPath + 2) - tmpShortNamePath);
      strlcpy (Dir->longParentPath,  tmpLongNamePath, 
                (longNameLastDirectoryInPath  + 2) -  tmpLongNamePath);

      Dir->FATFirstCluster = parentDirectoryFirstCluster;
    }
}



/*
***********************************************************************************************************************
 *                           (PRIVATE) SET CURRENT DIRECTORY TO ONE OF ITS CHILD DIRECTORIES
 * 
 * Description : This function is called by FAT_SetDirectory() if that function has been asked to set an 
 *               instance of a Dir's struct members to a child Dir and a valid matching entryPos was 
 *               found. This function will only update the struct's members to that of the matching entryPos. It does
 *               not perform any of the search/compare required to find the match.
 * 
 * Argument    : *Dir     pointer to an instance of a FatDir struct whose members will be
 *                                     updated to point to the child Dir indicated by *nameStr.
 *             : *sector               pointer to an array that holds the physical sector's contents that contains the 
 *                                     short name of the entryPos that matches the *nameStr.
 *             : snPos     integer that specifies the first position in *sector where the 32-byte short
 *                                     name entryPos begins.
 *             : *nameStr              pointer to a C-string that specifies the longName member of the currenDirectory
 *                                     struct will be set to. If there is no long name associated with a short name
 *                                     then the longName and shortName members will both be set to the short name.  
 *             : *bpb                  pointer to the BPB struct instance.
***********************************************************************************************************************
*/

void
pvt_SetCurrentDirectoryToChild (FatDir * Dir, uint8_t *sector, uint16_t snPos, char * nameStr, BPB * bpb)
{
  uint32_t dirFirstCluster;
  dirFirstCluster = sector[snPos + 21];
  dirFirstCluster <<= 8;
  dirFirstCluster |= sector[snPos + 20];
  dirFirstCluster <<= 8;
  dirFirstCluster |= sector[snPos + 27];
  dirFirstCluster <<= 8;
  dirFirstCluster |= sector[snPos + 26];

  Dir->FATFirstCluster = dirFirstCluster;
  
  uint8_t snLen;
  if (strlen(nameStr) < 8) snLen = strlen(nameStr);
  else snLen = 8; 

  char sn[9];                                    
  for (uint8_t k = 0; k < snLen; k++)  
    sn[k] = sector[snPos + k];
  sn[snLen] = '\0';

  strcat (Dir->longParentPath,  Dir->longName );
  strcat (Dir->shortParentPath, Dir->shortName);

  // if current Dir is not root then append '/'
  if (Dir->longName[0] != '/') 
    strcat(Dir->longParentPath, "/"); 
  strcpy(Dir->longName, nameStr);
  
  if (Dir->shortName[0] != '/') 
    strcat(Dir->shortParentPath, "/");
  strcpy(Dir->shortName, sn);
}



/*
***********************************************************************************************************************
 *                                  (PRIVATE) LOAD A LONG NAME ENTRY INTO A C-STRING
 * 
 * Description : This function is called by any of the FAT functions that need to read in a long name from a FAT
 *               Dir into a C-string. This function is called twice if a long name crosses a sector boundary, and
 *               the *lnStrIndx will point to the position in the c-string to begin loading the string char's
 * 
 * Arguments   : longNameFirstEntry    integer that points to the position in *sector that is the lowest order entryPos
 *                                     of the long name in the sector array.
 *             : longNameLastEntry     integer that points to the position in *sector that is the highest order entryPos
 *                                     of the long name in sector array.
 *             : *sector               pointer to an array that holds the physical sector's contents containing the 
 *                                     the entries of the long name to load into the c-string.
 *             : *lnStr          pointer to a null terminated char array (C-string) that will be loaded with the
 *                                     long name characters from the sector.
 *             : *lnStrIndx     pointer to an integer that specifies the position in *lnStr where the
 *                                     first character will be loaded when this function is called. This function will
 *                                     update this value as the characters are loaded. If a subsequent call to this
 *                                     function is required in order to fully load the long name into the char array,
 *                                     as in the case when a single long name crosses a sector boundary, then this 
 *                                     value will be non-zero.
***********************************************************************************************************************
*/

void
pvt_LoadLongName (int longNameFirstEntry, int longNameLastEntry, uint8_t * sector, char * lnStr, uint8_t * lnStrIndx)
{
  for (int i = longNameFirstEntry; i >= longNameLastEntry; i = i - ENTRY_LEN)
    {                                              
      for (uint16_t n = i + 1; n < i + 11; n++)
        {                                  
          if (sector[n] == 0 || sector[n] > 126);
          else 
            { 
              lnStr[*lnStrIndx] = sector[n];
              (*lnStrIndx)++;  
            }
        }

      for (uint16_t n = i + 14; n < i + 26; n++)
        {                                  
          if (sector[n] == 0 || sector[n] > 126);
          else 
            { 
              lnStr[*lnStrIndx] = sector[n];
              (*lnStrIndx)++;  
            }
        }
      
      for (uint16_t n = i + 28; n < i + 32; n++)
        {                                  
          if (sector[n] == 0 || sector[n] > 126);
          else 
            { 
              lnStr[*lnStrIndx] = sector[n];  
              (*lnStrIndx)++;  
            }
        }        
    }
}



/*
***********************************************************************************************************************
 *                                        (PRIVATE) GET THE NEXT CLUSTER FAT INDEX
 *
 * Description : Used by the FAT functions to get the location of the next cluster in a Dir or file. The value  
 *               returned points to the cluster's index in the FAT. This value is offset by two when counting the 
 *               clusters in the FATs Data Region. Therefore, to get the cluster number in the data region the value
 *               returned by this function must be subtracted by 2.
 * 
 * Arguments   : *currentClusterIndex       a cluster's FAT index. The value at this index in the FAT is the index of the
 *                                     the next cluster of the file or Dir.
 *             : *bpb                  pointer to the BPB struct instance.
 * 
 * Returns     : The FAT index of the next cluster of a file or Dir. This is the value stored at the indexed
 *               location in the FAT specified by the value of currentClusterIndex's index. If a value of 0xFFFFFFFF then 
 *               currentClusterIndex is the last cluster of the file/Dir.
***********************************************************************************************************************
*/

uint32_t 
pvt_GetNextClusterIndex (uint32_t currentClusterIndex, BPB * bpb)
{
  uint8_t  bytesPerClusterIndex = 4; // for FAT32
  uint16_t numberOfIndexedClustersPerSectorOfFat = bpb->bytesPerSec / bytesPerClusterIndex; // = 128

  uint32_t clusIndx = currentClusterIndex / numberOfIndexedClustersPerSectorOfFat;
  uint32_t clusterIndexStartByte = 4 * (currentClusterIndex % numberOfIndexedClustersPerSectorOfFat);
  uint32_t cluster = 0;

  uint32_t fatSectorToRead = clusIndx + bpb->rsvdSecCnt;

  uint8_t SectorContents[bpb->bytesPerSec];

  fat_ReadSingleSector (fatSectorToRead, SectorContents);

  cluster = SectorContents[clusterIndexStartByte+3];
  cluster <<= 8;
  cluster |= SectorContents[clusterIndexStartByte+2];
  cluster <<= 8;
  cluster |= SectorContents[clusterIndexStartByte+1];
  cluster <<= 8;
  cluster |= SectorContents[clusterIndexStartByte];

  return cluster;
}



/*
***********************************************************************************************************************
 *                                        (PRIVATE) PRINTS THE FIELDS OF FAT ENTRY
 *
 * Description : Used by FAT_PrintCurrentDirectory() to print the fields associated with an entryPos, such as creation or last
 *               modified date/time, file size, etc... Which fields are printed is determined by the entryFilter flags
 * 
 * Arguments   : *sector          pointer to an array that holds the short name of the entryPos that is being printed
 *                                to the screen. Only the short name entryPos of a short name/long name combination holds
 *                                these fields.
 *             : entryPos            location in *sector of the first byte of the short name entryPos whose fields should be
 *                                printed to the screen.
 *             : entryFilter      byte of ENTRY_FLAGs, used to determine which fields of the entryPos will be printed.
***********************************************************************************************************************
*/

void 
pvt_PrintEntryFields (uint8_t *sector, uint16_t entryPos, uint8_t entryFilter)
{
  uint16_t creationTime;
  uint16_t creationDate;
  uint16_t lastAccessDate;
  uint16_t writeTime;
  uint16_t writeDate;
  uint32_t fileSize;

  if (CREATION & entryFilter)
    {
      creationTime = sector[entryPos + 15];
      creationTime <<= 8;
      creationTime |= sector[entryPos + 14];
      
      creationDate = sector[entryPos + 17];
      creationDate <<= 8;
      creationDate |= sector[entryPos + 16];
    }

  if (LAST_ACCESS & entryFilter)
    {
      lastAccessDate = sector[entryPos + 19];
      lastAccessDate <<= 8;
      lastAccessDate |= sector[entryPos + 18];
    }

  if (LAST_MODIFIED & entryFilter)
    {
      writeTime = sector[entryPos + 23];
      writeTime <<= 8;
      writeTime |= sector[entryPos + 22];

      writeDate = sector[entryPos + 25];
      writeDate <<= 8;
      writeDate |= sector[entryPos + 24];
    }

  fileSize = sector[entryPos + 31];
  fileSize <<= 8;
  fileSize |= sector[entryPos + 30];
  fileSize <<= 8;
  fileSize |= sector[entryPos + 29];
  fileSize <<= 8;
  fileSize |= sector[entryPos + 28];

  print_str ("\n\r");

  if (CREATION & entryFilter)
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

  if (LAST_ACCESS & entryFilter)
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


  if (LAST_MODIFIED & entryFilter)
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
       if ((fileSize / div) >= 10000000) { print_str(" "); print_dec(fileSize / div); }
  else if ((fileSize / div) >= 1000000) { print_str("  "); print_dec(fileSize / div); }
  else if ((fileSize / div) >= 100000) { print_str("   "); print_dec(fileSize / div); }
  else if ((fileSize / div) >= 10000) { print_str("    "); print_dec(fileSize / div); }
  else if ((fileSize / div) >= 1000) { print_str("     "); print_dec(fileSize / div); }
  else if ((fileSize / div) >= 100) { print_str("      "); print_dec(fileSize / div); }
  else if ((fileSize / div) >= 10) { print_str("       "); print_dec(fileSize / div); }
  else                             { print_str("        ");print_dec(fileSize / div);}        
  
  print_str("kB");

}



/*
***********************************************************************************************************************
 *                                              (PRIVATE) PRINT SHORT NAME
 *
 * Description : Used by FAT_PrintCurrentDirectory to the short name of an fat file or Dir.
 * 
 * Arguments   : *sector          pointer to an array that holds the short name of the entryPos that is being printed
 *                                to the screen. Only the short name entryPos of a short name/long name combination holds
 *                                these fields.
 *             : entryPos            location in *sector of the first byte of the short name entryPos whose fields should be
 *                                printed to the screen.
***********************************************************************************************************************
*/

void 
pvt_PrintShortNameAndType (uint8_t *sector, uint16_t entryPos)
{
  char sn[9];
  char ext[5];

  for (uint8_t k = 0; k < 8; k++)
    {
      sn[k] = ' ';
    }
  sn[8] = '\0';

  uint8_t attr = sector[entryPos + 11];
  if (attr & 0x10)
    {
      print_str ("    <DIR>    ");
      for (uint8_t k = 0; k < 8; k++) 
        {
          sn[k] = sector[entryPos + k];
        }
      print_str (sn);
      print_str ("    ");
    }

  else 
    {
      print_str ("   <FILE>    ");

      // re-initialize extension character array;
      strcpy (ext, ".   ");

      for (uint8_t k = 1; k < 4; k++) 
        {  
          ext[k] = sector[entryPos + 7 + k];  
        }

      for (uint8_t k = 0; k < 8; k++) 
        {
          sn[k] = sector[k + entryPos];
          if (sn[k] == ' ') 
            { 
              sn[k] = '\0'; 
              break; 
            };
        }

      print_str (sn);
      if (strcmp (ext, ".   ")) 
        {
          print_str (ext);
        }
      for (uint8_t p = 0; p < 10 - (strlen (sn) + 2); p++ ) 
        {
          print_str (" ");
        }
    }
}



/*
***********************************************************************************************************************
 *                                              (PRIVATE) PRINT A FAT FILE
 *
 * Description : Used by FAT_PrintFile() to actually print the fat file.
 * 
 * Arguments   : entryPos            location in *fileSector of the first byte of the short name entryPos of the file whose 
 *                                contents will be printed to the screen. This is required as the first cluster index 
 *                                of the file is located in the short name entryPos.
 *             : *fileSector      pointer to an array that holds the short name entryPos of the file to be printed to the 
 *                                to the screen.
***********************************************************************************************************************
*/

void 
pvt_PrintFatFile (uint16_t entryPos, uint8_t *fileSector, BPB * bpb)
  {
    uint32_t currSecNumPhys;
    uint32_t cluster;

    //get FAT index for file's first cluster
    cluster =  fileSector[entryPos + 21];
    cluster <<= 8;
    cluster |= fileSector[entryPos + 20];
    cluster <<= 8;
    cluster |= fileSector[entryPos + 27];
    cluster <<= 8;
    cluster |= fileSector[entryPos + 26];
    
    print_str("\n\n\r");
    // read in contents of file starting at relative sector 0 in 'cluster' and print contents to the screen.
    do
      {
        for(uint32_t currSecNumInClus = 0; currSecNumInClus < bpb->secPerClus; currSecNumInClus++) 
          {
            currSecNumPhys = currSecNumInClus + bpb->dataRegionFirstSector + ( (cluster - 2) * bpb->secPerClus );

            fat_ReadSingleSector (currSecNumPhys, fileSector);
            for (uint16_t k = 0; k < bpb->bytesPerSec; k++)  
              {
                if (fileSector[k] == '\n') 
                  {
                    print_str ("\n\r");
                  }
                else if (fileSector[k] == 0);
                else 
                  { 
                    USART_Transmit (fileSector[k]);
                  }
              }
          }
      } 
    while( ( (cluster = pvt_GetNextClusterIndex(cluster,bpb)) != END_OF_CLUSTER ) );
  }



/*
***********************************************************************************************************************
 *                         (PRIVATE) CORRECTS WHERE ENTRY IS POINTING AND RESEST LONG NAME FLAGS
 *
 * Description : Used by the FAT functions when searching through a Dir. A correction may need to be made so that
 *               entryPos is pointing to the correct location. This function uses the long name flag settings passed as
 *               arguments to determine if the adjustment needs to be made and makes the adjustment, if the correct
 *               entryPos is still in the current sector. If the correct entryPos is in the next sector, however, then the 
 *               function will exit with a value of 1 so that the next sector can be retrieved. Once the next sector 
 *               is retrieved, then this function is called again so that the entryPos correction can be made and the long 
 *               name flags.
 * 
 * Arguments   : *longNameExistsFlag                   if this flag is set then a long name existed for the previous 
 *                                                     entryPos that was checked. Reset by this function.
 *             : *longNameCrossSectorBoundaryFlag      if this flag is set then the long name of the previous entryPos 
 *                                                     crossed a sector boundary. Reset by this function.
 *             : *longNameLastSectorEntryFlag          if this flag is set then the long name of the previous entryPos was
 *                                                     entirely in the current sector, however, its corresponding short
 *                                                     name was in the next sector. Reset by this function
 *             : *entryPos                                pointer to an integer that specifies the location in the current
 *                                                     sector that will be checked/read in in the subsequent execution
 *                                                     of the calling function. This value will be updated by this
 *                                                     function if a correction to it is required. 
 *             : *snPosCurrSec     pointer to an integer that specifies the position of this 
 *                                                     variable that was set for the previous entryPos checked.
 *             : *snPosNextSec        pointer to an integer that specifies the position of this
 *                                                     variable, if it was set, for the previous entryPos checked.
 * 
 * Returns     : 1 if it is determined that the next entryPos to check is in the next sector.
 *             : 0 if the next entryPos to check is in the current sector.
***********************************************************************************************************************
*/

uint8_t 
pvt_CorrectEntryCheck (uint8_t  * lnFlags, uint16_t * entryPos, uint16_t * snPosCurrSec, uint16_t * snPosNextSec)
{  
  if ((*lnFlags) & LONG_NAME_EXISTS_FLAG)
    {
      if ( (*snPosCurrSec) >= (SECTOR_LEN - ENTRY_LEN))
        {
          if ((*entryPos) != 0)
            return 1; // need to get the next sector
          else 
            (*snPosCurrSec) = -ENTRY_LEN;
        }

      if ((*lnFlags) & (LONG_NAME_CROSSES_SECTOR_BOUNDARY_FLAG | LONG_NAME_LAST_SECTOR_ENTRY_FLAG))
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

// ************




/*
***********************************************************************************************************************
 *                                       (PRIVATE) SET THE LONG NAME FLAGS
 *
 * Description : Used by the FAT functions to set the long name flags, if it was determined that a long name exists, 
 *               for the current entryPos begin checked / read in. This also sets the snPosCurrSec.
 * 
 * Arguments   : *longNameExistsFlag                 - This flag is automatically set to 1 by this function to indicate
 *                                                     a long exists for this entryPos.
 *             : *longNameCrossSectorBoundaryFlag    - This flag will be set to 1 if the long name is found to cross a 
 *                                                     a sector boundary.
 *             : *longNameLastSectorEntryFlag        - This flag will be set to 1 if the long name is entirely in the 
 *                                                     current sector, but its short name is in the next sector.
 *             :  entryPos                              - Integer that specifies the location in the current
 *                                                     sector that will be checked/read in in the subsequent execution
 *                                                     of the calling function. This value will be updated by this
 *                                                     function if a correction to it is required. 
 *             : *snPosCurrSec   - This value is set by this function, and is a pointer to an 
 *                                                     integer that specifies the position of the short name relative 
 *                                                     to the first byte of the current sector. If this is greater than
 *                                                     512 then the short name is in the next sector.
 *             : *snPosNextSec      - Pointer to an integer that specifies the position of the short
 *                                                     name if it is determined to be in the next sector.
***********************************************************************************************************************
*/

void
pvt_SetLongNameFlags ( uint8_t  * lnFlags,
                          uint16_t   entryPos,
                          uint16_t * snPosCurrSec,
                          uint8_t  * currSecArr,
                          BPB * bpb
                     )
{
  *lnFlags |= LONG_NAME_EXISTS_FLAG;

  // number of entries required by the long name
  uint8_t longNameOrder = LONG_NAME_ORDINAL_MASK & currSecArr[entryPos]; 
                                  
  *snPosCurrSec = entryPos + (ENTRY_LEN * longNameOrder);
  
  // if the short name position is greater than 511 (bytePerSector-1) then the short name is in the next sector.
  if ((*snPosCurrSec) >= bpb->bytesPerSec)
    {
      if ((*snPosCurrSec) > bpb->bytesPerSec)
        {
          *lnFlags |= LONG_NAME_CROSSES_SECTOR_BOUNDARY_FLAG;
        }
      else if (*snPosCurrSec == SECTOR_LEN)
        {
          *lnFlags |= LONG_NAME_LAST_SECTOR_ENTRY_FLAG;
        }
    }
}   

/*
***********************************************************************************************************************
 *                             (PRIVATE) LOAD THE CONTENTS OF THE NEXT SECTOR INTO AN ARRAY
 *
 * Description : Used by the FAT functions to load the contents of the next file or Dir sector into the 
 *               *nextSecArr array if it is foudn that a long / short name combo crosses the sector boundary.
 * 
 * Arguments   : *nextSecArr                 - Pointer to an array that will be loaded with the contents of the
 *                                                     next sector of a file or Dir.
 *             : currSecNumInClus        - Integer that specifies the current sector number relative to the
 *                                                     current cluster. This value is used to determine if the next
 *                                                     sector is in the current or the next cluster.
 *             : currSecNumPhys         - This is the the current sector's physical sector number on the
 *                                                     disk hosting the FAT volume.
 *             : clusIndx                       - This is the current cluster's FAT index. This is required if it
 *                                                     is determined that the next sector is in the next cluster.   
 *             : *bpb                                - pointer to the BPB struct instance.
***********************************************************************************************************************
*/

void
pvt_GetNextSector (uint8_t* nextSecArr, uint32_t currSecNumInClus, uint32_t currSecNumPhys, uint32_t clusIndx,  BPB* bpb)
{
  uint32_t absoluteNextSectorNumber; 
  
  if (currSecNumInClus >= (bpb->secPerClus - 1)) 
    absoluteNextSectorNumber = bpb->dataRegionFirstSector + ((pvt_GetNextClusterIndex (clusIndx, bpb) - 2) * bpb->secPerClus);
  else 
    absoluteNextSectorNumber = 1 + currSecNumPhys;

  fat_ReadSingleSector (absoluteNextSectorNumber, nextSecArr);
}
