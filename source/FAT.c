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
uint32_t pvt_GetNextCluster(uint32_t CurrentCluster, BiosParameterBlock * bpb);
void pvt_PrintEntryFields(uint8_t *byte, uint16_t entry, uint8_t FLAG);
void pvt_PrintShortNameAndType(uint8_t *byte, uint16_t entry, uint8_t attr);
void pvt_PrintFatFile(uint16_t entry, uint8_t *byte, BiosParameterBlock * bpb);



/******************************************************************************
 *                         "PUBLIC" FUNCTION DEFINITIONS
******************************************************************************/

// Set currentDirectory to newDirectoryStr if found.
// Return a Fat Error Flag
uint16_t SetFatCurrentDirectory( FatCurrentDirectory * currentDirectory, char * newDirectoryStr, BiosParameterBlock * bpb)
{
  uint8_t newDirStrLen = strlen(newDirectoryStr);
    
    
  // ***** SECTION *****
  // Verify newDirectoryStr is a legal directory name

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
  // ***** END SECTION *****


  uint32_t absoluteSectorNumber;
  uint32_t cluster = currentDirectory->FATFirstCluster;

  uint8_t currentSectorContents[bpb->bytesPerSector]; 
  uint8_t nextSectorContents[bpb->bytesPerSector];

  uint16_t shortNamePositionInCurrentSector = 0;
  uint16_t shortNamePositionInNextSector    = 0;

  // for the DIR_Attr byte of an entry
  uint8_t  attributeByte; // for the DIR_Attr byte of an entry

  char    longNameStr[LONG_NAME_MAX_LEN];
  uint8_t longNameStrIndex = 0;

  // Long Name flags
  uint8_t longNameExistsFlag = 0; 
  uint8_t longNameCrossSectorBoundaryFlag = 0;
  uint8_t longNameLastSectorEntryFlag = 0;
  

  // ***** SECTION *****
  // Check if new directory is the current or parent directory.

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
    // ** END SECTION : if newDirectoryStr is current or parent directory **
    

  // ** SECTION : newDirectoryStr is expected to be a child directory **
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
                if ( (attributeByte & LONG_NAME_ORDINAL_MASK) == LONG_NAME_ORDINAL_MASK ) return CORRUPT_FAT_ENTRY;
                else
                {                                                           
                  // Long name crosses sector boundary?
                  if( (longNameCrossSectorBoundaryFlag == 1) && (longNameLastSectorEntryFlag == 0) )
                  {
                    // Confirm entry preceding short name is first entry of a long name. 
                    // Value in ordinal position must be 1, but mask out possible LONG_NAME_LAST_ENTRY_FLAG
                    if( (nextSectorContents[shortNamePositionInNextSector - ENTRY_LEN] & 0x0F) != 1) return CORRUPT_FAT_ENTRY;                                      
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

  return END_OF_DIRECTORY;
  // ** END FUNCTION and SECTION : if newDirectoryStr is expected to be a child directory **
}


// Prints long and/or short name entries found in the current directory as well
// as prints the entry's associated fields as specified by FLAG.
// Returns a Fat Error Flag
uint16_t PrintFatCurrentDirectoryContents(
                FatCurrentDirectory * currentDirectory,
                uint8_t FLAG, 
                BiosParameterBlock * bpb)
{
    print_str("\n\rCurrent Directory: "); 
    print_str(currentDirectory->longName);
    
    uint32_t absoluteSectorNumber;  // absolute (phyiscal) sector number
    uint32_t cluster = currentDirectory->FATFirstCluster;

    uint8_t currentSectorContents[bpb->bytesPerSector]; 
    uint8_t nextSectorContents[bpb->bytesPerSector];

    uint16_t shortNamePositionInCurrentSector = 0;   //position of short name in currentSectorContents
    uint16_t shortNamePositionInNextSector    = 0; //position of short name in nextSectorContents

    char    longNameStr[64];
    uint8_t longNameStrIndex = 0;

    uint8_t  attributeByte;

    // long name flags. Set to 1 if true for current name
    uint8_t longNameExistsFlag = 0; 
    uint8_t longNameCrossSectorBoundaryFlag = 0;
    uint8_t longNameLastSectorEntryFlag = 0;

    // Prints column headers according to flag setting
    print_str("\n\n\r");
    if(CREATION & FLAG)  print_str(" CREATION DATE & TIME,");
    if(LAST_ACCESS & FLAG)  print_str(" LAST ACCESS DATE,");
    if(LAST_MODIFIED & FLAG)  print_str(" LAST MODIFIED DATE & TIME,");
    print_str(" SIZE, TYPE, NAME");
    print_str("\n\n\r");
    

    // *** SECTION : Print entries in the current directory
    int clusCnt = 0;
    do 
    {
        clusCnt++;

        for(int clusterSectorNumber = 0; clusterSectorNumber < bpb->sectorsPerCluster; clusterSectorNumber++)
        {
            // read in next sector's contents into currentSectorContents[] array 
            absoluteSectorNumber = clusterSectorNumber + bpb->dataRegionFirstSector + ( (cluster - 2) * bpb->sectorsPerCluster );

            fat_ReadSingleSector( absoluteSectorNumber, currentSectorContents );

            for(int entry = 0; entry < bpb->bytesPerSector; entry = entry + 32)
            {
                // ensure 'entry' is pointing at correct location in current sector.
                if( longNameExistsFlag )
                {
                    if (shortNamePositionInCurrentSector >= 480 )
                    {
                        if ( entry != 0) break;
                        else shortNamePositionInCurrentSector = -32;
                    }

                    if( (longNameCrossSectorBoundaryFlag || longNameLastSectorEntryFlag) )
                    {
                        entry = shortNamePositionInNextSector    + 32;
                        shortNamePositionInNextSector    = 0;
                        longNameCrossSectorBoundaryFlag = 0;
                        longNameLastSectorEntryFlag = 0;
                    }

                    else 
                    {
                        entry = shortNamePositionInCurrentSector + 32;
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
                    if( (0x0F & attributeByte) == 0x0F )
                    {
                        //confirm long name last entry flag is set
                        if( !(currentSectorContents[entry] & 0x40) ) return CORRUPT_FAT_ENTRY;
                                                
                        longNameExistsFlag = 1;
                        
                        for(int k = 0; k < 64; k++) longNameStr[k] = '\0';

                        // number of entries required by the long name
                        uint8_t longNameOrder = 0x3F & currentSectorContents[entry];

                        shortNamePositionInCurrentSector = entry + (32 * longNameOrder);

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
                            uint32_t nextSec;
                            if (clusterSectorNumber >= bpb->sectorsPerCluster - 1) 
                            {
                                nextSec = bpb->dataRegionFirstSector + ( (pvt_GetNextCluster(cluster,bpb) - 2) * bpb->sectorsPerCluster );
                            }
                            else nextSec = 1 + absoluteSectorNumber;
                            fat_ReadSingleSector( nextSec, nextSectorContents);
    
                            // short name position in the next sector
                            shortNamePositionInNextSector    = shortNamePositionInCurrentSector - bpb->bytesPerSector;

                            attributeByte = nextSectorContents[shortNamePositionInNextSector   +11];
                            
                            // shortNamePositionInNextSector    points to long name entry?
                            if ( attributeByte == 0x0F ) return CORRUPT_FAT_ENTRY;

                            if ( (attributeByte & 0x02) == 0x02 && (FLAG & HIDDEN) != HIDDEN ); 
                            else
                            {                                                           
                                if( (FLAG & SHORT_NAME) != SHORT_NAME );
                                else
                                {
                                    pvt_PrintEntryFields(nextSectorContents, shortNamePositionInNextSector   , FLAG);
                                    pvt_PrintShortNameAndType(nextSectorContents, shortNamePositionInNextSector   , attributeByte);
                                }
                        
                                if( (FLAG & LONG_NAME) != LONG_NAME);
                                else
                                {
                                    // entries for long name cross sector boundary
                                    if(longNameCrossSectorBoundaryFlag == 1 && longNameLastSectorEntryFlag == 0)
                                    {
                                        // Confirm entry preceding short name is first entry of a long name.
                                        if( ( nextSectorContents[shortNamePositionInNextSector   -32] & 0x01 ) != 1 ) return CORRUPT_FAT_ENTRY;                                              

                                        pvt_PrintEntryFields(nextSectorContents, shortNamePositionInNextSector   , FLAG);
                
                                        longNameStrIndex = 0;   

                                        if ( attributeByte & 0x10 ) print_str("    <DIR>    ");
                                        else print_str("   <FILE>    ");
                                        
                                        // load long name entry into longNameStr[]
                                        for(int i = (shortNamePositionInNextSector    - 32)  ; i >= 0 ; i = i - 32) 
                                        {
                                            for(int n = i + 1; n < i + 11; n++)
                                            {                                  
                                                if(nextSectorContents[n] == 0 || nextSectorContents[n] > 126);
                                                else { longNameStr[longNameStrIndex] = nextSectorContents[n];  longNameStrIndex++;  }
                                            }

                                            for(int n = i + 14; n < i + 26; n++)
                                            {                                  
                                                if(nextSectorContents[n] == 0 || nextSectorContents[n] > 126);
                                                else { longNameStr[longNameStrIndex] = nextSectorContents[n];  longNameStrIndex++;  }
                                            }
                                            
                                            for(int n = i + 28; n < i + 32; n++)
                                            {                                  
                                                if(nextSectorContents[n] == 0 || nextSectorContents[n] > 126);
                                                else { longNameStr[longNameStrIndex] = nextSectorContents[n];  longNameStrIndex++;  }
                                            }            
                                        }

                                        for(int i = 480 ; i >= entry ; i = i - 32)
                                        {                                
                                            for(int n = i + 1; n < i + 11; n++)
                                            {                                  
                                                if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                                                else { longNameStr[longNameStrIndex] = currentSectorContents[n];  longNameStrIndex++; }
                                            }
                                            
                                            for(int n = i + 14; n < i + 26; n++)
                                            {   
                                                if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                                                else { longNameStr[longNameStrIndex] = currentSectorContents[n];  longNameStrIndex++; }
                                            }
                                            
                                            for(int n = i + 28; n < i + 32; n++)
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
                                        if( (currentSectorContents[480] & 0x01) != 1) return CORRUPT_FAT_ENTRY;
                             
                                        pvt_PrintEntryFields(nextSectorContents, shortNamePositionInNextSector   , FLAG);

                                        if(attributeByte&0x10) print_str("    <DIR>    ");
                                        else print_str("   <FILE>    ");
                                        
                                        // load long name entry into longNameStr[]
                                        for(int i = 480 ; i >= entry ; i = i - 32)
                                        {                                
                                            for(int n = i + 1; n < i + 11; n++)
                                            {                                  
                                                if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                                                else { longNameStr[longNameStrIndex] = currentSectorContents[n];  longNameStrIndex++; }
                                            }
                                            
                                            for(int n = i + 14; n < i + 26; n++)
                                            {   
                                                if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                                                else { longNameStr[longNameStrIndex] = currentSectorContents[n];  longNameStrIndex++; }
                                            }
                                            
                                            for(int n = i + 28; n < i + 32; n++)
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
                            attributeByte = currentSectorContents[shortNamePositionInCurrentSector+11];
                            
                            // shortNamePositionInCurrentSector points to long name entry
                            if (attributeByte == 0x0F) return CORRUPT_FAT_ENTRY;
                            if ( (attributeByte & 0x02) == 0x02 && (FLAG&HIDDEN) != HIDDEN );
                            else
                            {                   
                                if( (FLAG & SHORT_NAME) != SHORT_NAME );
                                else
                                {
                                    pvt_PrintEntryFields(currentSectorContents, shortNamePositionInCurrentSector, FLAG);
                                    pvt_PrintShortNameAndType(currentSectorContents, shortNamePositionInCurrentSector, attributeByte);
                                }

                                if( (FLAG & LONG_NAME) != LONG_NAME );
                                else
                                {
                                    // Confirm entry preceding short name is first entry of a long name.
                                    if( (currentSectorContents[shortNamePositionInCurrentSector-32] & 0x01) != 1) return CORRUPT_FAT_ENTRY;

                                    pvt_PrintEntryFields(currentSectorContents, shortNamePositionInCurrentSector, FLAG);
                                    
                                    if(attributeByte&0x10) print_str("    <DIR>    ");
                                    else print_str("   <FILE>    ");

                                    longNameStrIndex = 0;

                                    // load long name entry into longNameStr[]
                                    for(int i = shortNamePositionInCurrentSector - 32 ; i >= entry ; i = i - 32)
                                    {                                
                                        for(int n = i + 1; n < i + 11; n++)
                                        {                                  
                                            if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                                            else { longNameStr[longNameStrIndex] = currentSectorContents[n];  longNameStrIndex++; }
                                        }
                                        
                                        for(int n = i + 14; n < i + 26; n++)
                                        {   
                                            if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                                            else { longNameStr[longNameStrIndex] = currentSectorContents[n];  longNameStrIndex++; }
                                        }
                                        
                                        for(int n = i + 28; n < i + 32; n++)
                                        {                                  
                                            if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                                            else { longNameStr[longNameStrIndex] = currentSectorContents[n];  longNameStrIndex++; }
                                        }
                                    }
                                    print_str(longNameStr);                                       
                                }
                            }
                        }
                    }                   
                    else  // Long Name Entry does not exist so use short name instead, regardless of SHORT_NAME FLAG setting
                    {
                        attributeByte = currentSectorContents[entry+11];

                        if ( (attributeByte & 0x02) == 0x02 && (FLAG & HIDDEN) != HIDDEN );
                        else 
                        {
                            pvt_PrintEntryFields(currentSectorContents, entry, FLAG);
                            pvt_PrintShortNameAndType(currentSectorContents, entry, attributeByte);
                        }
                    }
                }
            }
        }
    }while( ( (cluster = pvt_GetNextCluster(cluster,bpb) ) != 0x0FFFFFFF ) && clusCnt < 5);
    // *** END SECTION :  Print entries in the current directory

    return END_OF_DIRECTORY;
}


// Prints the contents of file specified by *fileName to the screen.
// Returns a Fat Error Flag
uint16_t PrintFatFileContents(
                FatCurrentDirectory * currentDirectory, 
                char * fileName,
                BiosParameterBlock * bpb)
{
    uint8_t fnlen = strlen(fileName);

    // *** SECTION: Verify newDirectoryStr is a legal directory name ***
    if ((strcmp(fileName,"") == 0) ) return INVALID_FILE_NAME;
    if ( fileName[0] == ' ') return INVALID_FILE_NAME;
    
    for (uint8_t k = 0; k < fnlen; k++)
    {
        if( ( fileName[k] == 92 /* '\' */ ) ||
            ( fileName[k] == '/' ) ||
            ( fileName[k] == ':' ) ||
            ( fileName[k] == '*' ) ||
            ( fileName[k] == '?' ) ||
            ( fileName[k] == '"' ) ||
            ( fileName[k] == '<' ) ||
            ( fileName[k] == '>' ) ||
            ( fileName[k] == '|' )   )
        {
            return INVALID_FILE_NAME;
        }
    }

    uint8_t allSpaces = 1;
    for (uint8_t k = 0; k < fnlen; k++) 
    { 
        if(fileName[k] != ' ') {  allSpaces = 0;  break; }
    }
    if ( allSpaces == 1 ) return INVALID_FILE_NAME;
    // *** END SECTION: legal name verification ***



    uint32_t absoluteSectorNumber;  // absolute (phyiscal) sector number
    uint32_t cluster = currentDirectory->FATFirstCluster;
    
    uint8_t currentSectorContents[bpb->bytesPerSector]; 
    uint8_t nextSectorContents[bpb->bytesPerSector];

    uint16_t shortNamePositionInCurrentSector = 0;   //position of short name in currentSectorContents
    uint16_t shortNamePositionInNextSector    = 0; //position of short name in nextSectorContents

    char    longNameStr[64];
    uint8_t longNameStrIndex = 0;

    uint8_t  attributeByte;

    // long name flags. Set to 1 if true for current name
    uint8_t longNameExistsFlag = 0; 
    uint8_t longNameCrossSectorBoundaryFlag = 0;
    uint8_t longNameLastSectorEntryFlag = 0;


    // *** SECTION : Search for, and print contents of fileName if match is found ***
    int clusCnt = 0;    
    do
    {
        clusCnt++;

        for(int clusterSectorNumber = 0; clusterSectorNumber < bpb->sectorsPerCluster; clusterSectorNumber++)
        {     
            absoluteSectorNumber = clusterSectorNumber + bpb->dataRegionFirstSector + ( (cluster - 2) * bpb->sectorsPerCluster );
            fat_ReadSingleSector( absoluteSectorNumber, currentSectorContents );

            for(int entry = 0; entry < bpb->bytesPerSector; entry = entry + 32)
            { 
                // ensure 'entry' is pointing to correct location in current sector.
                if( longNameExistsFlag )
                {
                    if (shortNamePositionInCurrentSector >= 480 )
                    {
                        if ( entry != 0) break;
                        else shortNamePositionInCurrentSector = -32;
                    }

                    if( ( longNameCrossSectorBoundaryFlag || longNameLastSectorEntryFlag) ) 
                    {
                        entry = shortNamePositionInNextSector    + 32; 
                        shortNamePositionInNextSector    = 0; 
                        longNameCrossSectorBoundaryFlag = 0; 
                        longNameLastSectorEntryFlag = 0;
                    }

                    else 
                    {
                        entry = shortNamePositionInCurrentSector + 32; 
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
                    if( (0x0F & attributeByte) == 0x0F)
                    {
                        // confirm last long name entry flag is set
                        if( !(currentSectorContents[entry] & 0x40) ) return CORRUPT_FAT_ENTRY;
                        else
                        {
                            longNameExistsFlag = 1;

                            for(int k = 0; k < 64; k++) longNameStr[k] = '\0';
                            
                            // number of entries required by the long name
                            uint8_t longNameOrder = 0x3F & currentSectorContents[entry]; 
                                                            
                            shortNamePositionInCurrentSector = entry + (32 * longNameOrder);
                            
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
                                else if (shortNamePositionInCurrentSector == 512)
                                {
                                    longNameCrossSectorBoundaryFlag = 0;
                                    longNameLastSectorEntryFlag = 1;
                                }
                                else return CORRUPT_FAT_ENTRY;

                                //get next sector's contents
                                uint32_t nextSec;
                                if (clusterSectorNumber >= bpb->sectorsPerCluster - 1) nextSec = bpb->dataRegionFirstSector + ( (pvt_GetNextCluster(cluster,bpb) - 2) * bpb->sectorsPerCluster);
                                else nextSec = 1 + absoluteSectorNumber;

                                // read next sector into nextSectorContents
                                fat_ReadSingleSector( nextSec, nextSectorContents);

                                // short name position in the next sector
                                shortNamePositionInNextSector    = shortNamePositionInCurrentSector - bpb->bytesPerSector;

                                attributeByte = nextSectorContents[shortNamePositionInNextSector   +11];

                                // confirm shortNamePositionInNextSector    points to short name entry
                                if ( attributeByte == 0x0F ) return CORRUPT_FAT_ENTRY;

                                // Do nothing. Entry is a directory.
                                else if (attributeByte & 0x10 ); 

                                // read in long name entry
                                else 
                                {                                                           
                                    // Long name crosses sector boundary
                                    if(longNameCrossSectorBoundaryFlag == 1 && longNameLastSectorEntryFlag == 0)
                                    {
                                        // confirm entry immediatedly preceding short name entry is not the first entry of a long name.
                                        if( ( nextSectorContents[shortNamePositionInNextSector   -32] & 0x01 ) != 1 ) return CORRUPT_FAT_ENTRY;
                                        
                                        longNameStrIndex = 0;   
                                        
                                        // read long name into longNameStr
                                        for(int i = (shortNamePositionInNextSector    - 32)  ; i >= 0 ; i = i - 32) 
                                        {
                                            for(int n = i + 1; n < i + 11; n++)
                                            {                                  
                                                if(nextSectorContents[n] == 0 || nextSectorContents[n] > 126);
                                                else { longNameStr[longNameStrIndex] = nextSectorContents[n];  longNameStrIndex++;  }
                                            }

                                            for(int n = i + 14; n < i + 26; n++)
                                            {                                  
                                                if(nextSectorContents[n] == 0 || nextSectorContents[n] > 126);
                                                else { longNameStr[longNameStrIndex] = nextSectorContents[n];  longNameStrIndex++;  }
                                            }
                                            
                                            for(int n = i + 28; n < i + 32; n++)
                                            {                                  
                                                if(nextSectorContents[n] == 0 || nextSectorContents[n] > 126);
                                                else { longNameStr[longNameStrIndex] = nextSectorContents[n];  longNameStrIndex++;  }
                                            }            
                                        }

                                        for(int i = 480 ; i >= entry ; i = i - 32)
                                        {                                
                                            for(int n = i + 1; n < i + 11; n++)
                                            {                                  
                                                if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                                                else { longNameStr[longNameStrIndex] = currentSectorContents[n];  longNameStrIndex++; }
                                            }
                                            
                                            for(int n = i + 14; n < i + 26; n++)
                                            {   
                                                if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                                                else { longNameStr[longNameStrIndex] = currentSectorContents[n];  longNameStrIndex++; }
                                            }
                                            
                                            for(int n = i + 28; n < i + 32; n++)
                                            {                                  
                                                if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                                                else { longNameStr[longNameStrIndex] = currentSectorContents[n];  longNameStrIndex++; }
                                            }
                                        }

                                        // print file contents if a matching entry was found
                                        if(!strcmp(fileName,longNameStr))
                                        { 
                                            pvt_PrintFatFile(shortNamePositionInNextSector   , nextSectorContents, bpb); 
                                            return SUCCESS;
                                        }
                                    }

                                    // Long name is the last entry of the current sector, so its short name is the first entry of the next sector
                                    else if(longNameCrossSectorBoundaryFlag == 0 && longNameLastSectorEntryFlag == 1)
                                    {
                                        longNameStrIndex = 0;

                                        // confirm last entry of current sector is the first entry of a long name
                                        if( (currentSectorContents[480] & 0x01) != 1 ) return CORRUPT_FAT_ENTRY;                                                                  
                                            
                                        // read long name into longNameStr
                                        for(int i = 480 ; i >= entry ; i = i - 32) 
                                        {                                
                                            for(int n = i + 1; n < i + 11; n++)
                                            {                                  
                                                if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                                                else { longNameStr[longNameStrIndex] = currentSectorContents[n];  longNameStrIndex++; }
                                            }
                                            
                                            for(int n = i + 14; n < i + 26; n++)
                                            {   
                                                if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                                                else { longNameStr[longNameStrIndex] = currentSectorContents[n];  longNameStrIndex++; }
                                            }
                                            
                                            for(int n = i + 28; n < i + 32; n++)
                                            {                                  
                                                if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                                                else { longNameStr[longNameStrIndex] = currentSectorContents[n];  longNameStrIndex++; }
                                            }
                                        }

                                        // print file contents if a matching entry was found
                                        if(!strcmp(fileName,longNameStr))
                                        { 
                                            pvt_PrintFatFile(shortNamePositionInNextSector   , nextSectorContents, bpb); 
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
                                if ( attributeByte == 0x0F ) return CORRUPT_FAT_ENTRY;
                                
                                // Entry is a directory. Do nothing.
                                else if(attributeByte & 0x10); 
                                
                                else 
                                {                   
                                    // confirm entry immediatedly preceding the short name entry the first entry of a long name
                                    if( ( currentSectorContents[shortNamePositionInCurrentSector-32] & 0x01 ) != 1 ) return CORRUPT_FAT_ENTRY;
                                    else
                                    {
                                        longNameStrIndex = 0;

                                        // read long name into longNameStr
                                        for(int i = shortNamePositionInCurrentSector - 32 ; i >= entry ; i = i - 32)
                                        {                                
                                            for(int n = i + 1; n < i + 11; n++)
                                            {                                  
                                                if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                                                else { longNameStr[longNameStrIndex] = currentSectorContents[n];  longNameStrIndex++; }
                                            }
                                            
                                            for(int n = i + 14; n < i + 26; n++)
                                            {   
                                                if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                                                else { longNameStr[longNameStrIndex] = currentSectorContents[n];  longNameStrIndex++; }
                                            }
                                            
                                            for(int n = i + 28; n < i + 32; n++)
                                            {                                  
                                                if(currentSectorContents[n] == 0 || currentSectorContents[n] > 126);
                                                else { longNameStr[longNameStrIndex] = currentSectorContents[n];  longNameStrIndex++; }
                                            }
                                        }
                                        
                                        // print file contents if a matching entry was found
                                        if(!strcmp(fileName,longNameStr))
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
                    // check if fileName matches the short name.
                    else
                    {
                        // Entry is a directory. Do nothing.
                        if (attributeByte & 0x10 );

                        // fileName is too long to be a short name.
                        else if (fnlen > 12);
                        
                        // read in short name.
                        else 
                        {                   
                            char SN[9];
                            char EXT[4];

                            for(uint8_t k = 0; k < 9; k++) SN[k] = '\0';
                            for(uint8_t k = 0; k < 4; k++) EXT[k] = '\0'; 
                    
                            // search for location of '.', if it exists, in fileName. Exclude first position.
                            int pt = fnlen;
                            uint8_t fnExtExistsFlag = 0;
                            for(uint8_t k = 1; k < pt; k++)
                            {
                                if( k+1 >= fnlen ) { break; }
                                if( fileName[k] == '.' )  
                                {   
                                    fnExtExistsFlag = 1;
                                    pt = k; 
                                    break; 
                                }
                            }

                            char tempFN[9];
                            for(uint8_t k = 0; k < pt; k++)  { tempFN[k] = fileName[k]; }
                            for(uint8_t k = pt; k < 8; k++)  { tempFN[k] = ' '; }
                            tempFN[8] = '\0';
                            
                            for(uint8_t k = 0; k < 8; k++)  { SN[k] = currentSectorContents[k+entry]; }

                            // if name portion of short name matches then check that extensions match.
                            if(!strcmp(tempFN,SN))
                            {                                
                                uint8_t match = 0;
                                int entryEXTExistsFlag = 0;

                                for(int k = 0; k < 3; k++)  
                                {
                                    EXT[k] = currentSectorContents[entry+8+k]; 
                                    entryEXTExistsFlag = 1;
                                }

                                if( (strcmp(EXT,"   ") ) && fnExtExistsFlag ) entryEXTExistsFlag = 1;

                                if ( (!entryEXTExistsFlag) && (!fnExtExistsFlag) ) match = 1;
                                else if ( (entryEXTExistsFlag && !fnExtExistsFlag) || (!entryEXTExistsFlag && fnExtExistsFlag) ) match = 0;
                                else if (entryEXTExistsFlag && fnExtExistsFlag)
                                {
                                    char tempEXT[4];
                                    for(uint8_t k = 0; k < 3; k++) tempEXT[k] = ' ';
                                    tempEXT[3] = '\0'; 

                                    for(uint8_t k = 0; k < 3; k++)
                                    {
                                        if(fileName[k+pt+1] == '\0') break;
                                        tempEXT[k] = fileName[k+pt+1];
                                    }

                                    // Extensions match!
                                    if(!strcmp(EXT,tempEXT) ) match = 1;
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
    } while( ( (cluster = pvt_GetNextCluster(cluster,bpb)) != 0x0FFFFFFF ) && (clusCnt < 5) );
    // *** END SECTION : Search for, and print contents of fileName if match is found ***
 
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
    uint8_t BootSector[512];
    bpb->bootSectorAddress = fat_FindBootSector();
    if(bpb->bootSectorAddress != 0xFFFFFFFF)
    {
        fat_ReadSingleSector(bpb->bootSectorAddress,BootSector);
    }
    else return BOOT_SECTOR_NOT_FOUND;

    if((BootSector[510] == 0x55) && (BootSector[511]==0xAA))
    {
        bpb->bytesPerSector = BootSector[12];
        bpb->bytesPerSector <<= 8;
        bpb->bytesPerSector |= BootSector[11];
        
        if(bpb->bytesPerSector != 512) return INVALID_BYTES_PER_SECTOR;

        // secPerClus
        bpb->sectorsPerCluster = BootSector[13];

        if((bpb->sectorsPerCluster != 1 ) && (bpb->sectorsPerCluster != 2  ) &&
           (bpb->sectorsPerCluster != 4 ) && (bpb->sectorsPerCluster != 8  ) &&
           (bpb->sectorsPerCluster != 16) && (bpb->sectorsPerCluster != 32 ) &&
           (bpb->sectorsPerCluster != 64) && (bpb->sectorsPerCluster != 128)) 
        {
            return INVALID_SECTORS_PER_CLUSTER;
        }

        bpb->reservedSectorCount =  BootSector[15];
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
 * (3) FLAG  : indicates which fields of the short name entry to print.
 * 
 * RETURNS
 * FAT cluster index pointed to by the current cluster. 
 * If 0xFFFFFFFF then End Of File / Directory
******************************************************************************/
uint32_t pvt_GetNextCluster(uint32_t CurrentCluster, BiosParameterBlock * bpb)
{
    uint8_t  BytesPerClusterIndx = 4;
    uint16_t IndxdClustersPerFATSector = bpb->bytesPerSector/BytesPerClusterIndx; // = 128

    uint32_t FATClusterIndxSector = CurrentCluster/IndxdClustersPerFATSector;
    uint32_t FATClusterIndxStartByte = 4 * (CurrentCluster%IndxdClustersPerFATSector);
    uint32_t cluster = 0;

    uint32_t AbsSectorToRead = FATClusterIndxSector + bpb->reservedSectorCount; //FATStartSector;

    uint8_t SectorContents[bpb->bytesPerSector];

    fat_ReadSingleSector( AbsSectorToRead, SectorContents );
    cluster = SectorContents[FATClusterIndxStartByte+3];
    cluster <<= 8;
    cluster |= SectorContents[FATClusterIndxStartByte+2];
    cluster <<= 8;
    cluster |= SectorContents[FATClusterIndxStartByte+1];
    cluster <<= 8;
    cluster |= SectorContents[FATClusterIndxStartByte];

    return cluster;
}


/******************************************************************************
 * DESCRIPTION
 * private function used by PrintFatCurrentDirectoryContents() to print the
 * non-name fields of a directory entry according to FLAG.
 * 
 * ARGUMENTS 
 * (1) *byte : pointer to the current directory sector loaded in memory.
 * (2) entry : entry is the first byte location of the short name in byte[].
 * (3) FLAG  : indicates which fields of the short name entry to print.  
******************************************************************************/
void pvt_PrintEntryFields(uint8_t *byte, uint16_t entry, uint8_t FLAG)
{
    uint16_t DIR_CrtTime;
    uint16_t DIR_CrtDate;
    uint16_t DIR_LstAccDate;
    uint16_t DIR_WrtTime;
    uint16_t DIR_WrtDate;
    uint32_t DIR_FileSize;

    if(CREATION&FLAG)
    {
        DIR_CrtTime = byte[entry+15];
        DIR_CrtTime <<= 8;
        DIR_CrtTime |= byte[entry+14];
        
        DIR_CrtDate = byte[entry+17];
        DIR_CrtDate <<= 8;
        DIR_CrtDate |= byte[entry+16];
    }

    if(LAST_ACCESS&FLAG)
    {
        DIR_LstAccDate = byte[entry+19];
        DIR_LstAccDate <<= 8;
        DIR_LstAccDate |= byte[entry+18];
    }

    if(LAST_MODIFIED&FLAG)
    {
        DIR_WrtTime = byte[entry+23];
        DIR_WrtTime <<= 8;
        DIR_WrtTime |= byte[entry+22];

        DIR_WrtDate = byte[entry+25];
        DIR_WrtDate <<= 8;
        DIR_WrtDate |= byte[entry+24];
    }

    DIR_FileSize = byte[entry+31];
    DIR_FileSize <<= 8;
    DIR_FileSize |= byte[entry+30];
    DIR_FileSize <<= 8;
    DIR_FileSize |= byte[entry+29];
    DIR_FileSize <<= 8;
    DIR_FileSize |= byte[entry+28];

    print_str("\n\r");

    if(CREATION&FLAG)
    {
        print_str("    ");
        if(((DIR_CrtDate&0x01E0)>>5)<10) print_str("0");
        print_dec((DIR_CrtDate&0x01E0)>>5);
        print_str("/");
        if((DIR_CrtDate&0x001F)<10) print_str("0");
        print_dec(DIR_CrtDate&0x001F);
        print_str("/");
        print_dec(1980+((DIR_CrtDate&0xFE00)>>9));

        print_str("  ");
        if(((DIR_CrtTime&0xF800)>>11)<10) print_str("0");
        print_dec(((DIR_CrtTime&0xF800)>>11));
        print_str(":");
        if(((DIR_CrtTime&0x07E0)>>5)<10) print_str("0");
        print_dec((DIR_CrtTime&0x07E0)>>5);
        print_str(":");
        if((2*(DIR_CrtTime&0x001F))<10) print_str("0");
        print_dec(2*(DIR_CrtTime&0x001F));
    }

    if(LAST_ACCESS&FLAG)
    {
        print_str("     ");
        if(((DIR_LstAccDate&0x01E0)>>5)<10) print_str("0");
        print_dec((DIR_LstAccDate&0x01E0)>>5);
        print_str("/");
        if((DIR_LstAccDate&0x001F)<10) print_str("0");
        print_dec(DIR_LstAccDate&0x001F);
        print_str("/");
        print_dec(1980+((DIR_LstAccDate&0xFE00)>>9));
    }


    if(LAST_MODIFIED&FLAG)
    {
        print_str("     ");
        if(((DIR_WrtDate&0x01E0)>>5)<10) print_str("0");
        print_dec((DIR_WrtDate&0x01E0)>>5);
        print_str("/");
        if((DIR_WrtDate&0x001F)<10) print_str("0");
        print_dec(DIR_WrtDate&0x001F);
        print_str("/");
        print_dec(1980+((DIR_WrtDate&0xFE00)>>9));

        print_str("  ");

        if(((DIR_WrtTime&0xF800)>>11)<10) print_str("0");
        print_dec(((DIR_WrtTime&0xF800)>>11));
        print_str(":");
        
        if(((DIR_WrtTime&0x07E0)>>5)<10) print_str("0");
        print_dec((DIR_WrtTime&0x07E0)>>5);

        print_str(":");
        if((2*(DIR_WrtTime&0x001F))<10) print_str("0");
        print_dec(2*(DIR_WrtTime&0x001F));
    }

    int div = 1000;
    print_str("     ");
         if( (DIR_FileSize/div) >= 10000000) { print_str(" "); print_dec(DIR_FileSize/div); }
    else if( (DIR_FileSize/div) >= 1000000) { print_str("  "); print_dec(DIR_FileSize/div); }
    else if( (DIR_FileSize/div) >= 100000) { print_str("   "); print_dec(DIR_FileSize/div); }
    else if( (DIR_FileSize/div) >= 10000) { print_str("    "); print_dec(DIR_FileSize/div); }
    else if( (DIR_FileSize/div) >= 1000) { print_str("     "); print_dec(DIR_FileSize/div); }
    else if( (DIR_FileSize/div) >= 100) { print_str("      "); print_dec(DIR_FileSize/div); }
    else if( (DIR_FileSize/div) >= 10) { print_str("       "); print_dec(DIR_FileSize/div); }
    else                              { print_str("        "); print_dec(DIR_FileSize/div); }        
    
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
void pvt_PrintShortNameAndType(uint8_t *byte, uint16_t entry, uint8_t attr)
{
    char SN[9]; // array to hold the short name file name string
    char EXT[5]; // array to hold the extension of the short name string

    for(int k = 0; k < 8; k++) SN[k] = ' ';
    SN[8] = '\0';
    //print_str(" ENTRY = 0x"); print_hex(entry);
    if(attr&0x10)
    {
        print_str("    <DIR>    ");
        for(int k = 0; k < 8; k++)  SN[k] = byte[entry + k];
        print_str(SN);
        print_str("    ");
    }

    else 
    {
        print_str("   <FILE>    ");

        // re-initialize extension character array;
        strcpy(EXT,".   ");

        for(int k = 1; k < 4; k++) {  EXT[k] = byte[entry + 7 + k];  }

        for(int k = 0; k < 8; k++) 
        {
            SN[k] = byte[k + entry];
            if(SN[k] == ' ') { SN[k] = '\0'; break; };
        }

        print_str(SN);
        if(strcmp(EXT,".   "))  print_str(EXT);
        for( int p = 0; p < 10 - (strlen(SN) + 2); p++ ) print_str(" ");
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
    uint32_t absoluteSectorNumber;  // absolute (phyiscal) sector number
    uint32_t cluster;

    //get FAT index for file's first cluster
    cluster =  fileSector[entry+21];
    cluster <<= 8;
    cluster |= fileSector[entry+20];
    cluster <<= 8;
    cluster |= fileSector[entry+27];
    cluster <<= 8;
    cluster |= fileSector[entry+26];
    
    // read in contents of file starting at relative sector 0 in 'cluster' and print contents to the screen.
    do
    {
        print_str("\n\n\r");   
        for(int clusterSectorNumber = 0; clusterSectorNumber < bpb->sectorsPerCluster; clusterSectorNumber++) 
        {
            absoluteSectorNumber = clusterSectorNumber + bpb->dataRegionFirstSector + ( (cluster - 2) * bpb->sectorsPerCluster );

            fat_ReadSingleSector( absoluteSectorNumber, fileSector);
            for(int k = 0; k < bpb->bytesPerSector; k++)  
            {
                if (fileSector[k] == '\n') print_str("\n\r");
                else if(fileSector[k] == 0);
                else USART_Transmit(fileSector[k]);
            }
        }
    } while( ( (cluster = pvt_GetNextCluster(cluster,bpb)) != 0x0FFFFFFF ) );
}