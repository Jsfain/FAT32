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

uint8_t pvt_CorrectEntryCheckAndLongNameFlagReset (uint8_t * longNameFlags, uint16_t * entry, uint16_t * shortNamePositionInCurrentSector, uint16_t * shortNamePositionInNextSector);
void pvt_SetLongNameFlags (uint8_t * longNameFlags, uint16_t entry, uint16_t * shortNamePositionInCurrentSector, uint8_t  * currentSectorContents, BPB * bpb);
void pvt_LoadLongName (int longNameFirstEntry, int longNameLastEntry, uint8_t *sector, char * longNameStr, uint8_t * longNameStrIndex);
void pvt_GetNextSector (uint8_t * nextSectorContents, uint32_t currentSectorNumberInCluster, uint32_t absoluteCurrentSectorNumber, uint32_t clusterIndex,  BPB * bpb);
uint32_t pvt_GetNextClusterIndex (uint32_t currentClusterIndex, BPB * bpb);
uint8_t pvt_CheckLegalName (char * nameStr);
void pvt_SetCurrentDirectoryToParent (FatDir * Dir, BPB * bpb);
void pvt_SetCurrentDirectoryToChild (FatDir * Dir, uint8_t *sector, uint16_t shortNamePosition, char * nameStr, BPB * bpb);
void pvt_PrintEntryFields (uint8_t *byte, uint16_t entry, uint8_t entryFilter);
void pvt_PrintShortNameAndType (uint8_t *byte, uint16_t entry);
void pvt_PrintFatFile (uint16_t entry, uint8_t *fileSector, BPB * bpb);



/*
***********************************************************************************************************************
 *                                            "PUBLIC" FUNCTION DEFINITIONS
***********************************************************************************************************************
*/


/*
***********************************************************************************************************************
 *                                   SET MEMBERS OF THE BIOS PARAMETER BLOCK STRUCT
 * 
 * An instantiated and valid BPB struct is a required argument of any function that accesses the FAT 
 * volume, therefore this function should be called first before implementing any other parts of this FAT module.
 *                                         
 * Description : This function will set the members of a BPB struct instance according to the values
 *               specified within the FAT volume's Bios Parameter Block / Boot Sector. 
 * 
 * Argument    : *bpb    pointer to an instance of a BPB struct.
 * 
 * Return      : Boot sector error flag     See the FAT.H header file for a list of these flags. To print the returned
 *                                          value, pass it to FAT_PrintBootSectorError(err). If the BPB
 *                                          struct's members have been successfully set then BOOT_SECTOR_VALID is
 *                                          returned. Any other returned value indicates a failure to set the BPB. 
 * 
 * Note        : This function DOES NOT set the values a physical FAT volume's Bios Parameter Block as would be 
 *               required during formatting of a FAT volume. This module can only read a FAT volume's contents and does
 *               not have the capability to modify anything on the volume, this includes formatting a FAT volume.
***********************************************************************************************************************
*/

uint8_t 
FAT_SetBiosParameterBlock (BPB * bpb)
{
  uint8_t BootSector[SECTOR_LEN];

  bpb->bootSectorAddress = fat_FindBootSector();
  
  if (bpb->bootSectorAddress != 0xFFFFFFFF)
    {
      fat_ReadSingleSector (bpb->bootSectorAddress, BootSector);
    }
  else
    {
      return BOOT_SECTOR_NOT_FOUND;
    }

  // last two bytes of Boot Sector should be signature bytes.
  if ((BootSector[SECTOR_LEN - 2] == 0x55) && (BootSector[SECTOR_LEN - 1] == 0xAA))
    {
      bpb->bytesPerSector = BootSector[12];
      bpb->bytesPerSector <<= 8;
      bpb->bytesPerSector |= BootSector[11];
      
      if (bpb->bytesPerSector != SECTOR_LEN) 
        {
          return INVALID_BYTES_PER_SECTOR;
        }

      // secPerClus
      bpb->sectorsPerCluster = BootSector[13];

      if ((    bpb->sectorsPerCluster != 1 ) && (bpb->sectorsPerCluster != 2  ) && (bpb->sectorsPerCluster != 4 )
           && (bpb->sectorsPerCluster != 8 ) && (bpb->sectorsPerCluster != 16 ) && (bpb->sectorsPerCluster != 32)
           && (bpb->sectorsPerCluster != 64) && (bpb->sectorsPerCluster != 128))
        {
          return INVALID_SECTORS_PER_CLUSTER;
        }

      bpb->reservedSectorCount = BootSector[15];
      bpb->reservedSectorCount <<= 8;
      bpb->reservedSectorCount |= BootSector[14];

      bpb->numberOfFats = BootSector[16];

      bpb->fatSize32 =  BootSector[39];
      bpb->fatSize32 <<= 8;
      bpb->fatSize32 |= BootSector[38];
      bpb->fatSize32 <<= 8;
      bpb->fatSize32 |= BootSector[37];
      bpb->fatSize32 <<= 8;
      bpb->fatSize32 |= BootSector[36];

      bpb->rootCluster =  BootSector[47];
      bpb->rootCluster <<= 8;
      bpb->rootCluster |= BootSector[46];
      bpb->rootCluster <<= 8;
      bpb->rootCluster |= BootSector[45];
      bpb->rootCluster <<= 8;
      bpb->rootCluster |= BootSector[44];

      bpb->dataRegionFirstSector = bpb->bootSectorAddress + bpb->reservedSectorCount + (bpb->numberOfFats * bpb->fatSize32);
    }
  else 
    return NOT_BOOT_SECTOR;

  return BOOT_SECTOR_VALID;
}



/*
***********************************************************************************************************************
 *                                        PRINT ERROR RETURNED BY FAT_SetBiosParameterBlock
 * 
 * Description : Call this function to print an error flag returned by the function FAT_SetBiosParameterBlock(). 
 * 
 * Argument    : err    This should be an error flag returned the function FAT_SetBiosParameterBlock(). If it is not 
 *                      then the output is meaningless.
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
 *                                                 SETS A DIRECTORY
 *                                        
 * Description : Call this function to set a new current Dir. This operates by searching the current Dir, 
 *               pointed to by the Dir struct, for a name that matches newDirectoryStr. If a match is
 *               found the members of the Dir struct are updated to those corresponding to the matching 
 *               newDirectoryStr entry.
 *
 * Arguments   : *Dir   pointer to a FatDir struct whose members must point to a valid
 *                                   FAT32 Dir.
 *             : *newDirectoryStr    pointer to a C-string that is the name of the intended new Dir. The function
 *                                   takes this and searches the current Dir for a matching name. This string
 *                                   must be a long name unless a long name does not exist for a given entry. Only then
 *                                   will a short name be searched.
 *             : *bpb                pointer to a BPB struct.
 * 
 * Return      : FAT Error Flag      The returned value can be read by passing it to FAT_PrintError(ErrorFlag). If 
 *                                   SUCCESS is returned then the Dir struct members were successfully 
 *                                   updated, but any other returned value indicates a failure struct members will not
 *                                   have been modified updated.
 *  
 * Limitations : This function will not work with absolute paths, it will only set a new Dir that is a child or
 *               the parent of the current Dir. 
***********************************************************************************************************************
*/

uint16_t 
FAT_SetDirectory (FatDir * Dir, char * newDirectoryStr, BPB * bpb)
{
  if (pvt_CheckLegalName (newDirectoryStr)) 
    return INVALID_DIR_NAME;

  // newDirStr == 'Current Directory' ?
  if (!strcmp (newDirectoryStr,  ".")) 
    return SUCCESS;
  
  // newDirStr == 'Parent Directory' ?
  if (!strcmp (newDirectoryStr, ".."))
    {
      pvt_SetCurrentDirectoryToParent (Dir, bpb);
      return SUCCESS;
    }

  uint8_t  newDirStrLen = strlen (newDirectoryStr);
  uint32_t clusterIndex = Dir->FATFirstCluster;
  uint8_t  currentSectorContents[ bpb->bytesPerSector ]; 
  uint32_t absoluteCurrentSectorNumber;
  uint16_t shortNamePositionInCurrentSector = 0;
  uint8_t  nextSectorContents[ bpb->bytesPerSector ];
  uint16_t shortNamePositionInNextSector = 0;
  uint8_t  attributeByte;
  char     longNameStr[ LONG_NAME_LEN_MAX ];
  uint8_t  longNameStrIndex = 0;
  uint8_t  longNameFlags = 0;
  uint8_t  entryCorrectionFlag = 0;

  // ***    Search Dir entries of the current Dir for match to newDirectoryStr and print if found    *** /
  // loop through the current Dir's clusters
  do
    {
      // loop through sectors in the current cluster
      for (uint32_t currentSectorNumberInCluster = 0; currentSectorNumberInCluster < bpb->sectorsPerCluster; currentSectorNumberInCluster++)
        {         
          // load sector bytes into currentSectorContents[]
          absoluteCurrentSectorNumber = currentSectorNumberInCluster + bpb->dataRegionFirstSector + ((clusterIndex - 2) * bpb->sectorsPerCluster);
          fat_ReadSingleSector (absoluteCurrentSectorNumber, currentSectorContents);

          // loop through entries in the current sector.
          for (uint16_t entry = 0; entry < SECTOR_LEN; entry = entry + ENTRY_LEN)
            {
              entryCorrectionFlag = pvt_CorrectEntryCheckAndLongNameFlagReset ( &longNameFlags, &entry, &shortNamePositionInCurrentSector, &shortNamePositionInNextSector);
              if (entryCorrectionFlag == 1)
                {
                  entryCorrectionFlag = 0;
                  break;    
                }
              // If first value of entry is 0 then all subsequent entries are empty.
              if (currentSectorContents[ entry ] == 0)
                { /*print_str("\n\r entry = "); print_dec(entry);*/ return END_OF_DIRECTORY; }

              // entry is marked for deletion. Do nothing.
              if (currentSectorContents[ entry ] != 0xE5)
                {                 
                  attributeByte = currentSectorContents[ entry + 11 ];
                  
                  // if entry being checked is a long name entry
                  if ((attributeByte & LONG_NAME_ATTR_MASK) == LONG_NAME_ATTR_MASK)
                    {
                      if ( !(currentSectorContents[ entry ] & LONG_NAME_LAST_ENTRY_FLAG))
                        return CORRUPT_FAT_ENTRY;
                      
                      longNameStrIndex = 0;

                      for (uint8_t k = 0; k < LONG_NAME_LEN_MAX; k++) 
                        longNameStr[k] = '\0';

                      pvt_SetLongNameFlags ( &longNameFlags, entry, &shortNamePositionInCurrentSector, currentSectorContents, bpb);
                      if (longNameFlags & (LONG_NAME_CROSSES_SECTOR_BOUNDARY_FLAG | LONG_NAME_LAST_SECTOR_ENTRY_FLAG))
                        {
                          pvt_GetNextSector (nextSectorContents, currentSectorNumberInCluster, absoluteCurrentSectorNumber, clusterIndex, bpb);
                          shortNamePositionInNextSector = (shortNamePositionInCurrentSector) - bpb->bytesPerSector;
                          attributeByte = nextSectorContents[ shortNamePositionInNextSector + 11 ];

                          // If shortNamePositionInNextSector points to long name entry then something is wrong.
                          if ((attributeByte & LONG_NAME_ATTR_MASK) == LONG_NAME_ATTR_MASK) 
                            return CORRUPT_FAT_ENTRY;

                          // only need to proceed if the short name attribute byte indicates the entry is a Dir
                          if (attributeByte & DIRECTORY_ENTRY_ATTR_FLAG)
                            {                                                           
                              // If long name crosses sector boundary
                              if (longNameFlags & LONG_NAME_CROSSES_SECTOR_BOUNDARY_FLAG)
                                {
                                  if ((nextSectorContents[ shortNamePositionInNextSector - ENTRY_LEN ] & LONG_NAME_ORDINAL_MASK) != 1)
                                    return CORRUPT_FAT_ENTRY;

                                  // load long name entry that crosses sector boundary into longNameStr[]
                                  pvt_LoadLongName (shortNamePositionInNextSector - ENTRY_LEN, 0, nextSectorContents, longNameStr, &longNameStrIndex);
                                  pvt_LoadLongName (SECTOR_LEN - ENTRY_LEN, (int)entry, currentSectorContents, longNameStr, &longNameStrIndex);
                                  if (!strcmp (newDirectoryStr, longNameStr)) 
                                    {                                                        
                                      pvt_SetCurrentDirectoryToChild (Dir, nextSectorContents, shortNamePositionInNextSector, newDirectoryStr, bpb);
                                      return SUCCESS;
                                    }
                                }

                              // All entries for long name are in current sector but short name is in next sector
                              else if (longNameFlags & LONG_NAME_LAST_SECTOR_ENTRY_FLAG)
                               {
                                  // confirm last entry of current sector is the first entry of the long name
                                  if ((currentSectorContents[ SECTOR_LEN - ENTRY_LEN ] & LONG_NAME_ORDINAL_MASK) != 1)
                                    return CORRUPT_FAT_ENTRY;

                                  // load long name entry into longNameStr[]
                                  pvt_LoadLongName(SECTOR_LEN - ENTRY_LEN, (int)entry, currentSectorContents, longNameStr, &longNameStrIndex);
                                  if (!strcmp (newDirectoryStr, longNameStr)) 
                                    { 
                                      pvt_SetCurrentDirectoryToChild (Dir, nextSectorContents, shortNamePositionInNextSector, newDirectoryStr, bpb);
                                      return SUCCESS;
                                    }
                                }
                              else
                                return CORRUPT_FAT_ENTRY;
                            }
                        }

                      // Long name exists and is entirely in current sector along with the short name
                      else
                        {   
                          // Confirm entry preceding short name is first entry of a long name.
                          if ((currentSectorContents[ shortNamePositionInCurrentSector - ENTRY_LEN ] & LONG_NAME_ORDINAL_MASK) != 1)                            
                            return CORRUPT_FAT_ENTRY;
        
                          attributeByte = currentSectorContents[ shortNamePositionInCurrentSector + 11 ];
                          // If not a Dir entry, move on to next entry.
                          if ((attributeByte & DIRECTORY_ENTRY_ATTR_FLAG))
                            {
                              // load long name entry into longNameStr[]
                              pvt_LoadLongName (shortNamePositionInCurrentSector - ENTRY_LEN, (int)entry, currentSectorContents, longNameStr, &longNameStrIndex);
                              if  (!strcmp (newDirectoryStr, longNameStr)) 
                                { 
                                  pvt_SetCurrentDirectoryToChild (Dir, currentSectorContents, shortNamePositionInCurrentSector, newDirectoryStr, bpb);
                                  return SUCCESS;
                                }                                       
                            }
                        }                      
                    }                   

                  // Long Name Entry does not exist
                  else  
                    {
                      attributeByte = currentSectorContents[entry + 11];

                      // If not a Dir entry OR newDirectoryStr is too long for a short name then skip this section.
                      if ((newDirStrLen < 9) && (attributeByte & DIRECTORY_ENTRY_ATTR_FLAG) )
                        {                   
                          char sn[ 9 ];
                          char tempDir[ 9 ];
                          strcpy(tempDir, newDirectoryStr);

                          for (uint8_t k = 0; k < newDirStrLen; k++) 
                            sn[ k ] = currentSectorContents[ k + entry ];
                          sn[ newDirStrLen ] = '\0';
                          if (!strcmp (tempDir, sn))
                            { 
                              pvt_SetCurrentDirectoryToChild (Dir, currentSectorContents, entry, newDirectoryStr, bpb);
                              return SUCCESS;
                            }
                        }
                    }              
                }
            }
        }
    } 
  while ((clusterIndex = pvt_GetNextClusterIndex(clusterIndex, bpb)) != END_OF_CLUSTER);
  
  return END_OF_DIRECTORY;
}



/*
***********************************************************************************************************************
 *                               PRINT THE ENTRIES OF THE CURRENT DIRECTORY TO A SCREEN
 * 
 * Description : Prints a list of entries (files and directories) contained in the current Dir. Which entries and
 *               which associated data (hidden files, creation date, ...) are indicated by the ENTRY_FLAG. See the 
 *               specific ENTRY_FLAGs that can be passed in the FAT.H header file.
 * 
 * Arguments   : *Dir   pointer to a FatDir struct whose members must be associated with a 
 *                                   valid FAT32 Dir.
 *             : entryFilter         byte of ENTRY_FLAGs, used to determine which entries will be printed. Any 
 *                                   combination of flags can be set. If neither LONG_NAME or SHORT_NAME are passed 
 *                                   then no entries will be printed.
 *             : *bpb                pointer to a BPB struct.
 * 
 * Return      : FAT Error Flag     Returns END_OF_DIRECTORY if the function completed successfully and was able to
 *                                  read in and print entries until it reached the end of the Dir. Any other 
 *                                  value returned indicates an error. Pass the value to FAT_PrintError(ErrorFlag). 
***********************************************************************************************************************
*/

uint16_t 
FAT_PrintCurrentDirectory (FatDir * Dir, uint8_t entryFilter, BPB * bpb)
{
  uint32_t clusterIndex = Dir->FATFirstCluster;
  uint8_t  currentSectorContents[ bpb->bytesPerSector ];
  uint32_t absoluteCurrentSectorNumber;
  uint16_t shortNamePositionInCurrentSector = 0;
  uint8_t  nextSectorContents[ bpb->bytesPerSector ];
  uint16_t shortNamePositionInNextSector = 0;
  uint8_t  attributeByte;
  char     longNameStr[ LONG_NAME_LEN_MAX ];
  uint8_t  longNameStrIndex = 0;
  uint8_t longNameFlags = 0;
  uint8_t   entryCorrectionFlag = 0;

  // Prints column headers according to entryFilter
  print_str("\n\n\r");
  
  if (CREATION & entryFilter)
    print_str(" CREATION DATE & TIME,");
  if (LAST_ACCESS & entryFilter)  
    print_str(" LAST ACCESS DATE,");
  if (LAST_MODIFIED & entryFilter)  
    print_str(" LAST MODIFIED DATE & TIME,");
  
  print_str(" SIZE, TYPE, NAME");
  print_str("\n\n\r");
  

  // loop through the current Dir's clusters
  do 
    {
      // loop through sectors in the current cluster
      for (uint32_t currentSectorNumberInCluster = 0; currentSectorNumberInCluster < bpb->sectorsPerCluster; currentSectorNumberInCluster++)
        {
          // load sector bytes into currentSectorContents[]
          absoluteCurrentSectorNumber = currentSectorNumberInCluster + bpb->dataRegionFirstSector + ((clusterIndex - 2) * bpb->sectorsPerCluster);
          fat_ReadSingleSector (absoluteCurrentSectorNumber, currentSectorContents);

          // loop through entries in the current sector.
          for (uint16_t entry = 0; entry < bpb->bytesPerSector; entry = entry + ENTRY_LEN)
            {
              entryCorrectionFlag = pvt_CorrectEntryCheckAndLongNameFlagReset ( &longNameFlags, &entry, &shortNamePositionInCurrentSector, &shortNamePositionInNextSector);
              if (entryCorrectionFlag == 1)
                {
                  entryCorrectionFlag = 0;
                  break;    
                }

              // If first value of entry is 0 then all subsequent entries are empty.
              if (currentSectorContents[ entry ] == 0) 
                return END_OF_DIRECTORY;

              // entry is marked for deletion. Do nothing.
              if (currentSectorContents[ entry ] != 0xE5)
                {
                  attributeByte = currentSectorContents[ entry + 11 ];
                  
                  // if entry being checked is a long name entry
                  if ((attributeByte & LONG_NAME_ATTR_MASK) == LONG_NAME_ATTR_MASK)
                    {
                      if ( !(currentSectorContents[ entry ] & LONG_NAME_LAST_ENTRY_FLAG))
                        return CORRUPT_FAT_ENTRY;
                      
                      longNameStrIndex = 0;

                      for (uint8_t k = 0; k < LONG_NAME_LEN_MAX; k++) 
                        longNameStr[k] = '\0';
 
                      pvt_SetLongNameFlags ( &longNameFlags, entry, &shortNamePositionInCurrentSector, currentSectorContents, bpb);

                      if (longNameFlags & (LONG_NAME_CROSSES_SECTOR_BOUNDARY_FLAG | LONG_NAME_LAST_SECTOR_ENTRY_FLAG))
                        {
                          pvt_GetNextSector ( nextSectorContents, currentSectorNumberInCluster, absoluteCurrentSectorNumber, clusterIndex, bpb );
                          shortNamePositionInNextSector = (shortNamePositionInCurrentSector) - bpb->bytesPerSector;
                          attributeByte = nextSectorContents[ (shortNamePositionInNextSector) + 11 ];
                          
                          // if shortNamePositionInNextSector points to a long name entry then something is wrong.
                          if ((attributeByte & LONG_NAME_ATTR_MASK) == LONG_NAME_ATTR_MASK) 
                            return CORRUPT_FAT_ENTRY;
                            
                          // print if hidden attribute flag is not set OR if it is set AND the hidden filter flag is set.
                          if ( (!(attributeByte & HIDDEN_ATTR_FLAG)) || ((attributeByte & HIDDEN_ATTR_FLAG) && (entryFilter & HIDDEN)))
                            {                                                           
                              if (entryFilter & SHORT_NAME)
                                {
                                  pvt_PrintEntryFields(nextSectorContents, shortNamePositionInNextSector, entryFilter);
                                  pvt_PrintShortNameAndType(nextSectorContents, shortNamePositionInNextSector);
                                }

                              if (entryFilter & LONG_NAME)
                                {
                                  // entries for long name cross sector boundary
                                  if (longNameFlags & LONG_NAME_CROSSES_SECTOR_BOUNDARY_FLAG)
                                    {
                                      // Confirm entry preceding short name is first entry of a long name.
                                      if ((nextSectorContents[ shortNamePositionInNextSector - ENTRY_LEN ] & LONG_NAME_ORDINAL_MASK) != 1) 
                                        return CORRUPT_FAT_ENTRY;                                              

                                      pvt_PrintEntryFields (nextSectorContents, shortNamePositionInNextSector, entryFilter);

                                      if (attributeByte & DIRECTORY_ENTRY_ATTR_FLAG) 
                                        print_str("    <DIR>    ");
                                      else 
                                        print_str("   <FILE>    ");
                                      
                                      // load long name entry into longNameStr[]
                                      pvt_LoadLongName (shortNamePositionInNextSector - ENTRY_LEN, 0, nextSectorContents, longNameStr, &longNameStrIndex);
                                      pvt_LoadLongName (SECTOR_LEN - ENTRY_LEN, (int)entry, currentSectorContents, longNameStr, &longNameStrIndex);

                                      print_str(longNameStr);
                                    }

                                  // all entries for long name are in current sector but short name is in next sector
                                  else if (longNameFlags & LONG_NAME_LAST_SECTOR_ENTRY_FLAG)
                                    {
                                      // confirm last entry of current sector is the first entry of the long name
                                      if ((currentSectorContents[ SECTOR_LEN - ENTRY_LEN] & LONG_NAME_ORDINAL_MASK) != 1) 
                                        return CORRUPT_FAT_ENTRY;
                            
                                      pvt_PrintEntryFields (nextSectorContents, shortNamePositionInNextSector, entryFilter);

                                      if (attributeByte & DIRECTORY_ENTRY_ATTR_FLAG) 
                                        print_str("    <DIR>    ");
                                      else 
                                        print_str("   <FILE>    ");
                                      
                                      // load long name entry into longNameStr[]
                                      pvt_LoadLongName (SECTOR_LEN - ENTRY_LEN, (int)entry, currentSectorContents, longNameStr, &longNameStrIndex);

                                      print_str(longNameStr); 
                                    }
                                  else 
                                    { print_str("\n\r this one?"); return CORRUPT_FAT_ENTRY;}
                                }
                            }
                        }
                      else // Long name exists and is entirely in current sector along with the short name
                        {   
                          attributeByte = currentSectorContents[ shortNamePositionInCurrentSector + 11 ];
                          // shortNamePositionInCurrentSector points to long name entry
                          if ((attributeByte & LONG_NAME_ATTR_MASK) == LONG_NAME_ATTR_MASK) 
                            return CORRUPT_FAT_ENTRY;

                          if ( (!(attributeByte & HIDDEN_ATTR_FLAG)) || ((attributeByte & HIDDEN_ATTR_FLAG) && (entryFilter & HIDDEN)))
                            {                   
                              if (entryFilter & SHORT_NAME)
                                {
                                  pvt_PrintEntryFields (currentSectorContents, shortNamePositionInCurrentSector, entryFilter);
                                  pvt_PrintShortNameAndType (currentSectorContents, shortNamePositionInCurrentSector);
                                }
                              
                              if (entryFilter & LONG_NAME)
                                {
                                  // Confirm entry preceding short name is first entry of a long name.
                                  if ((currentSectorContents[ shortNamePositionInCurrentSector - ENTRY_LEN ] & LONG_NAME_ORDINAL_MASK) != 1) 
                                    return CORRUPT_FAT_ENTRY;

                                  pvt_PrintEntryFields(currentSectorContents, shortNamePositionInCurrentSector, entryFilter);
                                  
                                  if(attributeByte & DIRECTORY_ENTRY_ATTR_FLAG) 
                                    print_str("    <DIR>    ");
                                  else 
                                    print_str("   <FILE>    ");

                                  // load long name entry into longNameStr[]
                                  pvt_LoadLongName (shortNamePositionInCurrentSector - ENTRY_LEN, (int)entry, currentSectorContents, longNameStr, &longNameStrIndex);

                                  print_str(longNameStr);                                       
                                }
                            }
                        }                   
                    }
                  else  // Long Name Entry does not exist so use short name instead, regardless of SHORT_NAME entryFilter setting
                    {
                      attributeByte = currentSectorContents[entry + 11];
                      if ( (!(attributeByte & HIDDEN_ATTR_FLAG)) || ((attributeByte & HIDDEN_ATTR_FLAG) && (entryFilter & HIDDEN)))
                        {
                          pvt_PrintEntryFields(currentSectorContents, entry, entryFilter);
                          pvt_PrintShortNameAndType(currentSectorContents, entry);
                        }
                    }
                }          
            }
        }
    }
  while ((clusterIndex = pvt_GetNextClusterIndex( clusterIndex, bpb )) != END_OF_CLUSTER);
  // END: Print entries in the current Dir

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
 *                                   must be a long name, unless there is no associated long name with an entry, in 
 *                                   which case it can be a short name.
 *             : *bpb                pointer to a BPB struct.
 * 
 * Return      : FAT Error Flag     Returns END_OF_FILE if the function completed successfully and was able to
 *                                  read in and print a file's contents to the screen. Any other value returned 
 *                                  indicates an error. Pass the returned value to FAT_PrintError(ErrorFlag).
***********************************************************************************************************************
*/

uint16_t 
FAT_PrintFile (FatDir * Dir, char * fileNameStr, BPB * bpb)
{
  if (pvt_CheckLegalName (fileNameStr)) 
    return INVALID_DIR_NAME;
  

  uint8_t fileNameStrLen = strlen(fileNameStr);
  uint32_t clusterIndex = Dir->FATFirstCluster;
  uint8_t  currentSectorContents[ bpb->bytesPerSector ];
  uint32_t absoluteCurrentSectorNumber;
  uint16_t shortNamePositionInCurrentSector = 0;
  uint8_t  nextSectorContents[ bpb->bytesPerSector ];
  uint16_t shortNamePositionInNextSector = 0;
  uint8_t  attributeByte;
  char     longNameStr[LONG_NAME_LEN_MAX];
  uint8_t  longNameStrIndex = 0;
  uint8_t longNameFlags = 0;
  uint8_t entryCorrectionFlag = 0;

  // ***     Search files in current Dir for match to fileNameStr and print if found    *** /
  
  // loop through the current Dir's clusters
  int clusCnt = 0;    
  do
    {
      clusCnt++;
      // loop through sectors in current cluster.
      for (uint32_t currentSectorNumberInCluster = 0; currentSectorNumberInCluster < bpb->sectorsPerCluster; currentSectorNumberInCluster++)
        {     
          // load sector bytes into currentSectorContents[]
          absoluteCurrentSectorNumber = currentSectorNumberInCluster + bpb->dataRegionFirstSector + ((clusterIndex - 2) * bpb->sectorsPerCluster);
          fat_ReadSingleSector (absoluteCurrentSectorNumber, currentSectorContents );
          
          // loop through entries in the current sector.
          for (uint16_t entry = 0; entry < bpb->bytesPerSector; entry = entry + ENTRY_LEN)
            { 
              entryCorrectionFlag = pvt_CorrectEntryCheckAndLongNameFlagReset ( &longNameFlags, &entry, &shortNamePositionInCurrentSector, &shortNamePositionInNextSector);
              if (entryCorrectionFlag == 1)
                {
                  entryCorrectionFlag = 0;
                  break;    
                }
              
              // If first value of entry is 0 then all subsequent entries are empty.
              if (currentSectorContents[ entry ] == 0) 
                return END_OF_DIRECTORY;

              // entry is marked for deletion. Do nothing.
              if (currentSectorContents[ entry ] != 0xE5)
                {
                  attributeByte = currentSectorContents[ (entry) + 11 ];
                  
                  // if entry being checked is a long name entry
                  if ((attributeByte & LONG_NAME_ATTR_MASK) == LONG_NAME_ATTR_MASK)
                    {
                      if ( !(currentSectorContents[ entry ] & LONG_NAME_LAST_ENTRY_FLAG))
                        return CORRUPT_FAT_ENTRY;
                      
                      longNameStrIndex = 0;

                      for (uint8_t k = 0; k < LONG_NAME_LEN_MAX; k++) 
                        longNameStr[k] = '\0';

                      pvt_SetLongNameFlags ( &longNameFlags, entry, &shortNamePositionInCurrentSector, currentSectorContents, bpb);
                      if (longNameFlags & (LONG_NAME_CROSSES_SECTOR_BOUNDARY_FLAG | LONG_NAME_LAST_SECTOR_ENTRY_FLAG))
                        {
                          pvt_GetNextSector (nextSectorContents, currentSectorNumberInCluster, absoluteCurrentSectorNumber, clusterIndex, bpb );
                          shortNamePositionInNextSector = shortNamePositionInCurrentSector - bpb->bytesPerSector;
                          attributeByte = nextSectorContents[ shortNamePositionInNextSector + 11 ];
                          
                          // If shortNamePositionInNextSector points to long name entry then something is wrong.
                          if ((attributeByte & LONG_NAME_ATTR_MASK) == LONG_NAME_ATTR_MASK) 
                            return CORRUPT_FAT_ENTRY;

                          // Only proceed if entry points to a file (i.e. Dir flag is not set)
                          if ( !(attributeByte & DIRECTORY_ENTRY_ATTR_FLAG))
                            {                                                           
                              if (longNameFlags & LONG_NAME_CROSSES_SECTOR_BOUNDARY_FLAG)
                                {
                                  // if entry immediately before the short name entry is not the ORDER = 1 entry of a long name then something is wrong
                                  if ((nextSectorContents[ shortNamePositionInNextSector - ENTRY_LEN ] & LONG_NAME_ORDINAL_MASK) != 1) 
                                    return CORRUPT_FAT_ENTRY; 
                                  
                                  // load long name entry into longNameStr[]
                                  pvt_LoadLongName (shortNamePositionInNextSector - ENTRY_LEN, 0, nextSectorContents, longNameStr, &longNameStrIndex);
                                  pvt_LoadLongName (SECTOR_LEN - ENTRY_LEN, (int)entry, currentSectorContents, longNameStr, &longNameStrIndex);
                                  
                                  // print file contents if a matching entry was found
                                  if(!strcmp (fileNameStr,longNameStr))
                                    { 
                                      pvt_PrintFatFile (shortNamePositionInNextSector, nextSectorContents, bpb); 
                                      return END_OF_FILE;
                                    }
                                }

                              // Long name is entirely in the current sector, but its short name is the first entry of the next sector
                              else if (longNameFlags & LONG_NAME_LAST_SECTOR_ENTRY_FLAG)
                                {
                                  longNameStrIndex = 0;

                                  // confirm last entry of current sector is the first entry of a long name
                                  if( (currentSectorContents[ SECTOR_LEN - ENTRY_LEN ] & LONG_NAME_ORDINAL_MASK) != 1 ) 
                                    return CORRUPT_FAT_ENTRY;                                                                  
                                      
                                  // read long name into longNameStr
                                  pvt_LoadLongName (SECTOR_LEN - ENTRY_LEN, (int)entry, currentSectorContents, longNameStr, &longNameStrIndex);

                                  // print file contents if a matching entry was found
                                  if ( !strcmp(fileNameStr, longNameStr))
                                    { 
                                      pvt_PrintFatFile (shortNamePositionInNextSector, nextSectorContents, bpb); 
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
                          attributeByte = currentSectorContents[shortNamePositionInCurrentSector+11];
                          
                          // confirm shortNamePositionInCurrentSector points to a short name entry in the current sector
                          if ((attributeByte & LONG_NAME_ATTR_MASK) == LONG_NAME_ATTR_MASK) 
                            return CORRUPT_FAT_ENTRY;
                          
                          // proceed if entry is a file
                          if ( !(attributeByte & DIRECTORY_ENTRY_ATTR_FLAG))
                            {                   
                              // confirm entry immediatedly preceding the short name entry the first entry of a long name
                              if ((currentSectorContents[ shortNamePositionInCurrentSector - ENTRY_LEN ] & LONG_NAME_ORDINAL_MASK ) != 1 ) 
                                return CORRUPT_FAT_ENTRY;

                              // read long name into longNameStr
                              pvt_LoadLongName (shortNamePositionInCurrentSector - ENTRY_LEN, (int)entry, currentSectorContents, longNameStr, &longNameStrIndex);

                              // print file contents if a matching entry was found
                              if ( !strcmp (fileNameStr, longNameStr))
                                { 
                                  pvt_PrintFatFile(shortNamePositionInCurrentSector, currentSectorContents, bpb); 
                                  return END_OF_FILE;
                                }                                                                                    
                            }
                        }           
                    }

                  // Long name does not exist for current entry so
                  // check if fileNameStr matches the short name.
                  else if ((fileNameStrLen < 13) && !(attributeByte & DIRECTORY_ENTRY_ATTR_FLAG))
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
                        sn[k] = currentSectorContents[k + entry];

                      // if name portion of short name matches then check that extension matches.
                      if (!strcmp(tempFileName,sn))
                        {                                
                          uint8_t match = 0;
                          int entryExtExistsFlag = 0;

                          for (int k = 0; k < 3; k++)  
                            {
                              ext[k] = currentSectorContents[entry + 8 + k]; 
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
                              pvt_PrintFatFile (entry, currentSectorContents, bpb);
                              return END_OF_FILE;
                            }
                        }
                    }
                }
            }
        }
    } 
  while ( ((clusterIndex = pvt_GetNextClusterIndex (clusterIndex, bpb)) != END_OF_CLUSTER) && (clusCnt < 5));

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
FAT_PrintError (uint16_t err)
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
 * Description : This function is called by any FAT function that must match a 'name' string argument to a FAT entry 
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
  uint32_t absoluteCurrentSectorNumber;
  uint8_t  currentSectorContents[bpb->bytesPerSector];

  //absoluteCurrentSectorNumber = bpb->dataRegionFirstSector + ((cluster - 2) * bpb->sectorsPerCluster);
  absoluteCurrentSectorNumber = bpb->dataRegionFirstSector + ((Dir->FATFirstCluster - 2) * bpb->sectorsPerCluster);

  fat_ReadSingleSector (absoluteCurrentSectorNumber, currentSectorContents);

  parentDirectoryFirstCluster = currentSectorContents[53];
  parentDirectoryFirstCluster <<= 8;
  parentDirectoryFirstCluster |= currentSectorContents[52];
  parentDirectoryFirstCluster <<= 8;
  parentDirectoryFirstCluster |= currentSectorContents[59];
  parentDirectoryFirstCluster <<= 8;
  parentDirectoryFirstCluster |= currentSectorContents[58];

  // if current Dir is root Dir, do nothing.
  if (Dir->FATFirstCluster == bpb->rootCluster); 

  // parent Dir is root Dir
  else if (parentDirectoryFirstCluster == 0)
    {
      strcpy (Dir->shortName, "/");
      strcpy (Dir->shortParentPath, "");
      strcpy (Dir->longName, "/");
      strcpy (Dir->longParentPath, "");
      Dir->FATFirstCluster = bpb->rootCluster;
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
 *               instance of a Dir's struct members to a child Dir and a valid matching entry was 
 *               found. This function will only update the struct's members to that of the matching entry. It does
 *               not perform any of the search/compare required to find the match.
 * 
 * Argument    : *Dir     pointer to an instance of a FatDir struct whose members will be
 *                                     updated to point to the child Dir indicated by *nameStr.
 *             : *sector               pointer to an array that holds the physical sector's contents that contains the 
 *                                     short name of the entry that matches the *nameStr.
 *             : shortNamePosition     integer that specifies the first position in *sector where the 32-byte short
 *                                     name entry begins.
 *             : *nameStr              pointer to a C-string that specifies the longName member of the currenDirectory
 *                                     struct will be set to. If there is no long name associated with a short name
 *                                     then the longName and shortName members will both be set to the short name.  
 *             : *bpb                  pointer to the BPB struct instance.
***********************************************************************************************************************
*/

void
pvt_SetCurrentDirectoryToChild (FatDir * Dir, uint8_t *sector, uint16_t shortNamePosition, char * nameStr, BPB * bpb)
{
  uint32_t dirFirstCluster;
  dirFirstCluster = sector[shortNamePosition + 21];
  dirFirstCluster <<= 8;
  dirFirstCluster |= sector[shortNamePosition + 20];
  dirFirstCluster <<= 8;
  dirFirstCluster |= sector[shortNamePosition + 27];
  dirFirstCluster <<= 8;
  dirFirstCluster |= sector[shortNamePosition + 26];

  Dir->FATFirstCluster = dirFirstCluster;
  
  uint8_t snLen;
  if (strlen(nameStr) < 8) snLen = strlen(nameStr);
  else snLen = 8; 

  char sn[9];                                    
  for (uint8_t k = 0; k < snLen; k++)  
    sn[k] = sector[shortNamePosition + k];
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
 *               the *longNameStrIndex will point to the position in the c-string to begin loading the string char's
 * 
 * Arguments   : longNameFirstEntry    integer that points to the position in *sector that is the lowest order entry
 *                                     of the long name in the sector array.
 *             : longNameLastEntry     integer that points to the position in *sector that is the highest order entry
 *                                     of the long name in sector array.
 *             : *sector               pointer to an array that holds the physical sector's contents containing the 
 *                                     the entries of the long name to load into the c-string.
 *             : *longNameStr          pointer to a null terminated char array (C-string) that will be loaded with the
 *                                     long name characters from the sector.
 *             : *longNameStrIndex     pointer to an integer that specifies the position in *longNameStr where the
 *                                     first character will be loaded when this function is called. This function will
 *                                     update this value as the characters are loaded. If a subsequent call to this
 *                                     function is required in order to fully load the long name into the char array,
 *                                     as in the case when a single long name crosses a sector boundary, then this 
 *                                     value will be non-zero.
***********************************************************************************************************************
*/

void
pvt_LoadLongName (int longNameFirstEntry, int longNameLastEntry, uint8_t * sector, char * longNameStr, uint8_t * longNameStrIndex)
{
  for (int i = longNameFirstEntry; i >= longNameLastEntry; i = i - ENTRY_LEN)
    {                                              
      for (uint16_t n = i + 1; n < i + 11; n++)
        {                                  
          if (sector[n] == 0 || sector[n] > 126);
          else 
            { 
              longNameStr[*longNameStrIndex] = sector[n];
              (*longNameStrIndex)++;  
            }
        }

      for (uint16_t n = i + 14; n < i + 26; n++)
        {                                  
          if (sector[n] == 0 || sector[n] > 126);
          else 
            { 
              longNameStr[*longNameStrIndex] = sector[n];
              (*longNameStrIndex)++;  
            }
        }
      
      for (uint16_t n = i + 28; n < i + 32; n++)
        {                                  
          if (sector[n] == 0 || sector[n] > 126);
          else 
            { 
              longNameStr[*longNameStrIndex] = sector[n];  
              (*longNameStrIndex)++;  
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
  uint16_t numberOfIndexedClustersPerSectorOfFat = bpb->bytesPerSector / bytesPerClusterIndex; // = 128

  uint32_t clusterIndex = currentClusterIndex / numberOfIndexedClustersPerSectorOfFat;
  uint32_t clusterIndexStartByte = 4 * (currentClusterIndex % numberOfIndexedClustersPerSectorOfFat);
  uint32_t cluster = 0;

  uint32_t fatSectorToRead = clusterIndex + bpb->reservedSectorCount;

  uint8_t SectorContents[bpb->bytesPerSector];

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
 * Description : Used by FAT_PrintCurrentDirectory() to print the fields associated with an entry, such as creation or last
 *               modified date/time, file size, etc... Which fields are printed is determined by the entryFilter flags
 * 
 * Arguments   : *sector          pointer to an array that holds the short name of the entry that is being printed
 *                                to the screen. Only the short name entry of a short name/long name combination holds
 *                                these fields.
 *             : entry            location in *sector of the first byte of the short name entry whose fields should be
 *                                printed to the screen.
 *             : entryFilter      byte of ENTRY_FLAGs, used to determine which fields of the entry will be printed.
***********************************************************************************************************************
*/

void 
pvt_PrintEntryFields (uint8_t *sector, uint16_t entry, uint8_t entryFilter)
{
  uint16_t creationTime;
  uint16_t creationDate;
  uint16_t lastAccessDate;
  uint16_t writeTime;
  uint16_t writeDate;
  uint32_t fileSize;

  if (CREATION & entryFilter)
    {
      creationTime = sector[entry + 15];
      creationTime <<= 8;
      creationTime |= sector[entry + 14];
      
      creationDate = sector[entry + 17];
      creationDate <<= 8;
      creationDate |= sector[entry + 16];
    }

  if (LAST_ACCESS & entryFilter)
    {
      lastAccessDate = sector[entry + 19];
      lastAccessDate <<= 8;
      lastAccessDate |= sector[entry + 18];
    }

  if (LAST_MODIFIED & entryFilter)
    {
      writeTime = sector[entry + 23];
      writeTime <<= 8;
      writeTime |= sector[entry + 22];

      writeDate = sector[entry + 25];
      writeDate <<= 8;
      writeDate |= sector[entry + 24];
    }

  fileSize = sector[entry + 31];
  fileSize <<= 8;
  fileSize |= sector[entry + 30];
  fileSize <<= 8;
  fileSize |= sector[entry + 29];
  fileSize <<= 8;
  fileSize |= sector[entry + 28];

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
 * Arguments   : *sector          pointer to an array that holds the short name of the entry that is being printed
 *                                to the screen. Only the short name entry of a short name/long name combination holds
 *                                these fields.
 *             : entry            location in *sector of the first byte of the short name entry whose fields should be
 *                                printed to the screen.
***********************************************************************************************************************
*/

void 
pvt_PrintShortNameAndType (uint8_t *sector, uint16_t entry)
{
  char sn[9];
  char ext[5];

  for (uint8_t k = 0; k < 8; k++)
    {
      sn[k] = ' ';
    }
  sn[8] = '\0';

  uint8_t attr = sector[entry + 11];
  if (attr & 0x10)
    {
      print_str ("    <DIR>    ");
      for (uint8_t k = 0; k < 8; k++) 
        {
          sn[k] = sector[entry + k];
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
          ext[k] = sector[entry + 7 + k];  
        }

      for (uint8_t k = 0; k < 8; k++) 
        {
          sn[k] = sector[k + entry];
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
 * Arguments   : entry            location in *fileSector of the first byte of the short name entry of the file whose 
 *                                contents will be printed to the screen. This is required as the first cluster index 
 *                                of the file is located in the short name entry.
 *             : *fileSector      pointer to an array that holds the short name entry of the file to be printed to the 
 *                                to the screen.
***********************************************************************************************************************
*/

void 
pvt_PrintFatFile (uint16_t entry, uint8_t *fileSector, BPB * bpb)
  {
    uint32_t absoluteCurrentSectorNumber;
    uint32_t cluster;

    //get FAT index for file's first cluster
    cluster =  fileSector[entry + 21];
    cluster <<= 8;
    cluster |= fileSector[entry + 20];
    cluster <<= 8;
    cluster |= fileSector[entry + 27];
    cluster <<= 8;
    cluster |= fileSector[entry + 26];
    
    // read in contents of file starting at relative sector 0 in 'cluster' and print contents to the screen.
    do
      {
        print_str("\n\n\r");   
        for(uint32_t currentSectorNumberInCluster = 0; currentSectorNumberInCluster < bpb->sectorsPerCluster; currentSectorNumberInCluster++) 
          {
            absoluteCurrentSectorNumber = currentSectorNumberInCluster + bpb->dataRegionFirstSector + ( (cluster - 2) * bpb->sectorsPerCluster );

            fat_ReadSingleSector (absoluteCurrentSectorNumber, fileSector);
            for (uint16_t k = 0; k < bpb->bytesPerSector; k++)  
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
 *               entry is pointing to the correct location. This function uses the long name flag settings passed as
 *               arguments to determine if the adjustment needs to be made and makes the adjustment, if the correct
 *               entry is still in the current sector. If the correct entry is in the next sector, however, then the 
 *               function will exit with a value of 1 so that the next sector can be retrieved. Once the next sector 
 *               is retrieved, then this function is called again so that the entry correction can be made and the long 
 *               name flags.
 * 
 * Arguments   : *longNameExistsFlag                   if this flag is set then a long name existed for the previous 
 *                                                     entry that was checked. Reset by this function.
 *             : *longNameCrossSectorBoundaryFlag      if this flag is set then the long name of the previous entry 
 *                                                     crossed a sector boundary. Reset by this function.
 *             : *longNameLastSectorEntryFlag          if this flag is set then the long name of the previous entry was
 *                                                     entirely in the current sector, however, its corresponding short
 *                                                     name was in the next sector. Reset by this function
 *             : *entry                                pointer to an integer that specifies the location in the current
 *                                                     sector that will be checked/read in in the subsequent execution
 *                                                     of the calling function. This value will be updated by this
 *                                                     function if a correction to it is required. 
 *             : *shortNamePositionInCurrentSector     pointer to an integer that specifies the position of this 
 *                                                     variable that was set for the previous entry checked.
 *             : *shortNamePositionInNextSector        pointer to an integer that specifies the position of this
 *                                                     variable, if it was set, for the previous entry checked.
 * 
 * Returns     : 1 if it is determined that the next entry to check is in the next sector.
 *             : 0 if the next entry to check is in the current sector.
***********************************************************************************************************************
*/
/*
uint8_t 
pvt_CorrectEntryCheckAndLongNameFlagReset (uint8_t  * longNameExistsFlag, uint8_t  * longNameCrossSectorBoundaryFlag, uint8_t  * longNameLastSectorEntryFlag,
                                           uint16_t * entry, uint16_t * shortNamePositionInCurrentSector, uint16_t * shortNamePositionInNextSector)
{  
  if ( (*longNameExistsFlag))
    {
      if ( (*shortNamePositionInCurrentSector) >= (SECTOR_LEN - ENTRY_LEN))
        {
          if ((*entry) != 0)
            return 1; // need to get the next sector
          else 
            (*shortNamePositionInCurrentSector) = -ENTRY_LEN;
        }

      if ((*longNameCrossSectorBoundaryFlag) || (*longNameLastSectorEntryFlag))
        {
          *entry = (*shortNamePositionInNextSector) + ENTRY_LEN; 
          *shortNamePositionInNextSector = 0; 
          *longNameCrossSectorBoundaryFlag = 0; 
          *longNameLastSectorEntryFlag = 0;
        }
      else 
        {
          *entry = (*shortNamePositionInCurrentSector) + ENTRY_LEN; 
          *shortNamePositionInCurrentSector = 0;
        }
      *longNameExistsFlag = 0;
    }
  return 0;
}   

*/
// ************

uint8_t 
pvt_CorrectEntryCheckAndLongNameFlagReset (uint8_t  * longNameFlags, uint16_t * entry, uint16_t * shortNamePositionInCurrentSector, uint16_t * shortNamePositionInNextSector)
{  
  if ((*longNameFlags) & LONG_NAME_EXISTS_FLAG)
    {
      if ( (*shortNamePositionInCurrentSector) >= (SECTOR_LEN - ENTRY_LEN))
        {
          if ((*entry) != 0)
            return 1; // need to get the next sector
          else 
            (*shortNamePositionInCurrentSector) = -ENTRY_LEN;
        }

      if ((*longNameFlags) & (LONG_NAME_CROSSES_SECTOR_BOUNDARY_FLAG | LONG_NAME_LAST_SECTOR_ENTRY_FLAG))
        {
          *entry = (*shortNamePositionInNextSector) + ENTRY_LEN; 
          *shortNamePositionInNextSector = 0;
        }
      else 
        {
          *entry = (*shortNamePositionInCurrentSector) + ENTRY_LEN;
          *shortNamePositionInCurrentSector = 0;
        }
      *longNameFlags = 0;
    }

  return 0;
}    

// ************




/*
***********************************************************************************************************************
 *                                       (PRIVATE) SET THE LONG NAME FLAGS
 *
 * Description : Used by the FAT functions to set the long name flags, if it was determined that a long name exists, 
 *               for the current entry begin checked / read in. This also sets the shortNamePositionInCurrentSector.
 * 
 * Arguments   : *longNameExistsFlag                 - This flag is automatically set to 1 by this function to indicate
 *                                                     a long exists for this entry.
 *             : *longNameCrossSectorBoundaryFlag    - This flag will be set to 1 if the long name is found to cross a 
 *                                                     a sector boundary.
 *             : *longNameLastSectorEntryFlag        - This flag will be set to 1 if the long name is entirely in the 
 *                                                     current sector, but its short name is in the next sector.
 *             :  entry                              - Integer that specifies the location in the current
 *                                                     sector that will be checked/read in in the subsequent execution
 *                                                     of the calling function. This value will be updated by this
 *                                                     function if a correction to it is required. 
 *             : *shortNamePositionInCurrentSector   - This value is set by this function, and is a pointer to an 
 *                                                     integer that specifies the position of the short name relative 
 *                                                     to the first byte of the current sector. If this is greater than
 *                                                     512 then the short name is in the next sector.
 *             : *shortNamePositionInNextSector      - Pointer to an integer that specifies the position of the short
 *                                                     name if it is determined to be in the next sector.
***********************************************************************************************************************
*/
/*
void
pvt_SetLongNameFlags ( uint8_t  * longNameExistsFlag, 
                       uint8_t  * longNameCrossSectorBoundaryFlag, 
                       uint8_t  * longNameLastSectorEntryFlag,
                       uint16_t   entry,
                       uint16_t * shortNamePositionInCurrentSector,
                       uint8_t  * currentSectorContents,
                       BPB * bpb
                     )
{
  *longNameExistsFlag = 1;

  // number of entries required by the long name
  uint8_t longNameOrder = LONG_NAME_ORDINAL_MASK & currentSectorContents[entry]; 
                                  
  *shortNamePositionInCurrentSector = (entry) + (ENTRY_LEN * longNameOrder);
  
  // if the short name position is greater than 511 (bytePerSector-1) then the short name is in the next sector.
  if ((*shortNamePositionInCurrentSector) >= bpb->bytesPerSector)
    {
      if ((*shortNamePositionInCurrentSector) > bpb->bytesPerSector)
        {
          *longNameCrossSectorBoundaryFlag = 1;
          *longNameLastSectorEntryFlag = 0;
        }
      else if (*shortNamePositionInCurrentSector == SECTOR_LEN)
        {
          *longNameCrossSectorBoundaryFlag = 0;
          *longNameLastSectorEntryFlag = 1;
        }
    }
}    
*/

void
pvt_SetLongNameFlags ( uint8_t  * longNameFlags,
                          uint16_t   entry,
                          uint16_t * shortNamePositionInCurrentSector,
                          uint8_t  * currentSectorContents,
                          BPB * bpb
                     )
{
  *longNameFlags |= LONG_NAME_EXISTS_FLAG;

  // number of entries required by the long name
  uint8_t longNameOrder = LONG_NAME_ORDINAL_MASK & currentSectorContents[entry]; 
                                  
  *shortNamePositionInCurrentSector = entry + (ENTRY_LEN * longNameOrder);
  
  // if the short name position is greater than 511 (bytePerSector-1) then the short name is in the next sector.
  if ((*shortNamePositionInCurrentSector) >= bpb->bytesPerSector)
    {
      if ((*shortNamePositionInCurrentSector) > bpb->bytesPerSector)
        {
          *longNameFlags |= LONG_NAME_CROSSES_SECTOR_BOUNDARY_FLAG;
        }
      else if (*shortNamePositionInCurrentSector == SECTOR_LEN)
        {
          *longNameFlags |= LONG_NAME_LAST_SECTOR_ENTRY_FLAG;
        }
    }
}   

/*
***********************************************************************************************************************
 *                             (PRIVATE) LOAD THE CONTENTS OF THE NEXT SECTOR INTO AN ARRAY
 *
 * Description : Used by the FAT functions to load the contents of the next file or Dir sector into the 
 *               *nextSectorContents array if it is foudn that a long / short name combo crosses the sector boundary.
 * 
 * Arguments   : *nextSectorContents                 - Pointer to an array that will be loaded with the contents of the
 *                                                     next sector of a file or Dir.
 *             : currentSectorNumberInCluster        - Integer that specifies the current sector number relative to the
 *                                                     current cluster. This value is used to determine if the next
 *                                                     sector is in the current or the next cluster.
 *             : absoluteCurrentSectorNumber         - This is the the current sector's physical sector number on the
 *                                                     disk hosting the FAT volume.
 *             : clusterIndex                       - This is the current cluster's FAT index. This is required if it
 *                                                     is determined that the next sector is in the next cluster.   
 *             : *bpb                                - pointer to the BPB struct instance.
***********************************************************************************************************************
*/

void
pvt_GetNextSector( uint8_t * nextSectorContents, uint32_t currentSectorNumberInCluster, uint32_t absoluteCurrentSectorNumber, uint32_t clusterIndex,  BPB * bpb )
{
  uint32_t absoluteNextSectorNumber; 
  
  if (currentSectorNumberInCluster >= (bpb->sectorsPerCluster - 1)) 
    absoluteNextSectorNumber = bpb->dataRegionFirstSector + ((pvt_GetNextClusterIndex (clusterIndex, bpb) - 2) * bpb->sectorsPerCluster);
  else 
    absoluteNextSectorNumber = 1 + absoluteCurrentSectorNumber;

  fat_ReadSingleSector (absoluteNextSectorNumber, nextSectorContents);
}
