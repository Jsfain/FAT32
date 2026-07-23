/*
 * File       : FAT.C
 * Version    : 2.0
 * License    : GNU GPLv3
 * Author     : Joshua Fain
 * Copyright (c) 2020 - 2025
 * 
 * Implementation of FAT.H
 */

#include <stdint.h>
#include <string.h>
//#include "fat_bpb.h"
#include "fat.h"
#include "fat_disk_if.h"

/*
 ******************************************************************************
 *                 "PRIVATE" FUNCTION PROTOTYPES
 ******************************************************************************
 */

static void pvt_UpdateFatEntryMembers(FatEntry *ent, const char lnStr[], 
                const uint8_t secArr[], uint16_t snPos,
                uint8_t snEntSecNumInClus, uint32_t snEntClusIndx);
static uint8_t pvt_CheckName(const char nameStr[]);
static uint8_t pvt_SetDirToParent(FatDir *dir, const BPB *bpb);
static void pvt_LoadLongName(int lnFirstEnt, int lnLastEnt, 
                             const uint8_t secArr[], char lnStr[]);
static uint32_t pvt_GetNextClusIndex(uint32_t clusIndex, const BPB *bpb);

/*
 ******************************************************************************
 *                                FUNCTIONS
 ******************************************************************************
 */


/*
 * ----------------------------------------------------------------------------
 *                                                       SET BPB STRUCT MEMBERS 
 *                                         
 * Description : Gets values of the Bios Parameter Block / Boot Sector fields 
 *               from a FAT volume and sets the corresponding members of the
 *               BPB struct's instance accordingly.
 * 
 * Arguments   : bpb   - Pointer to an BPB struct's instance. This function
 *                       will set the members of this instance.
 * 
 * Returns     : Boot Sector Error Flag. If any value other than BPB_VALID is
 *               returned then setting the BPB instance failed. To print, pass
 *               the returned value to fat_PrintErrorBPB().
 * 
 * Notes       : A valid BPB struct instance is a required argument of many 
 *               functions that access the FAT volume, therefore this function 
 *               should be called first before calling any other parts of the 
 *               FAT module.
 * 
 * Limitation  : Currently will only work if Boot Sector is block 0 on SD Card.
 * ----------------------------------------------------------------------------
 */
uint8_t fat_SetBPB(BPB *bpb)
{
  uint8_t bootSecArr[SECTOR_LEN], err; 

  // locate boot sector address on the disk. 
  uint32_t bootSecAddr = fatDisk_FindBootSector();
  if (bootSecAddr != FAILED_FIND_BOOT_SECTOR)
  {
    // load data from boot sector into bootSecArr.
    err = fatDisk_ReadSector(bootSecAddr, bootSecArr);
    if (err == FAILED_READ_SECTOR) 
      return FAILED_READ_BPB;
  }
  else
    return BPB_NOT_FOUND;
  
  // 
  // Confirm the sector loaded is the Boot Sector by checking the signature
  // bytes - the last two bytes of sector. If true, then begin loading the 
  // necessary BPB field values into their respective BPB struct members.
  // 
  if (bootSecArr[SECTOR_LEN - 2] == BS_SIGN_1 
      && bootSecArr[SECTOR_LEN - 1] == BS_SIGN_2)
  {
    bpb->bytesPerSec = bootSecArr[BYTES_PER_SEC_POS_MSB];      
    bpb->bytesPerSec <<= 8;                 
    bpb->bytesPerSec |= bootSecArr[BYTES_PER_SEC_POS_LSB];
    
    // Bytes Per Sector must be the same as SECTOR_LEN
    if (bpb->bytesPerSec != SECTOR_LEN)
      return INVALID_BYTES_PER_SECTOR;

    bpb->secPerClus = bootSecArr[SEC_PER_CLUS_POS];

    // check that secPerClus is a valid value.   
    if (!CHK_VLD_SEC_PER_CLUS(bpb->secPerClus))
      return INVALID_SECTORS_PER_CLUSTER;
    
    // number of reserved sectors
    bpb->rsvdSecCnt = bootSecArr[RSVD_SEC_CNT_POS_MSB];
    bpb->rsvdSecCnt <<= 8;
    bpb->rsvdSecCnt |= bootSecArr[RSVD_SEC_CNT_POS_LSB];

    // number of FATs
    bpb->numOfFats = bootSecArr[NUM_FATS_POS];

    // Size of a single FAT
    bpb->fatSize32 =  bootSecArr[FAT32_SIZE_POS4];
    bpb->fatSize32 <<= 8;
    bpb->fatSize32 |= bootSecArr[FAT32_SIZE_POS3];
    bpb->fatSize32 <<= 8;
    bpb->fatSize32 |= bootSecArr[FAT32_SIZE_POS2];
    bpb->fatSize32 <<= 8;
    bpb->fatSize32 |= bootSecArr[FAT32_SIZE_POS1];

    // Root directory cluster index
    bpb->rootClus =  bootSecArr[ROOT_CLUS_POS4];
    bpb->rootClus <<= 8;
    bpb->rootClus |= bootSecArr[ROOT_CLUS_POS3];
    bpb->rootClus <<= 8;
    bpb->rootClus |= bootSecArr[ROOT_CLUS_POS2];
    bpb->rootClus <<= 8;
    bpb->rootClus |= bootSecArr[ROOT_CLUS_POS1];

    //
    // The disk's sector address corresponding to the first sector of the FAT32
    // volume's Data Region. Since the first cluster of the Data Region is the 
    // Root Directory, this value points to the sector number of the Root Dir.
    //
    bpb->dataRegionFirstSector = bootSecAddr + bpb->rsvdSecCnt 
                               + bpb->numOfFats * bpb->fatSize32;
    return BPB_VALID;
  }
  else 
    return NOT_BPB;
}

/* 
 * ----------------------------------------------------------------------------
 *                                                        SET TO ROOT DIRECTORY
 *                                        
 * Description : Sets instance of FatDir to the root directory.
 *
 * Arguments   : dir   - Pointer to FatDir instance to be set to root dir.
 *               bpb   - Pointer to the BPB struct instance.
 *
 * Returns     : void
 * ----------------------------------------------------------------------------
 */
void fat_SetDirToRoot(FatDir *dir, const BPB *bpb)
{
  // set string members to indicate root cluster
  strcpy(dir->snStr, "/");
  strcpy(dir->snPathStr, "");
  strcpy(dir->lnStr, "/");
  strcpy(dir->lnPathStr, "");
  
  // set first cluster index to that of the root cluster
  dir->fstClusIndx = bpb->rootClus;
}

/*
 * ----------------------------------------------------------------------------
 *                                                         INITIALIZE FAT ENTRY
 *                                      
 * Description : Initializin an instance of a FatEntry struct will set it to 
 *               the first entry of the root directory.
 * 
 * Arguments   : ent   - Pointer to the FatEntry instance to be initialized.           
 *               bpb   - Pointer to the BPB struct instance.
 * 
 * Returns     : void
 * ----------------------------------------------------------------------------
 */
void fat_InitEntry(FatEntry *ent, const BPB *bpb)
{
  // set long and short names to empty strings
  strcpy(ent->lnStr, "");
  strcpy(ent->snStr, "");
  
  // fill short name entry array with 0's
  for(uint8_t entByte = 0; entByte < ENTRY_LEN; ++entByte)
    ent->snEnt[entByte] = 0;

  // set rest of the FatEntry members to 0. 
  ent->snEntSecNumInClus = 0;
  ent->nextEntPos = 0;

  // Set the cluster index to point to the root directory.
  ent->snEntClusIndx = bpb->rootClus;
}

/*
 * ----------------------------------------------------------------------------
 *                                                  SET FAT ENTRY TO NEXT ENTRY 
 *                                      
 * Description : Updates a FatEntry instance to point to the next entry in its
 *               directory.
 * 
 * Arguments   : currEnt   - Pointer to a FatEntry instance. Its members will 
 *                           be updated to point to the next entry. 
 *               bpb       - Pointer to the BPB struct instance.
 *
 * Returns     : A FAT Error Flag. If any value other than SUCCESS is returned 
 *               then the function was unable to update the FatEntry.
 * ----------------------------------------------------------------------------
 */
uint8_t fat_SetNextEntry(FatEntry *currEnt, const BPB *bpb)
{  
  //
  // this section sets the initial values of the different nested loop
  // counters for the first time they are entered during a single function 
  // call. These are set according to the state of the currEnt members.
  //

  // index of the cluster where the previous short name entry was found
  uint32_t clusIndx = currEnt->snEntClusIndx;
  // sector num in the cluster where the previous short name entry was found
  uint8_t  secNumInClus = currEnt->snEntSecNumInClus;
  // position of entry following previous short name entry in the sector
  uint16_t entPos = currEnt->nextEntPos;

  //
  // if previous short name entry occupied the last entry position of a sector
  // then increment secNumInClus and set entPos to 0 so that the search for the
  // next entry will begin on this function call at the first entry of the next
  // sector. For the case when the the next sector is beyond the cluster limit 
  // it will be handled in the nested loops.
  //
  if (entPos == SECTOR_LEN)
  {
    ++secNumInClus;
    entPos = 0;
  }

  // loop over clusters beginning at clusIndx  to search for next entry.
  do 
  {
    //
    // loop over sectors in the cluster to find the next entry. If this loop is
    // re-entered in a single function call then secNumInClus will be reset to
    // 0 in the outer cluser loop. The first time it is entered it should be
    // initialized to the value of the snEntSecNumInClus member of the currEnt
    // instance of FatEntry.
    //
    for (; secNumInClus < bpb->secPerClus; ++secNumInClus)
    {
      // calculate location of sector on the disk
      uint32_t secNumOnDisk = secNumInClus + bpb->dataRegionFirstSector
                            + (clusIndx - bpb->rootClus) 
                            * bpb->secPerClus;
      
      // create and load array with data bytes from the disk sector
      uint8_t secArr[bpb->bytesPerSec];  
      if (fatDisk_ReadSector(secNumOnDisk, secArr) 
          == FAILED_READ_SECTOR)
        return FAILED_READ_SECTOR;

      //
      // loop over entries in the sector to search for the next entry. If this 
      // loop is re-entered in a single function call then entPos will be reset 
      // to 0 in the sector loop. The first time it is entered entPos should be
      // initialized to the value of the nextEntPos member of the currEnt
      // instance of FatEntry.
      //
      for (; entPos < bpb->bytesPerSec; entPos += ENTRY_LEN)
      {
        // if first byte of an entry is 0, remaining entries should be empty
        if (!secArr[entPos])                                                       
          return END_OF_DIRECTORY;

        if (secArr[entPos] == DELETED_ENTRY_TOKEN)
          continue;

        // check attribute byte to see if entPos points to a long name entry
        if ((secArr[entPos + ATTR_BYTE_OFFSET] & LN_ATTR_MASK) == LN_ATTR_MASK)
        {
          // entPos must be pointing to the last entry of a long name here.
          if (!(secArr[entPos] & LN_LAST_ENTRY_FLAG))
            return CORRUPT_FAT_ENTRY;
          
          // initialize empty long name string 
          char lnStr[LN_STR_LEN_MAX] = {'\0'};   

          // calculate position of short name relative to first byte in sector
          uint16_t snPos = entPos + ENTRY_LEN * (LN_ORD_MASK & secArr[entPos]);

          // enter if short name is in the next sector
          if (snPos >= bpb->bytesPerSec)
          {              
            uint8_t nextSecArr[bpb->bytesPerSec]; 

            //
            // locate next sector. Depending on the number of the sector in the 
            // cluster, the next sector will either be in the next cluster or 
            // it will be the next sector in the cluster and on the disk.
            //
            if (secNumInClus == bpb->secPerClus - 1)  // next sec in next clus
            {
              // calculate location of next sector in next clus on the disk
              secNumOnDisk = bpb->dataRegionFirstSector 
                           + (pvt_GetNextClusIndex(clusIndx, bpb)
                           - bpb->rootClus)
                           * bpb->secPerClus;
              secNumInClus = 0;
            }
            else                  // next sector is the next physical sector 
            {
              ++secNumOnDisk;
              ++secNumInClus;
            }

            // load next sector into nextSecArr[].
            if (fatDisk_ReadSector(secNumOnDisk, nextSecArr) 
                == FAILED_READ_SECTOR)
              return FAILED_READ_SECTOR;
            
            // snPos to point to sn entry relative to first byte of next sector
            snPos -= bpb->bytesPerSec;
            
            // verify snPos does not point to long name
            if ((nextSecArr[snPos + ATTR_BYTE_OFFSET] & LN_ATTR_MASK) 
                 == LN_ATTR_MASK)
              return CORRUPT_FAT_ENTRY;
            
            //
            // check if a ln spans the sector boundary. At this point, sn is in
            // next sector, but if sn is not first entry (i.e. snPos != 0) then 
            // entries for ln are in the current sector and next sector.
            //
            if (snPos)
            {
              // Entry preceeding short name must be first entry of long name      
              if ((nextSecArr[snPos - ENTRY_LEN] & LN_ORD_MASK) != 1)
                return CORRUPT_FAT_ENTRY;

              // Call twice for both current and next sector.
              pvt_LoadLongName(snPos - ENTRY_LEN, FIRST_ENT_POS_IN_SEC,
                               nextSecArr, lnStr);
              pvt_LoadLongName(LAST_ENTRY_POS_IN_SEC, entPos, secArr, lnStr);
            }
            else   // full ln in current sec, but sn is first ent in next sec
            {
              // Entry preceeding short name must be first entry of long name
              if ((secArr[LAST_ENTRY_POS_IN_SEC] & LN_ORD_MASK) != 1)
                return CORRUPT_FAT_ENTRY;

              pvt_LoadLongName(LAST_ENTRY_POS_IN_SEC, entPos, secArr, lnStr);
            }
            pvt_UpdateFatEntryMembers(currEnt, lnStr, nextSecArr, snPos,
                                      secNumInClus, clusIndx);
            return SUCCESS;
          }
          else          // Long and short name are in the current sector.
          {   
            // Verify snPos does not point to long name
            if ((secArr[snPos + ATTR_BYTE_OFFSET] & LN_ATTR_MASK) 
                 == LN_ATTR_MASK)
              return CORRUPT_FAT_ENTRY;
    
            // entry preceeding short name must be first entry of long name
            if ((secArr[snPos - ENTRY_LEN] & LN_ORD_MASK) != 1)
              return CORRUPT_FAT_ENTRY;
            
            pvt_LoadLongName(snPos - ENTRY_LEN, entPos, secArr, lnStr);
            pvt_UpdateFatEntryMembers(currEnt, lnStr, secArr, snPos, 
                                      secNumInClus, clusIndx);
            return SUCCESS;                          
          }                   
        }
        else            // Long name does not exist. Use short name instead.
        {
          // passing empty string for long name
          pvt_UpdateFatEntryMembers(currEnt, "", secArr, entPos,
                                    secNumInClus, clusIndx);
          return SUCCESS;  
        }
      }
      entPos = FIRST_ENT_POS_IN_SEC;      // reset counter for entry loop
    }
    secNumInClus = FIRST_SEC_POS_IN_CLUS;// reset counter for sector loop
  }
  // get index of next cluster and continue looping if not last cluster
  while ((clusIndx = pvt_GetNextClusIndex(clusIndx, bpb)) != END_CLUSTER);

  // return here if the end of the dir was reached without finding a next entry
  return END_OF_DIRECTORY;
}

/*
 * ----------------------------------------------------------------------------
 *                                                            SET FAT DIRECTORY
 *                                       
 * Description : Set FatDir instance to the directory specified by newDirStr.
 * 
 * Arguments   : dir         - Pointer to the FatDir instance to be set to the
 *                             new directory.             
 *               newDirStr   - Pointer to a string that specifies the name of 
 *                             the new directory.
 *               bpb         - Pointer to the BPB struct instance.
 * 
 * Returns     : A FAT Error Flag. If any value other than SUCCESS is returned 
 *               then the function was unable to update the FatEntry. 
 *  
 * Notes       : 1) This function can only set the directory to a child or the
 *                  parent of the FatDir instance (dir) when the function is
 *                  called, or reset the instance to the root directory.
 *               2) Paths (relative or absolute) should not be included in the 
 *                  newDirStr. newDirStr must only be a directory name which
 *                  must be the name of a child, or the parent directory of
 *                  the current directory.
 *               3) If ".." is passed as the newDirStr then the new directory
 *                  will be set to the parent of the current directory.               
 *               4) newDirStr is case-sensitive.
 *               5) newDirStr must be a long name, unless a long name does not
 *                  exist for a directory, only then can it be a short name.
 * ----------------------------------------------------------------------------
 */
uint8_t fat_SetDir(FatDir *dir, const char newDirStr[], const BPB *bpb)
{
  // for return errors. This is the loop cond. and the return value.
  uint8_t err;                              

  // Initial checks before running the searching the FAT 
  if (pvt_CheckName(newDirStr) == INVALID_NAME)  // if newDirStr is illegal
    return INVALID_NAME;
  else if (!strcmp(newDirStr, "."))         // if newDirStr is current dir
    return SUCCESS;
  else if (!strcmp(newDirStr, ".."))        // if newDirStr is parent dir
    return pvt_SetDirToParent(dir, bpb);    // FAILED_READ_SECTOR or SUCCESS
  else if (!strcmp(newDirStr, "~"))         // if newDirStr is "return to root"
  {
    fat_SetDirToRoot(dir, bpb);
    return SUCCESS;
  }

  // 
  // Create and initialize a FatEntry. Sets the snEntClusIndx member to root
  // and all other members to 0 or null strings. Updates the snEntClusIndx 
  // to point to first cluster index of the FatDir instance (dir).
  //
  FatEntry ent;
  fat_InitEntry(&ent, bpb);
  ent.snEntClusIndx = dir->fstClusIndx;

  // 
  // Search FatDir directory instance for child directory matching newDirStr.
  // Repeatedly calls fat_SetNextEntry() to set the FatEntry instance to next
  // entry in the directory. Compares lnStr member of FatEntry to newDirStr. 
  // Note, short names can only be used when a lnStr does not exist for the 
  // entry. If this is the case then lnStr will be the same as snStr.
  //
  while ((err = fat_SetNextEntry(&ent, bpb)) == SUCCESS)
  {
    // if entry is not a directory entry, get next entry
    if (!(ent.snEnt[ATTR_BYTE_OFFSET] & DIR_ENTRY_ATTR))
      continue;

    // if entry matches newDirStr 
    if (!strcmp(ent.lnStr, newDirStr))
    {
      // get value of the first cluster index in the FAT for that entry.
      dir->fstClusIndx = ent.snEnt[FST_CLUS_INDX_BYTE_OFFSET_3];
      dir->fstClusIndx <<= 8;
      dir->fstClusIndx |= ent.snEnt[FST_CLUS_INDX_BYTE_OFFSET_2];
      dir->fstClusIndx <<= 8;
      dir->fstClusIndx |= ent.snEnt[FST_CLUS_INDX_BYTE_OFFSET_1];
      dir->fstClusIndx <<= 8;
      dir->fstClusIndx |= ent.snEnt[FST_CLUS_INDX_BYTE_OFFSET_0];
      
      // fill short name array with its characters from the entry
      char snStr[SN_NAME_CHAR_LEN + 1] = {'\0'};      
      for (uint8_t strPos = 0; strPos < SN_NAME_CHAR_LEN; ++strPos)
        snStr[strPos] = ent.snEnt[strPos];

      // Append current directory name to the short and long name paths
      strcat (dir->lnPathStr, dir->lnStr);
      strcat (dir->snPathStr, dir->snStr);

      // Update dir to new dir name. If current dir != root dir append '/'
      if (strcmp(dir->lnStr, "/"))
        strcat(dir->lnPathStr, "/"); 
      strcpy(dir->lnStr, newDirStr);
      
      if (strcmp(dir->snStr, "/"))
        strcat(dir->snPathStr, "/");
      strcpy(dir->snStr, snStr);

      return SUCCESS;
    }
  }
  // No matching entry found. FatDir is unchanged.
  return err;
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
 * Description : Sets the FatEntry instance struct members to the values of the 
 *               arguments passed in.
 * 
 * Arguments   : ent                 - ptr to FatEntry instance whose members 
 *                                     will be updated.
 *               lnStr               - ptr to array holding long name string.
 *               secArr              - ptr to array holding the data of the 
 *                                     sector that contains the short name
 *                                     entry that will update ent.
 *               snPos               - position in secArr of the first byte of
 *                                     the short name entry.
 *               snEntSecNumInClus   - Sector number where the short name is
 *                                     located relative to the first sector of   
 *                                     the cluster.
 *               snEntClusIndx       - Fat cluster index where short name entry
 *                                     is located.
 * 
 * Returns     : void
 * ----------------------------------------------------------------------------
 */
static void pvt_UpdateFatEntryMembers(FatEntry *ent, const char lnStr[], 
                const uint8_t secArr[], uint16_t snPos,
                uint8_t snEntSecNumInClus, uint32_t snEntClusIndx)
{
  // copy short name entry bytes into *snEnt FatEntry member
  for (uint8_t byteNum = 0; byteNum < ENTRY_LEN; ++byteNum)
    ent->snEnt[byteNum] = secArr[snPos + byteNum];
  
  //
  // The section parses the short name name + ext chars in the short name 
  // entry of the sector and then loads them into the snStr FatEntry member as
  // a string.
  //

  // vars to assist loading short name
  char sn[SN_CHAR_LEN + 1] = {'\0'};        // for sn string. Add 1 for null
  char *snPtr = sn;

  // load short name characters into array. skip spaces.
  for (uint8_t byteNum = 0; byteNum < SN_NAME_CHAR_LEN; ++byteNum)
    if (ent->snEnt[byteNum] != ' ')
      *snPtr++ = ent->snEnt[byteNum];

  // if there is an extension add it to sn string here
  if (ent->snEnt[SN_NAME_CHAR_LEN] != ' ')
  {
    *snPtr++ = '.';
    // load extension chars into array. Skip spaces stop at end of ext chars.
    for (uint8_t byteNum = SN_NAME_CHAR_LEN; 
         byteNum < SN_CHAR_LEN - 1; ++byteNum)
      if (ent->snEnt[byteNum] != ' ')
        *snPtr++ = ent->snEnt[byteNum];
  }
  strcpy(ent->snStr, sn);                   // load snStr FatEntry member.

  // 
  // load lnStr FatEntry member. If the lnStr function parameter is a non-empty
  // string, then the lnStr FatEntry member will be loaded with lnStr param. If
  // it is empty, then it will be loaded with the short name string.
  //  
  if (strcmp(lnStr,""))
    strcpy(ent->lnStr, lnStr);
  else
    strcpy(ent->lnStr, ent->snStr);

  // copy remaining parameters into FatEntry members.
  ent->snEntSecNumInClus = snEntSecNumInClus;
  ent->snEntClusIndx = snEntClusIndx;
  ent->nextEntPos = snPos + ENTRY_LEN;
}

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
 *                                (PRIVATE) SET CURRENT DIRECTORY TO ITS PARENT
 *  
 *  Description : Sets a FatDir instance to its parent directory. 
 * 
 *  Arguments   : dir   - Pointer to a FatDir struct instance. The members of
 *                        this instance will be set to its parent directory.
 *                bpb   - Pointer to the BPB struct instance.
 * 
 *  Returns     : SUCCESS or FAILED_READ_SECTOR
 * ----------------------------------------------------------------------------
 */
static uint8_t pvt_SetDirToParent(FatDir *dir, const BPB *bpb)
{
  uint32_t parentDirFirstClus, secNumOnDisk;
  uint8_t  secArr[bpb->bytesPerSec];

  // sector number/address on disk
  secNumOnDisk = bpb->dataRegionFirstSector 
               + (dir->fstClusIndx - bpb->rootClus) 
               * bpb->secPerClus;
                
  // load secArr with disk sector at secNumOnDisk
  if (fatDisk_ReadSector(secNumOnDisk, secArr) == FAILED_READ_SECTOR)
   return FAILED_READ_SECTOR;

  // load first cluster index of the parent directory.
  parentDirFirstClus = secArr[FST_CLUS_INDX_BYTE_OFFSET_3 + ENTRY_LEN];
  parentDirFirstClus <<= 8;
  parentDirFirstClus |= secArr[FST_CLUS_INDX_BYTE_OFFSET_2 + ENTRY_LEN];
  parentDirFirstClus <<= 8;
  parentDirFirstClus |= secArr[FST_CLUS_INDX_BYTE_OFFSET_1 + ENTRY_LEN];
  parentDirFirstClus <<= 8;
  parentDirFirstClus |= secArr[FST_CLUS_INDX_BYTE_OFFSET_0 + ENTRY_LEN];

  if (dir->fstClusIndx == bpb->rootClus);   // current dir is root dir.
  else if (parentDirFirstClus == 0)         // parent dir is root dir
    fat_SetDirToRoot(dir, bpb);
  else                                      // parent dir is a sub-dir
  { 
    // 
    // The parent dir is the dir in the path string between the last two '/'
    // chars. This section takes the parent directory substring from the path
    // string and stores it in the name directory strings. It then replaces the
    // '/' just before the parent substring in the path with a null effectively
    // removing the parent directory from the path strings
    // 

    // this will replace the ending '/' char in paths with a null
    strlcpy(dir->snPathStr, dir->snPathStr, strlen(dir->snPathStr));
    strlcpy(dir->lnPathStr, dir->lnPathStr, strlen(dir->lnPathStr));
    
    // ptrs to locations in path strings holding '/' prior to parent dir
    char *snLastDirInPathPtr = strrchr(dir->snPathStr, '/');
    char *lnLastDirInPathPtr = strrchr(dir->lnPathStr, '/');
    
    // copy last directory in path string to the short/long name strings
    strcpy(dir->snStr, ++snLastDirInPathPtr);
    strcpy(dir->lnStr, ++lnLastDirInPathPtr);

    // removes previous parent directory from the path
    *snLastDirInPathPtr = '\0';
    *lnLastDirInPathPtr = '\0';

    dir->fstClusIndx = parentDirFirstClus;
  }
  return SUCCESS;
}

/*
 * ----------------------------------------------------------------------------
 *                               (PRIVATE) LOAD A LONG NAME ENTRY INTO A STRING 
 * 
 * Description : Loads characters of a long name into a C-string array.  
 * 
 * Arguments   : lnFirstEnt   - Position of the lowest order entry of the long 
 *                              name in secArr.
 *               lnLastEnt    - Position of the highest order entry of the long 
 *                              name in secArr.
 *               secArr       - Pointer to array holding contents of a single
 *                              sector of a dir from a FAT-formatted disk.
 *               lnStr        - Pointer to a string array that will be loaded
 *                              with the characters from a long name entry.
 * 
 * Returns     : void 
 * 
 * Notes       : Must be called twice if long name crosses sector boundary.
 * ----------------------------------------------------------------------------
 */
static void pvt_LoadLongName(int lnFirstEnt, int lnLastEnt,
                             const uint8_t secArr[], char lnStr[])
{
  //
  // set lnStr to point to first null char in array. This will be position 0 
  // except when function is called twice to load a long name that crosses 
  // sector boundaries.
  //
  for (; *lnStr; ++lnStr)
    ;
  
  // loop over the entries in the sector containing the long name
  for (int entPos = lnFirstEnt; entPos >= lnLastEnt; entPos -= ENTRY_LEN)
  {                                              
    //
    // loops to load long name chars from a single entry. Skips any nulls and 
    // any characters outside of the standard ascii range.
    // 
    for (uint16_t byteNum = entPos + LN_CHAR_RANGE_1_BEGIN; 
         byteNum < entPos + LN_CHAR_RANGE_1_END; byteNum++)
      if (secArr[byteNum] && secArr[byteNum] <= LAST_STD_ASCII_CHAR)
        *lnStr++ = secArr[byteNum];

    for (uint16_t byteNum = entPos + LN_CHAR_RANGE_2_BEGIN; 
         byteNum < entPos + LN_CHAR_RANGE_2_END; byteNum++)
      if (secArr[byteNum] && secArr[byteNum] <= LAST_STD_ASCII_CHAR)
        *lnStr++ = secArr[byteNum];
    
    for (uint16_t byteNum = entPos + LN_CHAR_RANGE_3_BEGIN;
         byteNum < entPos + LN_CHAR_RANGE_3_END; byteNum++)
      if (secArr[byteNum] && secArr[byteNum] <= LAST_STD_ASCII_CHAR)
        *lnStr++ = secArr[byteNum];    
  }
}

/*
 * ----------------------------------------------------------------------------
 *                                      (PRIVATE) GET FAT INDEX OF NEXT CLUSTER
 * 
 * Description : Finds and returns the index of the next FAT cluster.
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
