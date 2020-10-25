/*
***********************************************************************************************************************
*                                                       AVR-FAT MODULE
*
* File   : FAT.C
* Author : Joshua Fain
* Target : ATMega1280
*
*
* DESCRIPTION: 
* Defines functions declared in FAT.H for accessing contents of a FAT32 formatted volume using an AVR microconstroller. 
* The fuctions defined here only provide READ access to the volume's contents (i.e. print file, print directory), no 
* WRITE access is currently possible.
*
*
* FUNCTION "PUBLIC":
*  (1) uint8_t  FAT_SetBiosParameterBlock(BPB * bpb);
*  (2) void     FAT_PrintBootSectorError (uint8_t err);
*  (3) void     FAT_SetDirectoryToRoot(FatDir * Dir, BPB * bpb);
*  (4) uint8_t  FAT_SetDirectory (FatDir * Dir, char * newDirStr, BPB * bpb);
*  (5) uint8_t  FAT_PrintDirectory (FatDir * Dir, uint8_t entryFilter, BPB * bpb);
*  (6) uint8_t  FAT_PrintFile (FatDir * Dir, char * file, BPB * bpb);
*  (7) void     FAT_PrintError(uint8_t err);
*
*
* STRUCTS USED (defined in FAT.H):
*  (1) typedef struct BiosParameterBlock BPB
*  (2) typedef struct FatDirectory FatDir
*
*                                                 
*                                                       MIT LICENSE
*
* Copyright (c) 2020 Joshua Fain
*
* Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated 
* documentation files (the "Software"), to deal in the Software without restriction, including without limitation the 
* rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to 
* permit ersons to whom the Software is furnished to do so, subject to the following conditions: The above copyright 
* notice and this permission notice shall be included in all copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE 
* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
* COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
* OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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

uint8_t pvt_CheckValidName (char * nameStr, FatDir * Dir);
uint8_t pvt_SetDirectoryToParent (FatDir * Dir, BPB * bpb);
void pvt_SetDirectoryToChild (FatDir * Dir, uint8_t * sectorArr, uint16_t snPos, char * childDirStr, BPB * bpb);
void pvt_LoadLongName (int lnFirstEntry, int lnLastEntry, uint8_t * sectorArr, char * lnStr, uint8_t * lnStrIndx);
uint32_t pvt_GetNextClusterIndex (uint32_t currentClusterIndex, BPB * bpb);
void pvt_PrintEntryFields (uint8_t * byte, uint16_t entryPos, uint8_t entryFilter);
void pvt_PrintShortName (uint8_t * byte, uint16_t entryPos, uint8_t entryFilter);
uint8_t pvt_PrintFatFile (uint16_t entryPos, uint8_t * fileSector, BPB * bpb);
uint8_t pvt_CorrectEntryCheck (uint8_t lnFlags, uint16_t * entryPos, uint16_t * snPosCurrSec, uint16_t * snPosNextSec);           
void pvt_SetLongNameFlags (uint8_t * lnFlags, uint16_t entryPos, 
                           uint16_t * snPosCurrSec, uint8_t * currSecArr, BPB * bpb);
uint8_t pvt_GetNextSector (uint8_t * nextSecArr, uint32_t currSecNumInClus, 
                        uint32_t currSecNumPhys, uint32_t clusIndx, BPB* bpb);




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
  uint8_t err;

  // 0xFFFFFFFF is returned for the boot sector location, then locating it failed.
  bpb->bootSecAddr = FATtoDisk_FindBootSector();
  
  if (bpb->bootSecAddr != 0xFFFFFFFF)
    {
      err = FATtoDisk_ReadSingleSector (bpb->bootSecAddr, BootSector);
      if (err == 1) return FAILED_READ_BOOT_SECTOR;
    }
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
    case FAILED_READ_BOOT_SECTOR:
      print_str("FAILED_READ_BOOT_SECTOR");
      break;
    default:
      print_str("UNKNOWN_ERROR");
      break;
  }
}



/*
***********************************************************************************************************************
 *                                SET MEMBERS OF FAT DIRECTORY INSTANCE TO ROOT DIRECTORY
 * 
 * This function should be called before maniupulating/accessing the FatDir instance using the other FAT functions.
 * Call this function to set the members of the FatDir struct's instance to the root directory
 *                                         
 * Description : This function will set the members of a BiosParameterBlock (BPB) struct instance according to the
 *               values specified within the FAT volume's Bios Parameter Block / Boot Sector. 
 * 
 * Arguments   : *Dir          - Pointer to a FatDir struct whose members will be set to point to the root directory.
 *             : *bpb          - Pointer to a BPB struct instance.
***********************************************************************************************************************
*/

void
FAT_SetDirectoryToRoot(FatDir * Dir, BPB * bpb)
{
  for (uint8_t i = 0; i < LONG_NAME_STRING_LEN_MAX; i++)
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
  if (pvt_CheckValidName (newDirStr, Dir)) 
    return INVALID_DIR_NAME;

  // newDirStr == 'Current Directory' ?
  if (!strcmp (newDirStr,  ".")) 
    return SUCCESS;
  
  // newDirStr == 'Parent Directory' ?
  if (!strcmp (newDirStr, ".."))
      // returns either FAILED_READ_SECTOR or SUCCESS
      return pvt_SetDirectoryToParent (Dir, bpb);

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
  char     lnStr[LONG_NAME_STRING_LEN_MAX];
  uint8_t  lnStrIndx = 0;
  uint8_t  lnFlags = 0;
  uint8_t  err;

  do
    {
      for (uint32_t currSecNumInClus = 0; currSecNumInClus < bpb->secPerClus; currSecNumInClus++)
        {         
          // load sector data into currSecArr
          currSecNumPhys = currSecNumInClus + bpb->dataRegionFirstSector + ((clusIndx - 2) * bpb->secPerClus);
          err = FATtoDisk_ReadSingleSector (currSecNumPhys, currSecArr);
          if (err == 1) return FAILED_READ_SECTOR;

          for (uint16_t entryPos = 0; entryPos < SECTOR_LEN; entryPos += ENTRY_LEN)
            {
              entryCorrectionFlag = pvt_CorrectEntryCheck (lnFlags, &entryPos, &snPosCurrSec, &snPosNextSec);
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
                      
                      for (uint8_t k = 0; k < LONG_NAME_STRING_LEN_MAX; k++) lnStr[k] = '\0';

                      lnStrIndx = 0;
                      pvt_SetLongNameFlags ( &lnFlags, entryPos, &snPosCurrSec, currSecArr, bpb);

                      if (lnFlags & (LONG_NAME_CROSSES_SECTOR_BOUNDARY | LONG_NAME_LAST_SECTOR_ENTRY))
                        {
                          err = pvt_GetNextSector (nextSecArr, currSecNumInClus, currSecNumPhys, clusIndx, bpb);
                          if (err == FAILED_READ_SECTOR) return FAILED_READ_SECTOR;
                          snPosNextSec = snPosCurrSec - bpb->bytesPerSec;
                          attrByte = nextSecArr[snPosNextSec + 11];

                          // If snPosNextSec points to long name entry then something is wrong.
                          if ((attrByte & LONG_NAME_ATTR_MASK) == LONG_NAME_ATTR_MASK) return CORRUPT_FAT_ENTRY;

                          // Only continue for current entry if it is a directory.
                          if (attrByte & DIRECTORY_ENTRY_ATTR)
                            {                                                           
                              if (lnFlags & LONG_NAME_CROSSES_SECTOR_BOUNDARY)
                                {
                                  // Entry immediately preceeding short name must be the long names's first entry.
                                  if ((nextSecArr[snPosNextSec - ENTRY_LEN] & LONG_NAME_ORDINAL_MASK) != 1)
                                    return CORRUPT_FAT_ENTRY;

                                  // Load long name entry into lnStr[]
                                  pvt_LoadLongName (snPosNextSec - ENTRY_LEN, 0, nextSecArr, lnStr, &lnStrIndx);
                                  pvt_LoadLongName (SECTOR_LEN - ENTRY_LEN, entryPos, currSecArr, lnStr, &lnStrIndx);
                                  if ( !strcmp (newDirStr, lnStr)) 
                                    {                                                        
                                      pvt_SetDirectoryToChild (Dir, nextSecArr, snPosNextSec, newDirStr, bpb);
                                      return SUCCESS;
                                    }
                                }

                              // All entries for long name are in current sector but short name is in next sector
                              else if (lnFlags & LONG_NAME_LAST_SECTOR_ENTRY)
                               {
                                  // Entry immediately preceeding short name must be the long names's first entry.
                                  if ((currSecArr[SECTOR_LEN - ENTRY_LEN] & LONG_NAME_ORDINAL_MASK) != 1)
                                    return CORRUPT_FAT_ENTRY;

                                  // Load long name entry into lnStr[]
                                  pvt_LoadLongName(SECTOR_LEN - ENTRY_LEN, entryPos, currSecArr, lnStr, &lnStrIndx);
                                  if ( !strcmp (newDirStr, lnStr)) 
                                    { 
                                      pvt_SetDirectoryToChild (Dir, nextSecArr, snPosNextSec, newDirStr, bpb);
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
                                  pvt_SetDirectoryToChild (Dir, currSecArr, snPosCurrSec, newDirStr, bpb);
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
                          pvt_SetDirectoryToChild (Dir, currSecArr, entryPos, newDirStr, bpb);
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
FAT_PrintDirectory (FatDir * Dir, uint8_t entryFilter, BPB * bpb)
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
  char     lnStr[LONG_NAME_STRING_LEN_MAX];
  uint8_t  lnStrIndx = 0;
  uint8_t  lnFlags = 0;
  uint8_t  err;

  // Prints column headers according to entryFilter
  print_str("\n\n\r");
  if (CREATION & entryFilter) print_str(" CREATION DATE & TIME,");
  if (LAST_ACCESS & entryFilter) print_str(" LAST ACCESS DATE,");
  if (LAST_MODIFIED & entryFilter) print_str(" LAST MODIFIED DATE & TIME,");
  if (FILE_SIZE & entryFilter) print_str(" SIZE,");
  if (TYPE & entryFilter) print_str(" TYPE,");
  
  //print_str(" SIZE, TYPE, NAME");
  print_str(" NAME");
  print_str("\n\n\r");

  do 
    {
      for (uint32_t currSecNumInClus = 0; currSecNumInClus < bpb->secPerClus; currSecNumInClus++)
        {
          // load sector bytes into currSecArr[]
          currSecNumPhys = currSecNumInClus + bpb->dataRegionFirstSector + ((clusIndx - 2) * bpb->secPerClus);
          err = FATtoDisk_ReadSingleSector (currSecNumPhys, currSecArr);
          if (err == 1) return FAILED_READ_SECTOR;

          for (uint16_t entryPos = 0; entryPos < bpb->bytesPerSec; entryPos = entryPos + ENTRY_LEN)
            {
              entryCorrectionFlag = pvt_CorrectEntryCheck (lnFlags, &entryPos, &snPosCurrSec, &snPosNextSec);
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

                      for (uint8_t k = 0; k < LONG_NAME_STRING_LEN_MAX; k++) lnStr[k] = '\0';
 
                      lnStrIndx = 0;

                      pvt_SetLongNameFlags ( &lnFlags, entryPos, &snPosCurrSec, currSecArr, bpb);

                      if (lnFlags & (LONG_NAME_CROSSES_SECTOR_BOUNDARY | LONG_NAME_LAST_SECTOR_ENTRY))
                        {
                          err = pvt_GetNextSector ( nextSecArr, currSecNumInClus, currSecNumPhys, clusIndx, bpb );
                          if (err == FAILED_READ_SECTOR) return FAILED_READ_SECTOR;
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
                                  pvt_PrintShortName(nextSecArr, snPosNextSec, entryFilter);
                                }
                              // print long name if filter flag is set.
                              if (entryFilter & LONG_NAME)
                                {
                                  if (lnFlags & LONG_NAME_CROSSES_SECTOR_BOUNDARY)
                                    {
                                      // Entry immediately preceeding short name must be the long names's first entry.
                                      if ((nextSecArr[snPosNextSec - ENTRY_LEN] & LONG_NAME_ORDINAL_MASK) != 1) 
                                        return CORRUPT_FAT_ENTRY;                                              

                                      pvt_PrintEntryFields (nextSecArr, snPosNextSec, entryFilter);

                                      if (attrByte & DIRECTORY_ENTRY_ATTR) 
                                        { 
                                          if ((entryFilter & TYPE) == TYPE) 
                                            print_str (" <DIR>   ");
                                        }
                                      else 
                                        { 
                                          if ((entryFilter & TYPE) == TYPE) 
                                            print_str (" <FILE>  ");
                                        }
  
                                      // load long name entryPos into lnStr[]
                                      pvt_LoadLongName (snPosNextSec - ENTRY_LEN, 0, 
                                                        nextSecArr, lnStr, &lnStrIndx);
                                      pvt_LoadLongName (SECTOR_LEN - ENTRY_LEN, entryPos, 
                                                        currSecArr, lnStr, &lnStrIndx);
                                      print_str(lnStr);
                                    }

                                  else if (lnFlags & LONG_NAME_LAST_SECTOR_ENTRY)
                                    {
                                      // Entry immediately preceeding short name must be the long names's first entry.
                                      if ((currSecArr[SECTOR_LEN - ENTRY_LEN] & LONG_NAME_ORDINAL_MASK) != 1) 
                                        return CORRUPT_FAT_ENTRY;
                            
                                      pvt_PrintEntryFields (nextSecArr, snPosNextSec, entryFilter);


                                      if (attrByte & DIRECTORY_ENTRY_ATTR) 
                                        { 
                                          if ((entryFilter & TYPE) == TYPE) 
                                            print_str (" <DIR>   ");
                                        }
                                      else 
                                        { 
                                          if ((entryFilter & TYPE) == TYPE) 
                                            print_str (" <FILE>  ");
                                        }
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
                                  pvt_PrintShortName (currSecArr, snPosCurrSec, entryFilter);
                                }
                              // print long name if filter flag is set.
                              if (entryFilter & LONG_NAME)
                                {
                                  // Confirm entry preceding short name is first entryPos of a long name.
                                  if ((currSecArr[snPosCurrSec - ENTRY_LEN] & LONG_NAME_ORDINAL_MASK) != 1) 
                                    return CORRUPT_FAT_ENTRY;

                                  pvt_PrintEntryFields(currSecArr, snPosCurrSec, entryFilter);
                                  
                                  if (attrByte & DIRECTORY_ENTRY_ATTR) 
                                    { 
                                      if ((entryFilter & TYPE) == TYPE) 
                                        print_str (" <DIR>   ");
                                    }
                                  else 
                                    { 
                                      if ((entryFilter & TYPE) == TYPE) 
                                        print_str (" <FILE>  ");
                                    }

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
                          pvt_PrintShortName(currSecArr, entryPos, entryFilter);
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
 * Description : Prints the contents of a FAT file from the current directory to a terminal/screen.
 * 
 * Arguments   : *Dir              - Pointer to a FatDir struct instance pointing to a valid FAT32 directory.
 *             : *fileNameStr      - Ptr to C-string. This is the name of the file to be printed to the screen. This
 *                                   can only be a long name unless there is no long name for a given entry, in which
 *                                   case it must be a short name.
 *             : *bpb              - Pointer to a valid instance of a BPB struct.
 * 
 * Return      : FAT Error Flag     Returns END_OF_FILE if the function completed successfully and was able to read in
 *                                  and print a file's contents to the screen. Any other value returned indicates an
 *                                  an error. Pass the returned value to FAT_PrintError(ErrorFlag).
***********************************************************************************************************************
*/

uint8_t 
FAT_PrintFile (FatDir * Dir, char * fileNameStr, BPB * bpb)
{
  if (pvt_CheckValidName (fileNameStr, Dir)) 
    return INVALID_FILE_NAME;
  
  uint8_t  fileNameStrLen = strlen (fileNameStr);

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
  char     lnStr[LONG_NAME_STRING_LEN_MAX];
  uint8_t  lnStrIndx = 0;
  uint8_t  lnFlags = 0;
  uint8_t  err;
   
  do
    {
      for (uint32_t currSecNumInClus = 0; currSecNumInClus < bpb->secPerClus; currSecNumInClus++)
        {     
          // load sector bytes into currSecArr[]
          currSecNumPhys = currSecNumInClus + bpb->dataRegionFirstSector + ((clusIndx - 2) * bpb->secPerClus);
          err = FATtoDisk_ReadSingleSector (currSecNumPhys, currSecArr );
          if (err == 1) return FAILED_READ_SECTOR;

          // loop through entries in the current sector.
          for (uint16_t entryPos = 0; entryPos < bpb->bytesPerSec; entryPos = entryPos + ENTRY_LEN)
            { 
              entryCorrectionFlag = pvt_CorrectEntryCheck (lnFlags, &entryPos, &snPosCurrSec, &snPosNextSec);
              if (entryCorrectionFlag == 1)
                {
                  entryCorrectionFlag = 0;
                  break;    
                }
              
              // reset long name flags
              lnFlags = 0;

              // If first value of entry is 0 then all subsequent entries are empty.
              if (currSecArr[ entryPos ] == 0) 
                return FILE_NOT_FOUND;

              // Only continue checking this entry if it has not been marked for deletion.
              if (currSecArr[entryPos] != 0xE5)
                {
                  attrByte = currSecArr[entryPos + 11];
                  
                  if ((attrByte & LONG_NAME_ATTR_MASK) == LONG_NAME_ATTR_MASK)
                    {
                      if ( !(currSecArr[entryPos] & LONG_NAME_LAST_ENTRY)) return CORRUPT_FAT_ENTRY;
                      
                      for (uint8_t k = 0; k < LONG_NAME_STRING_LEN_MAX; k++) lnStr[k] = '\0';

                      pvt_SetLongNameFlags (&lnFlags, entryPos, &snPosCurrSec, currSecArr, bpb);
                      
                      lnStrIndx = 0;
                      if (lnFlags & (LONG_NAME_CROSSES_SECTOR_BOUNDARY | LONG_NAME_LAST_SECTOR_ENTRY))
                        {
                          err = pvt_GetNextSector (nextSecArr, currSecNumInClus, currSecNumPhys, clusIndx, bpb );
                          if (err == FAILED_READ_SECTOR) return FAILED_READ_SECTOR;
                          snPosNextSec = snPosCurrSec - bpb->bytesPerSec;
                          attrByte = nextSecArr[ snPosNextSec + 11 ];
                          
                          // If snPosNextSec points to long name entry then something is wrong.
                          if ((attrByte & LONG_NAME_ATTR_MASK) == LONG_NAME_ATTR_MASK) return CORRUPT_FAT_ENTRY;

                          // Only proceed if entry points to a file (i.e. directory attribute flag is not set).
                          if ( !(attrByte & DIRECTORY_ENTRY_ATTR))
                            {                                                           
                              if (lnFlags & LONG_NAME_CROSSES_SECTOR_BOUNDARY)
                                {
                                  // Entry immediately preceeding short name must be the long names's first entry.
                                  if ((nextSecArr[snPosNextSec - ENTRY_LEN] & LONG_NAME_ORDINAL_MASK) != 1) 
                                    return CORRUPT_FAT_ENTRY; 
                                  
                                  // load long name entry into lnStr[]
                                  pvt_LoadLongName (snPosNextSec - ENTRY_LEN, 0, nextSecArr, lnStr, &lnStrIndx);
                                  pvt_LoadLongName (SECTOR_LEN - ENTRY_LEN, entryPos, currSecArr, lnStr, &lnStrIndx);
                                }
                              else if (lnFlags & LONG_NAME_LAST_SECTOR_ENTRY)
                                {
                                  lnStrIndx = 0;

                                  // Entry immediately preceeding short name must be the long names's first entry.
                                  if ((currSecArr[SECTOR_LEN - ENTRY_LEN] & LONG_NAME_ORDINAL_MASK) != 1) 
                                    return CORRUPT_FAT_ENTRY;                                                                  
                                      
                                  // read long name entry into lnStr
                                  pvt_LoadLongName (SECTOR_LEN - ENTRY_LEN, entryPos, currSecArr, lnStr, &lnStrIndx);
                                                                             
                                }
                              else return CORRUPT_FAT_ENTRY;

                              // print file contents if a matching file entry was found
                              if ( !strcmp(fileNameStr, lnStr))
                                // returnes either END_OF_FILE or FAILED_READ_SECTOR
                                return pvt_PrintFatFile (entryPos, currSecArr, bpb);
                            }
                        }

                      // Long name exists and long and short name are entirely in the current sector.
                      else 
                        {   
                          attrByte = currSecArr[snPosCurrSec + 11];
                          
                          // confirm snPosCurrSec points to a short name entry in the current sector
                          if ((attrByte & LONG_NAME_ATTR_MASK) == LONG_NAME_ATTR_MASK) 
                            return CORRUPT_FAT_ENTRY;
                          
                          // proceed if entry is a file
                          if ( !(attrByte & DIRECTORY_ENTRY_ATTR))
                            {                   
                              // confirm entry immediatedly preceding the short name entry is a long name's first entry
                              if ((currSecArr[snPosCurrSec - ENTRY_LEN] & LONG_NAME_ORDINAL_MASK) != 1 ) 
                                return CORRUPT_FAT_ENTRY;

                              // read long name into lnStr
                              pvt_LoadLongName (snPosCurrSec - ENTRY_LEN, entryPos, currSecArr, lnStr, &lnStrIndx);

                              // print file contents if a matching entryPos was found
                              if ( !strcmp (fileNameStr, lnStr))
                                // returnes either END_OF_FILE or FAILED_READ_SECTOR
                                return pvt_PrintFatFile (entryPos, currSecArr, bpb);                                                                                   
                            }
                        }           
                    }

                  // Check if fileNameStr matches a short name if a long name does not exist for current entry.
                  else if ((fileNameStrLen < 13) && !(attrByte & DIRECTORY_ENTRY_ATTR))
                    {                   
                      
                      char sn[9];
                      char ext[4];

                      
                      for (uint8_t k = 0; k < 9; k++) sn[k] = '\0';
                      for (uint8_t k = 0; k < 4; k++) ext[k] = '\0'; 
              
                      // Locate '.', if it exists, in fileNameStr. Exclude first position (indicate hidden).
                      uint8_t pt = fileNameStrLen;
                      uint8_t extExistsFlag = 0;
                      for (uint8_t k = 1; k < pt; k++)
                        {
                          if (k+1 >= fileNameStrLen) break;
                          if (fileNameStr[k] == '.')  
                            {
                              extExistsFlag = 1;
                              pt = k;
                              break; 
                            }
                        }

                      // load fileNameStr into an array that can be directly compared with 
                      char compFN[9];
                      for (uint8_t k = 0; k < pt; k++) compFN[k] = fileNameStr[k];
                      for (uint8_t k = pt; k < 8; k++) compFN[k] = ' ';
                      compFN[8] = '\0';
                      
                      for (uint8_t k = 0; k < 8; k++) sn[k] = currSecArr[k + entryPos];

                      // if name portion of short name matches then check that extension matches.
                      if ( !strcmp (compFN, sn))
                        {                                
                          uint8_t match = 0;
                          int entryExtExistsFlag = 0;

                          for (int k = 0; k < 3; k++)  
                            {
                              ext[k] = currSecArr[entryPos + 8 + k]; 
                              entryExtExistsFlag = 1;
                            }

                          if (strcmp (ext, "   ") && extExistsFlag) entryExtExistsFlag = 1;

                          if ( !entryExtExistsFlag && !extExistsFlag) 
                            match = 1;
                          else if ((entryExtExistsFlag && !extExistsFlag) || (!entryExtExistsFlag && extExistsFlag)) 
                            match = 0;
                          else if (entryExtExistsFlag && extExistsFlag)
                            {
                              char tempEXT[4];
                              for (uint8_t k = 0; k < 3; k++) tempEXT[k] = ' ';
                              tempEXT[3] = '\0'; 

                              for (uint8_t k = 0; k < 3; k++)
                                {
                                  if (fileNameStr[k+pt+1] == '\0') break;
                                  tempEXT[k] = fileNameStr[k + pt + 1];
                                }

                              // if extensions match set to match to 1
                              if ( !strcmp (ext, tempEXT)) match = 1;
                            }
                          if(match)
                            // returnes either END_OF_FILE or FAILED_READ_SECTOR
                            return pvt_PrintFatFile (entryPos, currSecArr, bpb);
                        }
                    }
                }
            }
        }
    } 
  while ((clusIndx = pvt_GetNextClusterIndex (clusIndx, bpb)) != END_OF_CLUSTER);

  return FILE_NOT_FOUND; 
}



/*
***********************************************************************************************************************
 *                                        PRINT ERROR RETURNED BY A FAT FUNCTION
 * 
 * Description : Call this function to print an error flag returned by one of the FAT functions to the screen. 
 * 
 * Argument    : err    This should be an error flag returned by one of the FAT file/directory functions.
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
    case FAILED_READ_SECTOR:
      print_str("\n\rFAILED_READ_SECTOR");
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
 * Description : Function used to confirm a 'name' string is legal and valid size based on the current settings. This 
 *               function is called by any FAT function that must match a 'name' string argument to a FAT entry name 
 *               (e.g. FAT_PrintFile or FAT_SetDirectory). 
 * 
 * Argument    : *nameStr    C-string that the calling function needs to be verify is a legal FAT name.
 *             : *bpb        - Pointer to a valid instance of a BPB struct.
 *            
 * Return      : 0 if name is LEGAL, 1 if name is ILLEGAL.
***********************************************************************************************************************
*/

uint8_t
pvt_CheckValidName (char * nameStr, FatDir * Dir)
{
  // check that long name and path size are 
  // not too large for current settings.
  if (strlen (nameStr) > LONG_NAME_STRING_LEN_MAX) return 1;
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
***********************************************************************************************************************
 *                                  (PRIVATE) SET CURRENT DIRECTORY TO ITS PARENT
 * 
 * Description : This function is called by FAT_SetDirectory() if that function has been requested to set the FatDir 
 *               instance to its parent directory. This function will carry out the task of setting the members.
 * 
 * Argument    : *Dir         - Pointer to a FatDir instance whose members will be updated to point to the parent of 
 *                              the directory pointed to by the current instance's members.
 *             : *bpb         - Pointer to a valid instance of a BPB struct.
***********************************************************************************************************************
*/


uint8_t
pvt_SetDirectoryToParent (FatDir * Dir, BPB * bpb)
{
  uint32_t parentDirFirstClus;
  uint32_t currSecNumPhys;
  uint8_t  currSecArr[bpb->bytesPerSec];
  uint8_t  err;

  currSecNumPhys = bpb->dataRegionFirstSector + ((Dir->FATFirstCluster - 2) * bpb->secPerClus);

  // function returns either 0 for success for 1 for failed.
  err = FATtoDisk_ReadSingleSector (currSecNumPhys, currSecArr);
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
 *                           (PRIVATE) SET DIRECTORY TO A CHILD DIRECTORY
 * 
 * Description : This function is called by FAT_SetDirectory() if that function has been asked to set an instance of
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

void
pvt_SetDirectoryToChild (FatDir * Dir, uint8_t * sectorArr, uint16_t snPos, char * childDirStr, BPB * bpb)
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



/*
***********************************************************************************************************************
 *                                  (PRIVATE) LOAD A LONG NAME ENTRY INTO A C-STRING
 * 
 * Description : This function is called by a FAT function that must read in a long name from a FAT directory into a
 *               C-string. The function is called twice if a long name crosses a sector boundary, and *lnStrIndx will
 *               point to the position in the string to begin loading the next characters
 * 
 * Arguments   : lnFirstEntry  - Integer that points to the position in *sectorArr that is the lowest order entry
 *                                     of the long name in the sector array.
 *             : lnLastEntry   - Integer that points to the position in *sectorArr that is the highest order
 *                                     entry of the long name in the sector array.
 *             : *sectorArr          - Pointer to an array that holds the physical sector's contents containing the 
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

void
pvt_LoadLongName (int lnFirstEntry, int lnLastEntry, uint8_t * sectorArr, char * lnStr, uint8_t * lnStrIndx)
{
  for (int i = lnFirstEntry; i >= lnLastEntry; i = i - ENTRY_LEN)
    {                                              
      for (uint16_t n = i + 1; n < i + 11; n++)
        {                                  
          if ((sectorArr[n] == 0) || (sectorArr[n] > 126));
          else 
            { 
              lnStr[*lnStrIndx] = sectorArr[n];
              (*lnStrIndx)++;  
            }
        }

      for (uint16_t n = i + 14; n < i + 26; n++)
        {                                  
          if ((sectorArr[n] == 0) || (sectorArr[n] > 126));
          else 
            { 
              lnStr[*lnStrIndx] = sectorArr[n];
              (*lnStrIndx)++;  
            }
        }
      
      for (uint16_t n = i + 28; n < i + 32; n++)
        {                                  
          if ((sectorArr[n] == 0) || (sectorArr[n] > 126));
          else 
            { 
              lnStr[*lnStrIndx] = sectorArr[n];  
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
pvt_GetNextClusterIndex (uint32_t currClusIndx, BPB * bpb)
{
  uint8_t  bytesPerClusIndx = 4; // for FAT32
  uint16_t numOfIndexedClustersPerSecOfFat = bpb->bytesPerSec / bytesPerClusIndx; // = 128

  uint32_t clusIndx = currClusIndx / numOfIndexedClustersPerSecOfFat;
  uint32_t clusIndxStartByte = 4 * (currClusIndx % numOfIndexedClustersPerSecOfFat);
  uint32_t cluster = 0;

  uint32_t fatSectorToRead = clusIndx + bpb->rsvdSecCnt;

  uint8_t sectorArr[bpb->bytesPerSec];
  
  FATtoDisk_ReadSingleSector (fatSectorToRead, sectorArr);

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
 * Description : Used by FAT_PrintDirectory() to print the fields associated with an entry (e.g. creation/last modified
 *               date/time, file size, etc...). Which fields are printed is determined by the entryFilter flags set.
 * 
 * Arguments   : *sectorArr     - Pointer to an array that holds the short name of the entry that is being printed to
 *                                the screen. Only the short name entry of a short name/long name combination holds
 *                                the values of these fields.
 *             : entryPos       - Integer specifying the location in *sectorArr of the first byte of the short name 
 *                                entry whose fields should be printed to the screen.
 *             : entryFilter    - Byte specifying the entryFlag settings. This is used to determined which fields of 
 *                                the entry will be printed.
***********************************************************************************************************************
*/

void 
pvt_PrintEntryFields (uint8_t *sectorArr, uint16_t entryPos, uint8_t entryFilter)
{
  uint16_t creationTime;
  uint16_t creationDate;
  uint16_t lastAccessDate;
  uint16_t writeTime;
  uint16_t writeDate;
  uint32_t fileSize;

  // Load fields with values from sectorArr

  if (CREATION & entryFilter)
    {
      creationTime = sectorArr[entryPos + 15];
      creationTime <<= 8;
      creationTime |= sectorArr[entryPos + 14];
      
      creationDate = sectorArr[entryPos + 17];
      creationDate <<= 8;
      creationDate |= sectorArr[entryPos + 16];
    }

  if (LAST_ACCESS & entryFilter)
    {
      lastAccessDate = sectorArr[entryPos + 19];
      lastAccessDate <<= 8;
      lastAccessDate |= sectorArr[entryPos + 18];
    }

  if (LAST_MODIFIED & entryFilter)
    {
      writeTime = sectorArr[entryPos + 23];
      writeTime <<= 8;
      writeTime |= sectorArr[entryPos + 22];

      writeDate = sectorArr[entryPos + 25];
      writeDate <<= 8;
      writeDate |= sectorArr[entryPos + 24];
    }

  fileSize = sectorArr[entryPos + 31];
  fileSize <<= 8;
  fileSize |= sectorArr[entryPos + 30];
  fileSize <<= 8;
  fileSize |= sectorArr[entryPos + 29];
  fileSize <<= 8;
  fileSize |= sectorArr[entryPos + 28];

  print_str ("\n\r");

  // Print fields 

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
  if ((entryFilter & FILE_SIZE) == FILE_SIZE)
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
 * Description : Used by FAT_PrintDirectory to print the short name of a FAT file or directory.
 * 
 * Arguments   : *sectorArr     - Pointer to an array that holds the short name of the entry that is to be printed to
 *                                the screen.
 *             : entryPos       - Location in *sectorArr of the first byte of the short name entry that is to be
 *                                printed to the screen.
 *             : entryFilter    - Byte specifying the entryFlag settings. This is used to determined here if the TYPE 
 *                                flag has been passed
***********************************************************************************************************************
*/

void 
pvt_PrintShortName (uint8_t *sectorArr, uint16_t entryPos, uint8_t entryFilter)
{
  char sn[9];
  char ext[5];

  for (uint8_t k = 0; k < 8; k++) sn[k] = ' ';
  sn[8] = '\0';

  uint8_t attr = sectorArr[entryPos + 11];
  if (attr & 0x10)
    {
      if ((entryFilter & TYPE) == TYPE) print_str (" <DIR>   ");
      for (uint8_t k = 0; k < 8; k++) sn[k] = sectorArr[entryPos + k];
      print_str (sn);
      print_str ("    ");
    }
  else 
    {
      if ((entryFilter & TYPE) == TYPE) print_str (" <FILE>  ");
      // initialize extension char array
      strcpy (ext, ".   ");
      for (uint8_t k = 1; k < 4; k++) ext[k] = sectorArr[entryPos + 7 + k];
      for (uint8_t k = 0; k < 8; k++) 
        {
          sn[k] = sectorArr[k + entryPos];
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
 * Description : Used by FAT_PrintFile() to perform that actual print operation.
 * 
 * Arguments   : entryPos       - Integer that points to the location in *fileSector of the first byte of the short name entryPos of the file whose 
 *                                contents will be printed to the screen. This is required as the first cluster index 
 *                                of the file is located in the short name entryPos.
 *             : *fileSector      pointer to an array that holds the short name entryPos of the file to be printed to the 
 *                                to the screen.
***********************************************************************************************************************
*/

uint8_t 
pvt_PrintFatFile (uint16_t entryPos, uint8_t *fileSector, BPB * bpb)
  {
    uint32_t currSecNumPhys;
    uint32_t cluster;
    uint8_t  err;

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

            // function returns either 0 for success for 1 for failed.
            err = FATtoDisk_ReadSingleSector (currSecNumPhys, fileSector);
            if (err == 1) return FAILED_READ_SECTOR;

            for (uint16_t k = 0; k < bpb->bytesPerSec; k++)
              {
                // just for formatting how this shows up on the screen.
                if (fileSector[k] == '\n') print_str ("\n\r");
                else if (fileSector[k] != 0) USART_Transmit (fileSector[k]);
              }
          }
      } 
    while (((cluster = pvt_GetNextClusterIndex(cluster,bpb)) != END_OF_CLUSTER));
    return END_OF_FILE;
  }



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
 * Arguments   : lnFlags            - Byte that is holding the setting of the three long name flags: LONG_NAME_EXISTS,
 *                                    LONG_NAME_CROSSES_SECTOR_BOUNDARY, and LONG_NAME_LAST_SECTOR_ENTRY. The long 
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

uint8_t 
pvt_CorrectEntryCheck (uint8_t lnFlags, uint16_t * entryPos, uint16_t * snPosCurrSec, uint16_t * snPosNextSec)
{  
  if (lnFlags & LONG_NAME_EXISTS)
    {
      if ((*snPosCurrSec) >= (SECTOR_LEN - ENTRY_LEN))
        {
          if ((*entryPos) != 0) return 1; // need to get the next sector
          else (*snPosCurrSec) = -ENTRY_LEN;
        }

      if (lnFlags & (LONG_NAME_CROSSES_SECTOR_BOUNDARY | LONG_NAME_LAST_SECTOR_ENTRY))
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



/*
***********************************************************************************************************************
 *                                       (PRIVATE) SET THE LONG NAME FLAGS
 *
 * Description : Used by the FAT functions to set the long name flags if it was determined that a long name exists for 
 *               the current entry begin checked/read-in. This also sets the variable snPosCurrSec.
 * 
 * Arguments   : *lnFlags           - Pointer to a byte that will hold the long name flag settings: LONG_NAME_EXISTS,
 *                                    LONG_NAME_CROSSES_SECTOR_BOUNDARY, and LONG_NAME_LAST_SECTOR_ENTRY. This function
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

void
pvt_SetLongNameFlags ( uint8_t * lnFlags, uint16_t entryPos, uint16_t * snPosCurrSec, uint8_t * currSecArr, BPB * bpb)
{
  *lnFlags |= LONG_NAME_EXISTS;

  // number of entries required by the long name
  uint8_t longNameOrder = LONG_NAME_ORDINAL_MASK & currSecArr[entryPos];                 
  *snPosCurrSec = entryPos + (ENTRY_LEN * longNameOrder);
  
  // If short name position is greater than 511 then the short name is in the next sector.
  if ((*snPosCurrSec) >= bpb->bytesPerSec)
    {
      if ((*snPosCurrSec) > bpb->bytesPerSec) *lnFlags |= LONG_NAME_CROSSES_SECTOR_BOUNDARY;
      else if (*snPosCurrSec == SECTOR_LEN)   *lnFlags |= LONG_NAME_LAST_SECTOR_ENTRY;
    }
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
pvt_GetNextSector (uint8_t * nextSecArr, uint32_t currSecNumInClus, 
                   uint32_t currSecNumPhys, uint32_t clusIndx, BPB * bpb)
{
  uint32_t nextSecNumPhys;
  
  if (currSecNumInClus >= (bpb->secPerClus - 1)) 
    nextSecNumPhys = bpb->dataRegionFirstSector + ((pvt_GetNextClusterIndex (clusIndx, bpb) - 2) * bpb->secPerClus);
  else 
    nextSecNumPhys = 1 + currSecNumPhys;

  // function returns either 0 for success for 1 for failed.
  if (FATtoDisk_ReadSingleSector (nextSecNumPhys, nextSecArr) == 1) return FAILED_READ_SECTOR;
  else return SUCCESS;
}
