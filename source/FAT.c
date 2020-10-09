/******************************************************************************
 * Copyright (c) 2020 Joshua Fain
 * 
 * 
 * FAT.C
 * 
 * 
 * DESCRIPTION
 * Defines functions to interact with a FAT32 formatted disk/volume.
 * 
 * 
 * TARGET
 * ATmega1280
 *****************************************************************************/



#include <string.h>
#include <avr/io.h>
#include "../includes/fattosd.h"
#include "../includes/fat.h"
#include "../includes/prints.h"
#include "../includes/usart.h"



/******************************************************************************
 *                        "PRIVATE" FUNCTION DECLARATIONS
******************************************************************************/
uint32_t pvt_GetNextCluster(uint32_t currentCluster, BiosParameterBlock * bpb);
void pvt_PrintEntryFields(uint8_t *byte, uint16_t entry, uint8_t entryFilter);
void pvt_PrintShortNameAndType(uint8_t *byte, uint16_t entry, uint8_t attr);
void pvt_PrintFatFile(uint16_t entry, uint8_t *byte, BiosParameterBlock * bpb);



/******************************************************************************
 *                         "PUBLIC" FUNCTION DEFINITIONS
******************************************************************************/

// Set currentDirectory to newDirectoryStr if found.
// Return a Fat Error Flag
uint16_t SetFatCurrentDirectory(FatCurrentDirectory * currentDirectory, char * newDirectoryStr, BiosParameterBlock * bpb)
{
  uint8_t newDirStrLen = strlen(newDirectoryStr);
    
  // BEGIN: Legal name verification
  if ((strcmp(newDirectoryStr,"") == 0) ) return INVALID_DIR_NAME;
  if ( newDirectoryStr[0] == ' ') return INVALID_DIR_NAME;
  
  for (uint8_t k = 0; k < newDirStrLen; k++)
  {       
    if( ( newDirectoryStr[k] == 92 /* '\' */) || 
        ( newDirectoryStr[k] == '/' ) ||
        ( newDirectoryStr[k] == ':' ) ||
        ( newDirectoryStr[k] == '*' ) ||
        ( newDirectoryStr[k] == '?' ) ||
        ( newDirectoryStr[k] == '"' ) ||
        ( newDirectoryStr[k] == '<' ) ||
        ( newDirectoryStr[k] == '>' ) ||
        ( newDirectoryStr[k] == '|' )   )
    {
      return INVALID_DIR_NAME;
    }
  }

  uint8_t allSpacesFlag = 1;
  for (uint8_t k = 0; k < newDirStrLen; k++) 
  { 
    if(newDirectoryStr[k] != ' ') {  allSpacesFlag = 0;  break; }
  }
  if ( allSpacesFlag == 1 ) return INVALID_DIR_NAME;
  // END: Legal name verification


  uint32_t absoluteSectorNumber;
  uint32_t cluster = currentDirectory->FATFirstCluster;

  uint8_t currentSectorContents[bpb->bytesPerSector]; 
  uint8_t nextSectorContents[bpb->bytesPerSector];

  uint16_t shortNamePositionInCurrentSector = 0;
  uint16_t shortNamePositionInNextSector    = 0;

  uint8_t  attributeByte; // for the DIR_Attr byte of an entry

  char    longNameStr[LONG_NAME_MAX_LEN];
  uint8_t longNameStrIndex = 0;

  // Long Name flags
  uint8_t longNameExistsFlag = 0; 
  uint8_t longNameCrossSectorBoundaryFlag = 0;
  uint8_t longNameLastSectorEntryFlag = 0;
  

  // BEGIN: check if new directory is the current or parent directory
  if (!strcmp(newDirectoryStr,".")) return SUCCESS; // Current Directory
    
  if(!strcmp(newDirectoryStr,"..")) // Parent Dirctory
  {
    uint32_t parentDirectoryFirstCluster;

    absoluteSectorNumber = bpb->dataRegionFirstSector + ( (cluster - 2) * bpb->sectorsPerCluster);

    fat_ReadSingleSector( absoluteSectorNumber, currentSectorContents);

    parentDirectoryFirstCluster = currentSectorContents[53];
    parentDirectoryFirstCluster <<= 8;
    parentDirectoryFirstCluster |= currentSectorContents[52];
    parentDirectoryFirstCluster <<= 8;
    parentDirectoryFirstCluster |= currentSectorContents[59];
    parentDirectoryFirstCluster <<= 8;
    parentDirectoryFirstCluster |= currentSectorContents[58];

    // current directory is root directory? Do Nothing. 
    if(currentDirectory->FATFirstCluster == bpb->rootCluster); 

    // parent directory is root directory?
    else if(parentDirectoryFirstCluster == 0)
    {
      strcpy(currentDirectory->shortName,"/");
      strcpy(currentDirectory->shortParentPath,"");
      strcpy(currentDirectory->longName,"/");
      strcpy(currentDirectory->longParentPath,"");
      currentDirectory->FATFirstCluster = bpb->rootCluster;
    }

    else
    {
      // update currentDirectory struct members for parent.
      // Done by using the values of the directory in the file path.
      
      currentDirectory->FATFirstCluster = parentDirectoryFirstCluster;
      
      char tmpShortNamePath[64];
      char tmpLongNamePath[64];

      strlcpy(tmpShortNamePath, currentDirectory->shortParentPath, strlen(currentDirectory->shortParentPath)); 

      strlcpy(tmpLongNamePath, currentDirectory->longParentPath, strlen(currentDirectory->longParentPath));
      
      char *shortLastDirectoryInPath = strrchr(tmpShortNamePath, '/');
      char *longLastDirectoryInPath  = strrchr(tmpLongNamePath , '/');
      
      strcpy(currentDirectory->shortName, shortLastDirectoryInPath + 1);
      strcpy(currentDirectory->longName ,  longLastDirectoryInPath + 1);

      strlcpy(currentDirectory->shortParentPath,tmpShortNamePath, (shortLastDirectoryInPath + 2) - tmpShortNamePath);
      strlcpy(currentDirectory->longParentPath, tmpLongNamePath , (longLastDirectoryInPath  + 2) - tmpLongNamePath );
    }
    return SUCCESS;
  }
  // END: check if new directory is the current or parent directory

    
  // BEGIN: search for new directory as a child of the current directory
  do
  {
    // possilbe to have clusters number larger than 32-bit can hold
    for(uint32_t clusterSectorNumber = 0; clusterSectorNumber < bpb->sectorsPerCluster; clusterSectorNumber++)
    {         
      // get currentSectorContents[]
      absoluteSectorNumber = clusterSectorNumber + bpb->dataRegionFirstSector + ((cluster - 2) * bpb->sectorsPerCluster);
      fat_ReadSingleSector( absoluteSectorNumber, currentSectorContents );

      for(int entry = 0; entry < SECTOR_LEN; entry = entry + ENTRY_LEN)
      {
        // ensure 'entry' is pointing at correct location in sector
        if(longNameExistsFlag)  
        {
          if (shortNamePositionInCurrentSector >= (SECTOR_LEN - ENTRY_LEN)) //480
          {
            if ( entry != 0)  break;
            else shortNamePositionInCurrentSector = -ENTRY_LEN; // -32 used to adjust entry
          }

          if( (longNameCrossSectorBoundaryFlag || longNameLastSectorEntryFlag) )
          {
            entry = shortNamePositionInNextSector + ENTRY_LEN;
            shortNamePositionInNextSector = 0;
            longNameCrossSectorBoundaryFlag = 0;
            longNameLastSectorEntryFlag = 0;
          }

          else 
          {
            entry = shortNamePositionInCurrentSector + ENTRY_LEN;
            shortNamePositionInCurrentSector = 0;
          }
          longNameExistsFlag = 0;
        }

        // 0xE5 = marked for deletion
        if( currentSectorContents[entry] == 0xE5 );

        // all subsequent entries are empty.
        else if ( currentSectorContents[entry] == 0 ) return END_OF_DIRECTORY;
        else
        {                
          attributeByte = currentSectorContents[entry + 11];

          // Long Name?
          if( (attributeByte & LONG_NAME_ATTR_MASK) == LONG_NAME_ATTR_MASK )
          {
            // confirm long name last entry flag is set for this entry
            if( !(currentSectorContents[entry] & LONG_NAME_LAST_ENTRY_FLAG) ) return CORRUPT_FAT_ENTRY; 
            else
            {
              longNameExistsFlag = 1;
              for(uint8_t k = 0; k < LONG_NAME_MAX_LEN; k++) longNameStr[k] = '\0';
              // number of entries required for the long name
              uint8_t longNameOrder = LONG_NAME_ORDINAL_MASK & currentSectorContents[entry];

              shortNamePositionInCurrentSector = entry + (ENTRY_LEN * longNameOrder);
              
              // short name is in next sector?
              if (shortNamePositionInCurrentSector >= bpb->bytesPerSector)
              {
                // long name crosses sector boundary?
                if (shortNamePositionInCurrentSector > bpb->bytesPerSector)
                {
                  longNameCrossSectorBoundaryFlag = 1;
                  longNameLastSectorEntryFlag = 0;
                }

                // entire long name is in current sector?
                else if (shortNamePositionInCurrentSector == bpb->bytesPerSector)
                {
                  longNameCrossSectorBoundaryFlag = 0;
                  longNameLastSectorEntryFlag = 1;
                }
                else return CORRUPT_FAT_ENTRY;

                //get next sector's contents
                uint32_t nextSector;
                if (clusterSectorNumber >= bpb->bytesPerSector - 1) 
                    nextSector = bpb->dataRegionFirstSector + ( (pvt_GetNextCluster(cluster,bpb) - 2) * bpb->bytesPerSector );
                else nextSector = 1 + absoluteSectorNumber;
                fat_ReadSingleSector( nextSector, nextSectorContents);

                // short name start position in the next sector
                shortNamePositionInNextSector = shortNamePositionInCurrentSector - bpb->bytesPerSector;

                attributeByte = nextSectorContents[shortNamePositionInNextSector + 11];
                
                // If not a directory entry, move on to next entry
                if( !(attributeByte & DIRECTORY_ENTRY_FLAG) );

                // shortNamePositionInNextSector points to long name entry?
                if ( (attributeByte & LONG_NAME_ATTR_MASK) == LONG_NAME_ATTR_MASK ) return CORRUPT_FAT_ENTRY;
                else
                {                                                           
                  // Long name crosses sector boundary?
                  if( (longNameCrossSectorBoundaryFlag == 1) && (longNameLastSectorEntryFlag == 0) )
                  {
                    // Confirm entry preceding short name is first entry of a long name. 
                    // Value in ordinal position must be 1, but mask out possible LONG_NAME_LAST_ENTRY_FLAG
                    if( (nextSectorContents[shortNamePositionInNextSector - ENTRY_LEN] & LONG_NAME_ORDINAL_MASK) != 1) return CORRUPT_FAT_ENTRY;                                      
                    else
                    {
                      longNameStrIndex = 0;   

                      // load long name entry into longNameStr[]
                      for(int i = (shortNamePositionInNextSector - ENTRY_LEN); i >= 0; i = i - ENTRY_LEN)
                      {
                        for(uint16_t n = i + 1; n < i + 11; n++)
                        {                                  
                          if(nextSectorContents[n] == 0 || nextSectorContents[n] > 126);
                          else { longNameStr[longNameStrIndex] = nextSectorContents[n];  longNameStrIndex++;  }
                        }

                        for(uint16_t n = i + 14; n < i + 26; n++)
                        {                                  
                          if(nextSectorContents[n] == 0 || nextSectorContents[n] > 126);
                          else { longNameStr[longNameStrIndex] = nextSectorContents[n];  longNameStrIndex++;  }
                        }
                        
                        for(uint16_t n = i + 28; n < i + 32; n++)
                        {                                  
                          if(nextSectorContents[n] == 0 || nextSectorContents[n] > 126);
                          else { longNameStr[longNameStrIndex] = nextSectorContents[n];  longNameStrIndex++;  }
                        }            
                      }
                    
                      for(int i = SECTOR_LEN - ENTRY_LEN; i >= entry; i = i - ENTRY_LEN)
                      {                                
                        for(uint16_t n = i + 1; n < i + 11; n++)
                        {                                  
                          if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                          else { longNameStr[longNameStrIndex] = currentSectorContents[n];  longNameStrIndex++; }
                        }
                        
                        for(uint16_t n = i + 14; n < i + 26; n++)
                        {   
                          if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                          else { longNameStr[longNameStrIndex] = currentSectorContents[n];  longNameStrIndex++; }
                        }
                        
                        for(uint16_t n = i + 28; n < i + 32; n++)
                        {                                  
                          if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                          else { longNameStr[longNameStrIndex] = currentSectorContents[n];  longNameStrIndex++; }
                        }
                      }

                      // if match, then update currentDirectory members
                      if(!strcmp(newDirectoryStr,longNameStr)) 
                      {                                                        
                        uint32_t dirFstClus;
                        dirFstClus = nextSectorContents[shortNamePositionInNextSector + 21];
                        dirFstClus <<= 8;
                        dirFstClus |= nextSectorContents[shortNamePositionInNextSector + 20];
                        dirFstClus <<= 8;
                        dirFstClus |= nextSectorContents[shortNamePositionInNextSector + 27];
                        dirFstClus <<= 8;
                        dirFstClus |= nextSectorContents[shortNamePositionInNextSector + 26];

                        currentDirectory->FATFirstCluster = dirFstClus;

                        char sn[9];                                                     
                        for(uint8_t k = 0; k < 8; k++) sn[k] = nextSectorContents[shortNamePositionInNextSector + k];
                        sn[8] = '\0';

                        strcat(currentDirectory->longParentPath , currentDirectory->longName );
                        strcat(currentDirectory->shortParentPath, currentDirectory->shortName);

                        // if current directory is not root, append '/'
                        if(currentDirectory->longName[0] != '/') strcat(currentDirectory->longParentPath,"/"); 
                        strcpy(currentDirectory->longName,newDirectoryStr);
                        if(currentDirectory->shortName[0] != '/') strcat(currentDirectory->shortParentPath,"/");
                        strcpy(currentDirectory->shortName, sn);

                        return SUCCESS;
                      }
                    }
                  }

                  // all entries for long name are in current sector but short name is in next sector
                  else if(longNameCrossSectorBoundaryFlag == 0 && longNameLastSectorEntryFlag == 1)
                  {
                    longNameStrIndex = 0;

                    // confirm last entry of current sector is the first entry of the long name
                    if( (currentSectorContents[SECTOR_LEN - ENTRY_LEN] & LONG_NAME_ORDINAL_MASK) != 1) return CORRUPT_FAT_ENTRY;
                    else
                    {                               
                      // load long name entry into longNameStr[]
                      for(int i = (SECTOR_LEN - ENTRY_LEN) ; i >= entry ; i = i - ENTRY_LEN)
                      {                                
                        for(uint16_t n = i + 1; n < i + 11; n++)
                        {                                  
                          if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                          else { longNameStr[longNameStrIndex] = currentSectorContents[n]; longNameStrIndex++; }
                        }
                        
                        for(uint16_t n = i + 14; n < i + 26; n++)
                        {   
                          if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                          else { longNameStr[longNameStrIndex] = currentSectorContents[n]; longNameStrIndex++; }
                        }
                        
                        for(uint16_t n = i + 28; n < i + 32; n++)
                        {                                  
                          if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                          else { longNameStr[longNameStrIndex] = currentSectorContents[n]; longNameStrIndex++; }
                        }
                      }

                      // if match, then update currentDirectory members
                      if(!strcmp(newDirectoryStr,longNameStr)) 
                      { 
                        uint32_t dirFstClus;
                        dirFstClus = nextSectorContents[shortNamePositionInNextSector + 21];
                        dirFstClus <<= 8;
                        dirFstClus |= nextSectorContents[shortNamePositionInNextSector + 20];
                        dirFstClus <<= 8;
                        dirFstClus |= nextSectorContents[shortNamePositionInNextSector + 27];
                        dirFstClus <<= 8;
                        dirFstClus |= nextSectorContents[shortNamePositionInNextSector + 26];

                        currentDirectory->FATFirstCluster = dirFstClus;

                        char sn[9];                                                     
                        for(int k = 0; k < 8; k++) sn[k] = nextSectorContents[shortNamePositionInNextSector + k];
                        sn[8] = '\0';

                        strcat(currentDirectory->longParentPath , currentDirectory->longName );
                        strcat(currentDirectory->shortParentPath, currentDirectory->shortName);

                        // if current directory is not root, append '/' 
                        if(currentDirectory->longName[0] != '/') strcat(currentDirectory->longParentPath,"/"); 
                        strcpy(currentDirectory->longName,newDirectoryStr);
                        if(currentDirectory->shortName[0] != '/') strcat(currentDirectory->shortParentPath,"/");
                        strcpy(currentDirectory->shortName,sn);

                        return SUCCESS;
                      }
                    }
                  }
                  else return CORRUPT_FAT_ENTRY;
                }
              }

              else // Long name exists and is entirely in current sector along with the short name
              {   
                attributeByte = currentSectorContents[shortNamePositionInCurrentSector + 11];
                
                // If not a directory entry, move on to next entry.
                if( !(attributeByte & DIRECTORY_ENTRY_FLAG) );

                // Confirm entry preceding short name is first entry of a long name.
                if( (currentSectorContents[shortNamePositionInCurrentSector - ENTRY_LEN] & LONG_NAME_ORDINAL_MASK) != 1) return CORRUPT_FAT_ENTRY;
                else
                {
                  longNameStrIndex = 0;

                  // load long name entry into longNameStr[]
                  for(int i = shortNamePositionInCurrentSector - ENTRY_LEN; i >= entry; i = i - ENTRY_LEN)
                  {                                
                    for(uint16_t n = i + 1; n < i + 11; n++)
                    {                                  
                      if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                      else { longNameStr[longNameStrIndex] = currentSectorContents[n];  longNameStrIndex++; }
                    }
                    
                    for(uint16_t n = i + 14; n < i + 26; n++)
                    {   
                      if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                      else { longNameStr[longNameStrIndex] = currentSectorContents[n];  longNameStrIndex++; }
                    }
                    
                    for(uint16_t n = i + 28; n < i + 32; n++)
                    {                                  
                      if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                      else { longNameStr[longNameStrIndex] = currentSectorContents[n];  longNameStrIndex++; }
                    }
                  }
                  
                  // if match, then update currentDirectory members
                  if(!strcmp(newDirectoryStr,longNameStr)) 
                  { 
                    uint32_t dirFstClus;
                    dirFstClus = currentSectorContents[shortNamePositionInCurrentSector+21];
                    dirFstClus <<= 8;
                    dirFstClus |= currentSectorContents[shortNamePositionInCurrentSector+20];
                    dirFstClus <<= 8;
                    dirFstClus |= currentSectorContents[shortNamePositionInCurrentSector+27];
                    dirFstClus <<= 8;
                    dirFstClus |= currentSectorContents[shortNamePositionInCurrentSector+26];

                    currentDirectory->FATFirstCluster = dirFstClus;
                    
                    char sn[9];                                    
                    for(uint8_t k = 0; k < 8; k++)  sn[k] = currentSectorContents[shortNamePositionInCurrentSector + k];
                    sn[8] = '\0';

                    strcat(currentDirectory->longParentPath , currentDirectory->longName );
                    strcat(currentDirectory->shortParentPath, currentDirectory->shortName);

                    // if current directory is not root then append '/'
                    if(currentDirectory->longName[0] != '/') strcat(currentDirectory->longParentPath,"/"); 
                    strcpy(currentDirectory->longName,newDirectoryStr);
                    if(currentDirectory->shortName[0] != '/') strcat(currentDirectory->shortParentPath,"/");
                    strcpy(currentDirectory->shortName,sn);

                    return SUCCESS;
                  }                                       
                }
              }
            }
          }                   

          else  // Long Name Entry does not exist
          {
            attributeByte = currentSectorContents[entry + 11];

            // If not a directory entry, move on to next entry.
            if( !(attributeByte & DIRECTORY_ENTRY_FLAG) );

            // newDirectoryStr is too long for a short name
            else if(newDirStrLen > 8);

            else 
            {                   
              char sn[9];
      
              char tempDir[9];
              strcpy(tempDir,newDirectoryStr);

              for(uint8_t k = 0; k < newDirStrLen; k++)  {  sn[k] = currentSectorContents[k+entry];  }
              sn[newDirStrLen] = '\0';

              // if match, then update currentDirectory members
              if(!strcmp(tempDir,sn)) 
              { 
                uint32_t dirFstClus;
                dirFstClus = currentSectorContents[entry + 21];
                dirFstClus <<= 8;
                dirFstClus |= currentSectorContents[entry + 20];
                dirFstClus <<= 8;
                dirFstClus |= currentSectorContents[entry + 27];
                dirFstClus <<= 8;
                dirFstClus |= currentSectorContents[entry + 26];

                currentDirectory->FATFirstCluster = dirFstClus;
                
                strcat(currentDirectory->longParentPath , currentDirectory->longName );
                strcat(currentDirectory->shortParentPath, currentDirectory->shortName);
                
                // if current directory is not root then append '/'
                if(currentDirectory->longName[0] != '/') strcat(currentDirectory->longParentPath,"/");
                strcpy(currentDirectory->longName,newDirectoryStr);
                if(currentDirectory->shortName[0] != '/') strcat(currentDirectory->shortParentPath,"/");
                strcpy(currentDirectory->shortName,sn);

                return SUCCESS;
              }
            }
          }
        }
      }
    }
  } while( ( (cluster = pvt_GetNextCluster(cluster,bpb)) != END_OF_CLUSTER ) );
  // END: search for new directory as a child of the current directory
  
  return END_OF_DIRECTORY;
}



// Prints long and/or short name entries found in the current directory as well
// as prints the entry's associated fields as specified by entryFilter.
// Returns a Fat Error Flag
uint16_t PrintFatCurrentDirectoryContents(FatCurrentDirectory * currentDirectory, uint8_t entryFilter, BiosParameterBlock * bpb)
{
  uint32_t absoluteSectorNumber;
  uint32_t cluster = currentDirectory->FATFirstCluster;

  uint8_t currentSectorContents[bpb->bytesPerSector]; 
  uint8_t nextSectorContents[bpb->bytesPerSector];

  uint16_t shortNamePositionInCurrentSector = 0;
  uint16_t shortNamePositionInNextSector    = 0;

  char    longNameStr[LONG_NAME_MAX_LEN];
  uint8_t longNameStrIndex = 0;

  uint8_t  attributeByte; // for the DIR_Attr byte of an entry

  // long name flags. Set to 1 if true for current name
  uint8_t longNameExistsFlag = 0; 
  uint8_t longNameCrossSectorBoundaryFlag = 0;
  uint8_t longNameLastSectorEntryFlag = 0;

  // Prints column headers according to entryFilter
  print_str("\n\n\r");
  if(CREATION & entryFilter)  print_str(" CREATION DATE & TIME,");
  if(LAST_ACCESS & entryFilter)  print_str(" LAST ACCESS DATE,");
  if(LAST_MODIFIED & entryFilter)  print_str(" LAST MODIFIED DATE & TIME,");
  print_str(" SIZE, TYPE, NAME");
  print_str("\n\n\r");
  

  // BEGIN: Print entries in the current directory
  do 
  {
    for(uint32_t clusterSectorNumber = 0; clusterSectorNumber < bpb->sectorsPerCluster; clusterSectorNumber++)
    {
      // read in next sector's contents into currentSectorContents[] array 
      absoluteSectorNumber = clusterSectorNumber + bpb->dataRegionFirstSector + ((cluster - 2) * bpb->sectorsPerCluster);

      fat_ReadSingleSector(absoluteSectorNumber, currentSectorContents);

      for(uint16_t entry = 0; entry < bpb->bytesPerSector; entry = entry + ENTRY_LEN)
      {
        // ensure 'entry' is pointing at correct location in current sector.
        if( longNameExistsFlag )
        {
          if (shortNamePositionInCurrentSector >= (SECTOR_LEN - ENTRY_LEN) )
          {
            if ( entry != 0) break;
            else shortNamePositionInCurrentSector = -ENTRY_LEN; // used to adjust this value if needed
          }

          if( (longNameCrossSectorBoundaryFlag || longNameLastSectorEntryFlag) )
          {
            entry = shortNamePositionInNextSector + ENTRY_LEN;
            shortNamePositionInNextSector = 0;
            longNameCrossSectorBoundaryFlag = 0;
            longNameLastSectorEntryFlag = 0;
          }

          else 
          {
            entry = shortNamePositionInCurrentSector + ENTRY_LEN;
            shortNamePositionInCurrentSector = 0;
          }
          longNameExistsFlag = 0;
        }

        // this entry marked for deletion. Go to next entry.
        if( currentSectorContents[entry] == 0xE5 );

        // all subsequent entries are empty.
        else if ( currentSectorContents[entry] == 0 ) return END_OF_DIRECTORY;

        else
        {                
          attributeByte = currentSectorContents[entry + 11];

          //long name?
          if( (LONG_NAME_ATTR_MASK & attributeByte) == LONG_NAME_ATTR_MASK )
          {
            //confirm long name last entry flag is set
            if( !(currentSectorContents[entry] & LONG_NAME_LAST_ENTRY_FLAG) ) return CORRUPT_FAT_ENTRY;
                                    
            longNameExistsFlag = 1;
            
            for(uint8_t k = 0; k < LONG_NAME_MAX_LEN; k++) longNameStr[k] = '\0';

            // number of entries required by the long name
            uint8_t longNameOrder = LONG_NAME_ORDINAL_MASK & currentSectorContents[entry];

            shortNamePositionInCurrentSector = entry + (ENTRY_LEN * longNameOrder);

            // short name in next sector?
            if (shortNamePositionInCurrentSector >= bpb->bytesPerSector)
            {
              // long name crosses sector boundary?
              if (shortNamePositionInCurrentSector > bpb->bytesPerSector)
              {
                  longNameCrossSectorBoundaryFlag = 1;
                  longNameLastSectorEntryFlag = 0;
              }

              // entire long name in the current sector?
              else if (shortNamePositionInCurrentSector == bpb->bytesPerSector)
              {
                  longNameCrossSectorBoundaryFlag = 0;
                  longNameLastSectorEntryFlag = 1;
              }
              else return CORRUPT_FAT_ENTRY;

              //get next sector's contents
              uint32_t nextSector;
              if (clusterSectorNumber >= bpb->sectorsPerCluster - 1) 
                nextSector = bpb->dataRegionFirstSector + ((pvt_GetNextCluster(cluster,bpb) - 2) * bpb->sectorsPerCluster);
              else nextSector = 1 + absoluteSectorNumber;
              fat_ReadSingleSector(nextSector, nextSectorContents);

              // short name position in the next sector
              shortNamePositionInNextSector = shortNamePositionInCurrentSector - bpb->bytesPerSector;

              attributeByte = nextSectorContents[shortNamePositionInNextSector + 11];
              
              // shortNamePositionInNextSector points to long name entry?
              if ( (attributeByte & LONG_NAME_ATTR_MASK) == LONG_NAME_ATTR_MASK ) return CORRUPT_FAT_ENTRY;

              // don't print if entry is marked hidden and the HIDDEN entry filter is not set 
              if ( ((attributeByte & HIDDEN_ATTR_FLAG) == HIDDEN_ATTR_FLAG) && ((entryFilter & HIDDEN) != HIDDEN) ); 
              else
              {                                                           
                if( (entryFilter & SHORT_NAME) != SHORT_NAME );
                else
                {
                  pvt_PrintEntryFields(nextSectorContents, shortNamePositionInNextSector, entryFilter);
                  pvt_PrintShortNameAndType(nextSectorContents, shortNamePositionInNextSector, attributeByte);
                }
        
                if( (entryFilter & LONG_NAME) != LONG_NAME);
                else
                {
                  // entries for long name cross sector boundary
                  if(longNameCrossSectorBoundaryFlag == 1 && longNameLastSectorEntryFlag == 0)
                  {
                    // Confirm entry preceding short name is first entry of a long name.
                    if( (nextSectorContents[shortNamePositionInNextSector - ENTRY_LEN] & LONG_NAME_ORDINAL_MASK ) != 1 ) return CORRUPT_FAT_ENTRY;                                              

                    pvt_PrintEntryFields(nextSectorContents, shortNamePositionInNextSector, entryFilter);

                    longNameStrIndex = 0;   

                    if (attributeByte & DIRECTORY_ENTRY_FLAG) print_str("    <DIR>    ");
                    else print_str("   <FILE>    ");
                    
                    // load long name entry into longNameStr[]
                    for(int i = (shortNamePositionInNextSector - ENTRY_LEN); i >= 0; i = i - ENTRY_LEN) 
                    {
                      for(uint16_t n = i + 1; n < i + 11; n++)
                      {                                  
                        if(nextSectorContents[n] == 0 || nextSectorContents[n] > 126);
                        else { longNameStr[longNameStrIndex] = nextSectorContents[n];  longNameStrIndex++;  }
                      }

                      for(uint16_t n = i + 14; n < i + 26; n++)
                      {                                  
                        if(nextSectorContents[n] == 0 || nextSectorContents[n] > 126);
                        else { longNameStr[longNameStrIndex] = nextSectorContents[n];  longNameStrIndex++;  }
                      }
                      
                      for(uint16_t n = i + 28; n < i + 32; n++)
                      {                                  
                        if(nextSectorContents[n] == 0 || nextSectorContents[n] > 126);
                        else { longNameStr[longNameStrIndex] = nextSectorContents[n];  longNameStrIndex++;  }
                      }            
                    }

                    for(int i = (SECTOR_LEN - ENTRY_LEN) ; i >= entry ; i = i - ENTRY_LEN)
                    {                                
                      for(uint16_t n = i + 1; n < i + 11; n++)
                      {                                  
                        if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                        else { longNameStr[longNameStrIndex] = currentSectorContents[n];  longNameStrIndex++; }
                      }
                      
                      for(uint16_t n = i + 14; n < i + 26; n++)
                      {   
                        if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                        else { longNameStr[longNameStrIndex] = currentSectorContents[n];  longNameStrIndex++; }
                      }
                      
                      for(uint16_t n = i + 28; n < i + 32; n++)
                      {                                  
                        if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                        else { longNameStr[longNameStrIndex] = currentSectorContents[n];  longNameStrIndex++; }
                      }
                    }
                    print_str(longNameStr);
                  }

                  // all entries for long name are in current sector but short name is in next sector
                  else if(longNameCrossSectorBoundaryFlag == 0 && longNameLastSectorEntryFlag == 1)
                  {
                    longNameStrIndex = 0;

                    // confirm last entry of current sector is the first entry of the long name
                    if( (currentSectorContents[SECTOR_LEN - ENTRY_LEN] & LONG_NAME_ORDINAL_MASK) != 1) return CORRUPT_FAT_ENTRY;
          
                    pvt_PrintEntryFields(nextSectorContents, shortNamePositionInNextSector, entryFilter);

                    if(attributeByte & DIRECTORY_ENTRY_FLAG) print_str("    <DIR>    ");
                    else print_str("   <FILE>    ");
                    
                    // load long name entry into longNameStr[]
                    for(int i = (SECTOR_LEN - ENTRY_LEN); i >= entry; i = i - ENTRY_LEN)
                    {                                
                      for(uint16_t n = i + 1; n < i + 11; n++)
                      {                                  
                        if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                        else { longNameStr[longNameStrIndex] = currentSectorContents[n];  longNameStrIndex++; }
                      }
                      
                      for(uint16_t n = i + 14; n < i + 26; n++)
                      {   
                        if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                        else { longNameStr[longNameStrIndex] = currentSectorContents[n];  longNameStrIndex++; }
                      }
                      
                      for(uint16_t n = i + 28; n < i + 32; n++)
                      {                                  
                        if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                        else { longNameStr[longNameStrIndex] = currentSectorContents[n];  longNameStrIndex++; }
                      }
                    }
                    print_str(longNameStr); 
                  }
                  else return CORRUPT_FAT_ENTRY;
                }
              }
            }
            else // Long name exists and is entirely in current sector along with the short name
            {   
              attributeByte = currentSectorContents[shortNamePositionInCurrentSector + 11];
              
              // shortNamePositionInCurrentSector points to long name entry
              if ( (attributeByte & LONG_NAME_ATTR_MASK) == LONG_NAME_ATTR_MASK ) return CORRUPT_FAT_ENTRY;
              if ( ((attributeByte & HIDDEN_ATTR_FLAG) == HIDDEN_ATTR_FLAG) && ((entryFilter & HIDDEN) != HIDDEN) );
              else
              {                   
                if( (entryFilter & SHORT_NAME) != SHORT_NAME );
                else
                {
                  pvt_PrintEntryFields(currentSectorContents, shortNamePositionInCurrentSector, entryFilter);
                  pvt_PrintShortNameAndType(currentSectorContents, shortNamePositionInCurrentSector, attributeByte);
                }
                
                if( (entryFilter & LONG_NAME) != LONG_NAME );
                else
                {
                  // Confirm entry preceding short name is first entry of a long name.
                  if( (currentSectorContents[shortNamePositionInCurrentSector - ENTRY_LEN] & LONG_NAME_ORDINAL_MASK) != 1) return CORRUPT_FAT_ENTRY;

                  pvt_PrintEntryFields(currentSectorContents, shortNamePositionInCurrentSector, entryFilter);
                  
                  if(attributeByte & DIRECTORY_ENTRY_FLAG) print_str("    <DIR>    ");
                  else print_str("   <FILE>    ");

                  longNameStrIndex = 0;
                  // load long name entry into longNameStr[]
                  for(int i = shortNamePositionInCurrentSector - ENTRY_LEN ; i >= (int)entry ; i = i - ENTRY_LEN)
                  {
                    for(uint16_t n = i + 1; n < i + 11; n++)
                    {                                  
                      if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                      else { longNameStr[longNameStrIndex] = currentSectorContents[n]; longNameStrIndex++; }
                    }
                    
                    for(uint16_t n = i + 14; n < i + 26; n++)
                    {   
                      if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                      else { longNameStr[longNameStrIndex] = currentSectorContents[n]; longNameStrIndex++; }
                    }
                    
                    for(uint16_t n = i + 28; n < i + 32; n++)
                    {                                  
                      if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                      else { longNameStr[longNameStrIndex] = currentSectorContents[n]; longNameStrIndex++; }
                    }
                  }
                  print_str(longNameStr);                                       
                }
              }
            }
          }                   
          else  // Long Name Entry does not exist so use short name instead, regardless of SHORT_NAME entryFilter setting
          {
            attributeByte = currentSectorContents[entry + 11];
            if ( ((attributeByte & HIDDEN_ATTR_FLAG) == HIDDEN_ATTR_FLAG) && ((entryFilter & HIDDEN) != HIDDEN) );
            else 
            {
              pvt_PrintEntryFields(currentSectorContents, entry, entryFilter);
              pvt_PrintShortNameAndType(currentSectorContents, entry, attributeByte);
            }
          }
        }
      }
    }
  }while( ( (cluster = pvt_GetNextCluster(cluster,bpb) ) != END_OF_CLUSTER ));
  // END: Print entries in the current directory

  return END_OF_DIRECTORY;
}


// Prints the contents of file specified by *fileNameStr to the screen.
// Returns a Fat Error Flag
uint16_t PrintFatFileContents(FatCurrentDirectory * currentDirectory, char * fileNameStr, BiosParameterBlock * bpb)
{
  uint8_t fileNameStrLen = strlen(fileNameStr);

  // BEGIN: legal name verification
  if ((strcmp(fileNameStr,"") == 0) ) return INVALID_FILE_NAME;
  if ( fileNameStr[0] == ' ') return INVALID_FILE_NAME;
  
  for (uint8_t k = 0; k < fileNameStrLen; k++)
  {
    if( ( fileNameStr[k] == 92 /* '\' */ ) ||
        ( fileNameStr[k] == '/' ) ||
        ( fileNameStr[k] == ':' ) ||
        ( fileNameStr[k] == '*' ) ||
        ( fileNameStr[k] == '?' ) ||
        ( fileNameStr[k] == '"' ) ||
        ( fileNameStr[k] == '<' ) ||
        ( fileNameStr[k] == '>' ) ||
        ( fileNameStr[k] == '|' )   )
    {
      return INVALID_FILE_NAME;
    }
  }

  uint8_t allSpacesFlag = 1;
  for (uint8_t k = 0; k < fileNameStrLen; k++) 
  { 
    if(fileNameStr[k] != ' ') { allSpacesFlag = 0;  break; }
  }
  if ( allSpacesFlag == 1 ) return INVALID_FILE_NAME;
  // END: legal name verification


  uint32_t absoluteSectorNumber;
  uint32_t cluster = currentDirectory->FATFirstCluster;
  
  uint8_t currentSectorContents[bpb->bytesPerSector]; 
  uint8_t nextSectorContents[bpb->bytesPerSector];

  uint16_t shortNamePositionInCurrentSector = 0;
  uint16_t shortNamePositionInNextSector    = 0;

  char    longNameStr[LONG_NAME_MAX_LEN];
  uint8_t longNameStrIndex = 0;

  uint8_t  attributeByte;

  // long name flags. Set to 1 if true for current name
  uint8_t longNameExistsFlag = 0; 
  uint8_t longNameCrossSectorBoundaryFlag = 0;
  uint8_t longNameLastSectorEntryFlag = 0;


  // BEGIN: Search for, and print, contents of fileNameStr if match is found
  int clusCnt = 0;    
  do
  {
    clusCnt++;

    for(uint32_t clusterSectorNumber = 0; clusterSectorNumber < bpb->sectorsPerCluster; clusterSectorNumber++)
    {     
      absoluteSectorNumber = clusterSectorNumber + bpb->dataRegionFirstSector + ( (cluster - 2) * bpb->sectorsPerCluster );
      fat_ReadSingleSector( absoluteSectorNumber, currentSectorContents );

      for(uint16_t entry = 0; entry < bpb->bytesPerSector; entry = entry + ENTRY_LEN)
      { 
        // ensure 'entry' is pointing to correct location in current sector.
        if( longNameExistsFlag )
        {
          if (shortNamePositionInCurrentSector >= (SECTOR_LEN - ENTRY_LEN) )
          {
            if ( entry != 0) break;
            else shortNamePositionInCurrentSector = -ENTRY_LEN; // used to adjust the variable later.
          }

          if( ( longNameCrossSectorBoundaryFlag || longNameLastSectorEntryFlag) ) 
          {
            entry = shortNamePositionInNextSector + ENTRY_LEN; 
            shortNamePositionInNextSector = 0; 
            longNameCrossSectorBoundaryFlag = 0; 
            longNameLastSectorEntryFlag = 0;
          }

          else 
          {
            entry = shortNamePositionInCurrentSector + ENTRY_LEN; 
            shortNamePositionInCurrentSector = 0;
          }
          longNameExistsFlag = 0;
        }

        // this entry marked for deletion. Go to next entry.
        if( currentSectorContents[entry] == 0xE5 );

        // all subsequent entries are empty.
        else if ( currentSectorContents[entry] == 0 ) return FILE_NOT_FOUND;
        else
        {                
          attributeByte = currentSectorContents[entry + 11];

          // long name?
          if( (attributeByte & LONG_NAME_ATTR_MASK) == LONG_NAME_ATTR_MASK)
          {
            // confirm last long name entry flag is set
            if( !(currentSectorContents[entry] & LONG_NAME_LAST_ENTRY_FLAG) ) return CORRUPT_FAT_ENTRY;
            else
            {
              longNameExistsFlag = 1;

              for(uint8_t k = 0; k < LONG_NAME_MAX_LEN; k++) longNameStr[k] = '\0';
              
              // number of entries required by the long name
              uint8_t longNameOrder = LONG_NAME_ORDINAL_MASK & currentSectorContents[entry]; 
                                              
              shortNamePositionInCurrentSector = entry + (ENTRY_LEN * longNameOrder);
              
              // short name in next sector?
              if (shortNamePositionInCurrentSector >= bpb->bytesPerSector)
              {
                // Long name crosses sector boundary?
                if (shortNamePositionInCurrentSector > bpb->bytesPerSector)
                {
                    longNameCrossSectorBoundaryFlag = 1;
                    longNameLastSectorEntryFlag = 0;
                }

                // or Long name is entirely in current sector, but short name is in next sector.
                else if (shortNamePositionInCurrentSector == SECTOR_LEN)
                {
                    longNameCrossSectorBoundaryFlag = 0;
                    longNameLastSectorEntryFlag = 1;
                }
                else return CORRUPT_FAT_ENTRY;

                //get next sector's contents
                uint32_t nextSector;
                if (clusterSectorNumber >= bpb->sectorsPerCluster - 1) nextSector = bpb->dataRegionFirstSector + ( (pvt_GetNextCluster(cluster,bpb) - 2) * bpb->sectorsPerCluster);
                else nextSector = 1 + absoluteSectorNumber;

                // read next sector into nextSectorContents
                fat_ReadSingleSector( nextSector, nextSectorContents);

                // short name position in the next sector
                shortNamePositionInNextSector = shortNamePositionInCurrentSector - bpb->bytesPerSector;

                attributeByte = nextSectorContents[shortNamePositionInNextSector + 11];

                // confirm shortNamePositionInNextSector points to short name entry
                if ( (attributeByte & LONG_NAME_ATTR_MASK) == LONG_NAME_ATTR_MASK ) return CORRUPT_FAT_ENTRY;

                // Do nothing. Entry is a directory.
                else if (attributeByte & DIRECTORY_ENTRY_FLAG ); 

                // read in long name entry
                else 
                {                                                           
                  // Long name crosses sector boundary
                  if(longNameCrossSectorBoundaryFlag == 1 && longNameLastSectorEntryFlag == 0)
                  {
                    // confirm entry immediatedly preceding short name entry is not the first entry of a long name.
                    if( ( nextSectorContents[shortNamePositionInNextSector - ENTRY_LEN] & LONG_NAME_ORDINAL_MASK ) != 1 ) return CORRUPT_FAT_ENTRY;
                    
                    longNameStrIndex = 0;   
                    
                    // read long name into longNameStr
                    for(int i = (shortNamePositionInNextSector - ENTRY_LEN)  ; i >= 0 ; i = i - ENTRY_LEN) 
                    {
                      for(uint16_t n = i + 1; n < i + 11; n++)
                      {                                  
                        if(nextSectorContents[n] == 0 || nextSectorContents[n] > 126);
                        else { longNameStr[longNameStrIndex] = nextSectorContents[n]; longNameStrIndex++;  }
                      }

                      for(uint16_t n = i + 14; n < i + 26; n++)
                      {                                  
                        if(nextSectorContents[n] == 0 || nextSectorContents[n] > 126);
                        else { longNameStr[longNameStrIndex] = nextSectorContents[n]; longNameStrIndex++;  }
                      }
                      
                      for(uint16_t n = i + 28; n < i + 32; n++)
                      {                                  
                        if(nextSectorContents[n] == 0 || nextSectorContents[n] > 126);
                        else { longNameStr[longNameStrIndex] = nextSectorContents[n]; longNameStrIndex++;  }
                      }            
                    }

                    for(int i = (SECTOR_LEN - ENTRY_LEN) ; i >= entry ; i = i - ENTRY_LEN)
                    {                                
                      for(uint16_t n = i + 1; n < i + 11; n++)
                      {                                  
                        if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                        else { longNameStr[longNameStrIndex] = currentSectorContents[n]; longNameStrIndex++; }
                      }
                      
                      for(uint16_t n = i + 14; n < i + 26; n++)
                      {   
                        if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                        else { longNameStr[longNameStrIndex] = currentSectorContents[n]; longNameStrIndex++; }
                      }
                      
                      for(uint16_t n = i + 28; n < i + 32; n++)
                      {                                  
                        if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                        else { longNameStr[longNameStrIndex] = currentSectorContents[n]; longNameStrIndex++; }
                      }
                    }

                    // print file contents if a matching entry was found
                    if(!strcmp(fileNameStr,longNameStr))
                    { 
                      pvt_PrintFatFile(shortNamePositionInNextSector, nextSectorContents, bpb); 
                      return SUCCESS;
                    }
                  }

                  // Long name is the last entry of the current sector, so its short name is the first entry of the next sector
                  else if(longNameCrossSectorBoundaryFlag == 0 && longNameLastSectorEntryFlag == 1)
                  {
                    longNameStrIndex = 0;

                    // confirm last entry of current sector is the first entry of a long name
                    if( (currentSectorContents[SECTOR_LEN - ENTRY_LEN] & LONG_NAME_ORDINAL_MASK) != 1 ) return CORRUPT_FAT_ENTRY;                                                                  
                        
                    // read long name into longNameStr
                    for(int i = SECTOR_LEN - ENTRY_LEN ; i >= entry ; i = i - ENTRY_LEN) 
                    {                                
                      for(uint16_t n = i + 1; n < i + 11; n++)
                      {                                  
                        if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                        else { longNameStr[longNameStrIndex] = currentSectorContents[n]; longNameStrIndex++; }
                      }
                      
                      for(uint16_t n = i + 14; n < i + 26; n++)
                      {   
                        if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                        else { longNameStr[longNameStrIndex] = currentSectorContents[n]; longNameStrIndex++; }
                      }
                      
                      for(uint16_t n = i + 28; n < i + 32; n++)
                      {                                  
                        if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                        else { longNameStr[longNameStrIndex] = currentSectorContents[n]; longNameStrIndex++; }
                      }
                    }

                    // print file contents if a matching entry was found
                    if(!strcmp(fileNameStr,longNameStr))
                    { 
                      pvt_PrintFatFile(shortNamePositionInNextSector, nextSectorContents, bpb); 
                      return SUCCESS;
                    }                                            
                  }
                  else return CORRUPT_FAT_ENTRY;
                }
              }

              // Long name exists and long and short name entirely in the current directory.
              else 
              {   
                attributeByte = currentSectorContents[shortNamePositionInCurrentSector+11];
                
                // confirm shortNamePositionInCurrentSector points to a short name entry in the current sector
                if ( (attributeByte & LONG_NAME_ATTR_MASK) == LONG_NAME_ATTR_MASK ) return CORRUPT_FAT_ENTRY;
                
                // Entry is a directory. Do nothing.
                else if( attributeByte & DIRECTORY_ENTRY_FLAG); 
                
                else 
                {                   
                  // confirm entry immediatedly preceding the short name entry the first entry of a long name
                  if( ( currentSectorContents[shortNamePositionInCurrentSector - ENTRY_LEN] & LONG_NAME_ORDINAL_MASK ) != 1 ) return CORRUPT_FAT_ENTRY;
                  else
                  {
                    longNameStrIndex = 0;

                    // read long name into longNameStr
                    for(int i = shortNamePositionInCurrentSector - ENTRY_LEN ; i >= (int)entry ; i = i - ENTRY_LEN)
                    {                                
                      for(uint16_t n = i + 1; n < i + 11; n++)
                      {                                  
                        if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                        else { longNameStr[longNameStrIndex] = currentSectorContents[n]; longNameStrIndex++; }
                      }
                      
                      for(uint16_t n = i + 14; n < i + 26; n++)
                      {   
                        if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                        else { longNameStr[longNameStrIndex] = currentSectorContents[n]; longNameStrIndex++; }
                      }
                      
                      for(uint16_t n = i + 28; n < i + 32; n++)
                      {                                  
                        if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                        else { longNameStr[longNameStrIndex] = currentSectorContents[n]; longNameStrIndex++; }
                      }
                    }
                    
                    // print file contents if a matching entry was found
                    if(!strcmp(fileNameStr,longNameStr))
                    { 
                      pvt_PrintFatFile(shortNamePositionInCurrentSector, currentSectorContents, bpb); 
                      return SUCCESS;
                    }                                                                                    
                  }
                }
              }
            }
          }            

          // Long name does not exist for current entry so
          // check if fileNameStr matches the short name.
          else
          {
            // Entry is a directory. Do nothing.
            if ( attributeByte & DIRECTORY_ENTRY_FLAG );

            // fileNameStr is too long to be a short name.
            else if (fileNameStrLen > 12);
            
            // read in short name.
            else 
            {                   
              char sn[9];
              char ext[4];

              for(uint8_t k = 0; k < 9; k++) sn[k] = '\0';
              for(uint8_t k = 0; k < 4; k++) ext[k] = '\0'; 
      
              // search for location of '.', if it exists, in fileNameStr. Exclude first position.
              int pt = fileNameStrLen;
              uint8_t fileNameExtExistsFlag = 0;
              for(uint8_t k = 1; k < pt; k++)
              {
                if( k+1 >= fileNameStrLen ) { break; }
                if( fileNameStr[k] == '.' )  
                {   
                  fileNameExtExistsFlag = 1;
                  pt = k; 
                  break; 
                }
              }

              char tempFileName[9];
              for(uint8_t k = 0; k < pt; k++)  { tempFileName[k] = fileNameStr[k]; }
              for(uint8_t k = pt; k < 8; k++)  { tempFileName[k] = ' '; }
              tempFileName[8] = '\0';
              
              for(uint8_t k = 0; k < 8; k++)  { sn[k] = currentSectorContents[k + entry]; }

              // if name portion of short name matches then check that extensions match.
              if(!strcmp(tempFileName,sn))
              {                                
                uint8_t match = 0;
                int entryExtExistsFlag = 0;

                for(int k = 0; k < 3; k++)  
                {
                  ext[k] = currentSectorContents[entry + 8 + k]; 
                  entryExtExistsFlag = 1;
                }

                if( (strcmp(ext,"   ") ) && fileNameExtExistsFlag ) entryExtExistsFlag = 1;

                if ( (!entryExtExistsFlag) && (!fileNameExtExistsFlag) ) match = 1;
                else if ( (entryExtExistsFlag && !fileNameExtExistsFlag) || (!entryExtExistsFlag && fileNameExtExistsFlag) ) match = 0;
                else if (entryExtExistsFlag && fileNameExtExistsFlag)
                {
                  char tempEXT[4];
                  for(uint8_t k = 0; k < 3; k++) tempEXT[k] = ' ';
                  tempEXT[3] = '\0'; 

                  for(uint8_t k = 0; k < 3; k++)
                  {
                    if(fileNameStr[k+pt+1] == '\0') break;
                    tempEXT[k] = fileNameStr[k+pt+1];
                  }

                  // Extensions match!
                  if(!strcmp(ext,tempEXT) ) match = 1;
                }

                if(match)
                {
                  pvt_PrintFatFile(entry, currentSectorContents, bpb);
                  return SUCCESS;
                }
              }
            }
          }
        }
      }
    }
  } while( ( (cluster = pvt_GetNextCluster(cluster,bpb)) != END_OF_CLUSTER ) && (clusCnt < 5) );
  // END: Search for, and print, contents of fileNameStr if match is found

  return FILE_NOT_FOUND; 
}


// Prints an error code returned by a fat function.
void PrintFatError(uint16_t err)
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
      print_str("\n\rCORRUPT_SECTOR");
      break;
    case END_OF_FILE:
      print_str("\n\rEND_OF_FILE");
      break;
    default:
      print_str("\n\rUNKNOWN_ERROR");
      break;
  }
}


// **** Boot Sector/BIOS Parameter Block GET Functions ****


uint16_t FAT_GetBiosParameterBlock(BiosParameterBlock * bpb)
{
  uint8_t BootSector[SECTOR_LEN];
  bpb->bootSectorAddress = fat_FindBootSector();
  if(bpb->bootSectorAddress != 0xFFFFFFFF)
  {
    fat_ReadSingleSector(bpb->bootSectorAddress,BootSector);
  }
  else return BOOT_SECTOR_NOT_FOUND;

  // last two bytes of Boot Sector should be signature bytes.
  if((BootSector[SECTOR_LEN - 2] == 0x55) && (BootSector[SECTOR_LEN - 1] == 0xAA))
  {
    bpb->bytesPerSector = BootSector[12];
    bpb->bytesPerSector <<= 8;
    bpb->bytesPerSector |= BootSector[11];
    
    if(bpb->bytesPerSector != SECTOR_LEN) return INVALID_BYTES_PER_SECTOR;

    // secPerClus
    bpb->sectorsPerCluster = BootSector[13];

    if((bpb->sectorsPerCluster != 1 ) && (bpb->sectorsPerCluster != 2  ) &&
       (bpb->sectorsPerCluster != 4 ) && (bpb->sectorsPerCluster != 8  ) &&
       (bpb->sectorsPerCluster != 16) && (bpb->sectorsPerCluster != 32 ) &&
       (bpb->sectorsPerCluster != 64) && (bpb->sectorsPerCluster != 128)) 
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

    
    bpb->dataRegionFirstSector = bpb->bootSectorAddress + 
                                 bpb->reservedSectorCount + 
                                 (bpb->numberOfFats*bpb->fatSize32);
  }
  else return NOT_BOOT_SECTOR;
  return BOOT_SECTOR_VALID;
}



/******************************************************************************
 *                        "PRIVATE" FUNCTION DEFINITIONS
******************************************************************************/



/******************************************************************************
 * DESCRIPTION
 * Used by the fat functions to get the next cluster in a directory or file.
 * 
 * ARGUMENTS 
 * (1) *byte : pointer to the current directory sector loaded in memory.
 * (2) entry : entry is the first byte location of the short name in byte[].
 * (3) entryFilter  : indicates which fields of the short name entry to print.
 * 
 * RETURNS
 * FAT cluster index pointed to by the current cluster. 
 * If 0xFFFFFFFF then End Of File / Directory
******************************************************************************/
uint32_t pvt_GetNextCluster(uint32_t currentCluster, BiosParameterBlock * bpb)
{
  uint8_t  bytesPerClusterIndex = 4; // for FAT32
  uint16_t numberOfIndexedClustersPerSectorOfFat = bpb->bytesPerSector / bytesPerClusterIndex; // = 128

  uint32_t clusterIndex = currentCluster / numberOfIndexedClustersPerSectorOfFat;
  uint32_t clusterIndexStartByte = 4 * (currentCluster % numberOfIndexedClustersPerSectorOfFat);
  uint32_t cluster = 0;

  uint32_t FatSectorToRead = clusterIndex + bpb->reservedSectorCount;

  uint8_t SectorContents[bpb->bytesPerSector];

  fat_ReadSingleSector( FatSectorToRead, SectorContents );
  cluster = SectorContents[clusterIndexStartByte+3];
  cluster <<= 8;
  cluster |= SectorContents[clusterIndexStartByte+2];
  cluster <<= 8;
  cluster |= SectorContents[clusterIndexStartByte+1];
  cluster <<= 8;
  cluster |= SectorContents[clusterIndexStartByte];

  return cluster;
}


/******************************************************************************
 * DESCRIPTION
 * private function used by PrintFatCurrentDirectoryContents() to print the
 * non-name fields of a directory entry according to entryFilter.
 * 
 * ARGUMENTS 
 * (1) *byte : pointer to the current directory sector loaded in memory.
 * (2) entry : entry is the first byte location of the short name in byte[].
 * (3) entryFilter  : indicates which fields of the short name entry to print.  
******************************************************************************/
void pvt_PrintEntryFields(uint8_t *sector, uint16_t entry, uint8_t entryFilter)
{
  uint16_t creationTime;
  uint16_t creationDate;
  uint16_t lastAccessDate;
  uint16_t writeTime;
  uint16_t writeDate;
  uint32_t fileSize;

  if(CREATION & entryFilter)
  {
    creationTime = sector[entry + 15];
    creationTime <<= 8;
    creationTime |= sector[entry + 14];
    
    creationDate = sector[entry + 17];
    creationDate <<= 8;
    creationDate |= sector[entry + 16];
  }

  if(LAST_ACCESS & entryFilter)
  {
    lastAccessDate = sector[entry + 19];
    lastAccessDate <<= 8;
    lastAccessDate |= sector[entry + 18];
  }

  if(LAST_MODIFIED & entryFilter)
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

  print_str("\n\r");

  if(CREATION & entryFilter)
  {
    print_str("    ");
    if( ((creationDate & 0x01E0) >> 5) < 10) print_str("0");
    print_dec( (creationDate & 0x01E0) >> 5 );
    print_str("/");
    if( (creationDate & 0x001F) < 10) print_str("0");
    print_dec(creationDate & 0x001F);
    print_str("/");
    print_dec(1980 + ( (creationDate & 0xFE00) >> 9) );

    print_str("  ");
    if( ( (creationTime & 0xF800) >> 11) < 10) print_str("0");
    print_dec( ((creationTime & 0xF800) >> 11) );
    print_str(":");
    if( ( (creationTime & 0x07E0) >> 5) < 10) print_str("0");
    print_dec( (creationTime & 0x07E0) >> 5);
    print_str(":");
    if( (2 * (creationTime & 0x001F)) < 10) print_str("0");
    print_dec( 2 * (creationTime & 0x001F) );
  }

  if(LAST_ACCESS & entryFilter)
  {
    print_str("     ");
    if( ((lastAccessDate & 0x01E0) >> 5) < 10) print_str("0");
    print_dec( (lastAccessDate & 0x01E0) >> 5);
    print_str("/");
    if( (lastAccessDate & 0x001F) < 10) print_str("0");
    print_dec(lastAccessDate & 0x001F);
    print_str("/");
    print_dec(1980 + ((lastAccessDate & 0xFE00) >> 9) );
  }


  if(LAST_MODIFIED & entryFilter)
  {
    print_str("     ");
    if( ((writeDate & 0x01E0) >> 5) < 10) print_str("0");
    print_dec( (writeDate & 0x01E0) >> 5);
    print_str("/");
    if( (writeDate & 0x001F) < 10) print_str("0");
    print_dec(writeDate & 0x001F);
    print_str("/");
    print_dec(1980 + ( (writeDate & 0xFE00) >> 9) );

    print_str("  ");

    if( ((writeTime & 0xF800) >> 11) < 10) print_str("0");
    print_dec(((writeTime&0xF800)>>11));
    print_str(":");
    
    if( ((writeTime & 0x07E0) >> 5) < 10) print_str("0");
    print_dec( (writeTime & 0x07E0) >> 5);

    print_str(":");
    if( (2 * (writeTime & 0x001F)) < 10) print_str("0");
    print_dec(2 * (writeTime & 0x001F));
  }

  uint16_t div = 1000;
  print_str("     ");
       if( (fileSize / div) >= 10000000) { print_str(" "); print_dec(fileSize / div); }
  else if( (fileSize / div) >= 1000000) { print_str("  "); print_dec(fileSize / div); }
  else if( (fileSize / div) >= 100000) { print_str("   "); print_dec(fileSize / div); }
  else if( (fileSize / div) >= 10000) { print_str("    "); print_dec(fileSize / div); }
  else if( (fileSize / div) >= 1000) { print_str("     "); print_dec(fileSize / div); }
  else if( (fileSize / div) >= 100) { print_str("      "); print_dec(fileSize / div); }
  else if( (fileSize / div) >= 10) { print_str("       "); print_dec(fileSize / div); }
  else                             { print_str("        "); print_dec(fileSize / div);}        
  
  print_str("kB");

}



/******************************************************************************
 * DESCRIPTION
 * private function used by PrintFatCurrentDirectoryContents() to print the 
 * short name entry and its entry type (DIR / FILE).
 * 
 * ARGUMENTS 
 * (1) *byte : pointer to the current directory sector loaded in memory.
 * (2) entry : entry is the first byte location of the short name in byte[].
 * (3) attr  : attribute byte of the short name entry.  
*******************************************************************************/
void pvt_PrintShortNameAndType(uint8_t *sector, uint16_t entry, uint8_t attr)
{
  char sn[9];
  char ext[5];

  for(uint8_t k = 0; k < 8; k++) sn[k] = ' ';
  sn[8] = '\0';

  //print_str(" ENTRY = 0x"); print_hex(entry);
  if(attr & 0x10)
  {
    print_str("    <DIR>    ");
    for(uint8_t k = 0; k < 8; k++)  sn[k] = sector[entry + k];
    print_str(sn);
    print_str("    ");
  }

  else 
  {
    print_str("   <FILE>    ");

    // re-initialize extension character array;
    strcpy(ext,".   ");

    for(uint8_t k = 1; k < 4; k++) {  ext[k] = sector[entry + 7 + k];  }

    for(uint8_t k = 0; k < 8; k++) 
    {
      sn[k] = sector[k + entry];
      if(sn[k] == ' ') { sn[k] = '\0'; break; };
    }

    print_str(sn);
    if(strcmp(ext,".   "))  print_str(ext);
    for( uint8_t p = 0; p < 10 - (strlen(sn) + 2); p++ ) print_str(" ");
  }
}



/******************************************************************************
 * DESCRIPTION
 * private function used by PrintFatFileContents() to print a file's contents.
 * 
 * ARGUMENTS 
 * (1) entry : entry is the first byte location of the short name in byte[].
 * (2) *fileSector : pointer to an array loaded with the directory sector that
 *                   contains the file name entry for the file to be printed.
*******************************************************************************/
void pvt_PrintFatFile(uint16_t entry, uint8_t *fileSector, BiosParameterBlock * bpb)
{
  uint32_t absoluteSectorNumber;
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
    for(uint32_t clusterSectorNumber = 0; clusterSectorNumber < bpb->sectorsPerCluster; clusterSectorNumber++) 
    {
      absoluteSectorNumber = clusterSectorNumber + bpb->dataRegionFirstSector + ( (cluster - 2) * bpb->sectorsPerCluster );

      fat_ReadSingleSector( absoluteSectorNumber, fileSector);
      for(uint16_t k = 0; k < bpb->bytesPerSector; k++)  
      {
        if (fileSector[k] == '\n') print_str("\n\r");
        else if(fileSector[k] == 0);
        else USART_Transmit(fileSector[k]);
      }
    }
  } while( ( (cluster = pvt_GetNextCluster(cluster,bpb)) != END_OF_CLUSTER ) );
}