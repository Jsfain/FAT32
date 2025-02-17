/*
 * File       : FATtoSD.C
 * Version    : 2.0
 * License    : GNU GPLv3
 * Author     : Joshua Fain
 * Copyright (c) 2020 - 2025
 * 
 * Implementation of FATtoDISK.H to access an FAT formatted SD card.
 */

#include <stdint.h>
#include "prints.h"
#include "sd_spi_base.h"
#include "sd_spi_rwe.h"
#include "sd_spi_print.h"
#include "fat_to_disk_if.h"

/*
 ******************************************************************************
 *           "PRIVATE" FUNCTIONS, MACROS, and GLOBAL VARIABLES
 ******************************************************************************
 */

//
// global static varible for the CARD TYPE VERSION struct defined in 
// SD_SPI_BASE. This is set from FindBootSector function below, during SD card
// initialization but it's also needed in the read disk sector function.
//
static CTV ctv;

// calls the SD card initialization routine.
static void pvt_SDCardInit(CTV *cardTypeVers);

// The number of times the module can attempt to initialize the SD card.
#define SD_CARD_INIT_ATTEMPTS_MAX      5    // set to any value <= 255.


/*
 ******************************************************************************
 *                                 FUNCTIONS
 ******************************************************************************
 */

/*
 * ----------------------------------------------------------------------------
 *                                                             FIND BOOT SECTOR
 *                                 
 * Description : Finds the address of the boot sector on the FAT32-formatted 
 *               SD card. This function is required by fat_SetBPB in fat_bpb.c.
 *               This version searches for the block containing the Jump Boot
 *               and Boot Signature bits and returns this block number.
 * 
 * Arguments   : void
 * 
 * Returns     : Address of the boot sector on the SD card.
 * 
 * Notes       : The search for the boot sector will begin at
 *               FBS_SEARCH_START_BLOCK, and search a total of 
 *               FBS_MAX_NUM_BLKS_SEARCH_MAX blocks. 
 * ----------------------------------------------------------------------------
 */
uint32_t FATtoDisk_FindBootSector(void)
{
  // Initialize the FAT formatted SD card 
  pvt_SDCardInit(&ctv);

  //
  // For SD cards, addressing is determined by the card type. If the card type 
  // is SDHC then it is block addressable. If it is SDSC then it is byte
  // addressable. The assumed default is SDHC, and the block is addressed by
  // the block number. If SDSC then the block number is multiplied by the block
  // length to deterime the byte address of the first by in the block.
  // 
  uint16_t addrMult = 1;                    // init for SDHC.
  if (ctv.type == SDSC)
    addrMult = BLOCK_LEN;
  
  // Send the READ MULTIPLE BLOCK command and confirm R1 Response is good.
  CS_ASSERT;
  sd_SendCommand(READ_MULTIPLE_BLOCK, FBS_SEARCH_START_BLOCK * addrMult); 
  if (sd_GetR1() != OUT_OF_IDLE)
  {
    CS_DEASSERT;
    return FAILED_FIND_BOOT_SECTOR;
  }

  // loop to search for the boot sector and set blkNum to its address.
  for (uint32_t blkNum = FBS_SEARCH_START_BLOCK * addrMult;
       blkNum < FBS_SEARCH_START_BLOCK + FBS_MAX_NUM_BLKS_SEARCH_MAX;
       ++blkNum)
  {   
    uint8_t  blckArr[BLOCK_LEN];            // to hold the block data bytes

    //
    // loop until the 'Start Block Token' has been received from the SD card,
    // which indicates data from requested block is about to be sent.
    //
    for (uint16_t attempt = 0; sd_ReceiveByteSPI() != START_BLOCK_TKN;)
      if (++attempt >= MAX_CR_ATT)
      {
        CS_DEASSERT;
        print_Str("\n\rFailed to receive START_BLOCK_TOKEN from SD card.");
        return FAILED_FIND_BOOT_SECTOR;
      }

    // load all bytes of the sector into the block array
    for (uint16_t byteNum = 0; byteNum < BLOCK_LEN; ++byteNum) 
      blckArr[byteNum] = sd_ReceiveByteSPI();
    
    // 16-bit CRC. CRC is off (default) so these values do not matter.
    sd_ReceiveByteSPI(); 
    sd_ReceiveByteSPI(); 

    //
    // Check if JUMP BOOT bytes present. If yes, confirm the BOOT SIGNATURE
    // bytes also present. If yes, this is BPB so return this block number.      
    //
    if ((blckArr[0] == JMP_BOOT_1A && blckArr[2] == JMP_BOOT_3A) ||
         blckArr[0] == JMP_BOOT_1B)
          if (blckArr[BLOCK_LEN - 2] == BS_SIGN_1 &&
             blckArr[BLOCK_LEN - 1] == BS_SIGN_2)
          {
            // Boot Sector has been found!
            sd_SendCommand(STOP_TRANSMISSION, 0); // stop sending data blocks.
            sd_ReceiveByteSPI();                  // R1B resp. Don't care.
            CS_DEASSERT;
            return blkNum;                        // return success!
          }
  }

  // FAILED to find BS in the set block range.
  sd_SendCommand(STOP_TRANSMISSION, 0);     // stop sending data blocks.
  sd_ReceiveByteSPI();                      // R1B resp. Don't care.
  CS_DEASSERT;
  return FAILED_FIND_BOOT_SECTOR;           // return failed token
}

/* 
 * ----------------------------------------------------------------------------
 *                                                 READ SINGLE SECTOR FROM DISK
 *                                       
 * Description : Loads the contents of the sector/block at the specified 
 *               address on the SD card into the array, blckArr.
 *
 * Arguments   : blkNum    - Block number address of the sector/block on the SD
 *                           card that should be read into blkArr.
 * 
 *               blkArr    - Pointer to the array that will be loaded with the 
 *                           contents of the sector/block on the SD card at 
 *                           block specified by blkNum.
 * 
 * Returns     : READ_SECTOR_SUCCES if successful.
 *               READ_SECTOR_FAILED if failure.
 * ----------------------------------------------------------------------------
 */
uint8_t FATtoDisk_ReadSingleSector(uint32_t blkNum, uint8_t blkArr[])
{
  //
  // For SD cards, addressing is determined by the card type. If the card type 
  // is SDHC then it is block addressable. If it is SDSC then it is byte
  // addressable. The assumed default is SDHC, and the block is addressed by
  // the block number. If SDSC then the block number is multiplied by the block
  // length to deterime the byte address of the first by in the block.
  // 
  uint16_t addrMult = 1;                    // init for SDHC.
  if (ctv.type == SDSC)
    addrMult = BLOCK_LEN;

  // Load data block into array by passing the array to the Read Block function
  if (sd_ReadSingleBlock(blkNum * addrMult, blkArr) == READ_SUCCESS)
    return READ_SECTOR_SUCCESS; 
  return FAILED_READ_SECTOR;
};

/*
 ******************************************************************************
 *                            "PRIVATE" FUNCTION        
 ******************************************************************************
 */

/* 
 * ----------------------------------------------------------------------------
 *                                                             GET SD CARD TYPE
 *                                       
 * Description : Determines and returns the SD card type. The card type is used
 *               determine if the SD card is block or byte addressable.
 * 
 * Arguments   : void
 * 
 * Returns     : SD card type - SDSC or SDHC, or GET_CARD_TYPE_ERROR.
 * ----------------------------------------------------------------------------
 */
static void pvt_SDCardInit(CTV *cardTypeVers)
{
//
  // SD card initialization hosting the FAT volume.
  //
  uint32_t sdInitResp;          


  // Loop will continue until SD card init succeeds or max attempts reached.
  for (uint8_t att = 0; att < SD_CARD_INIT_ATTEMPTS_MAX; ++att)
  {
    print_Str("\n\n\r >> Initializing SD Card: Attempt "); 
    print_Dec(att + 1);
    sdInitResp = sd_InitSpiMode(cardTypeVers);      // init SD Card

    if (sdInitResp != OUT_OF_IDLE)          // Fail to init if not OUT_OF_IDLE
    {    
      print_Str("\n\r >> FAILED to initialize SD Card."
                "\n\r >> Error Response returned: "); 
      sd_PrintInitErrorResponse(sdInitResp);
      print_Str(" R1 Response: "); 
      sd_PrintR1(sdInitResp);
    }
    else
    {   
      print_Str("\n\r >> SD Card Initialization Successful");
      break;
    }
  }
}


//
// Want to preserve these for now, but don't need these macros or function 
// since adding call to SD INIT to this file which inherently determines the 
// card type.
//
/*
// macros used by pvt_GetCardType
#define GET_CARD_TYPE_ERROR 0xFF
#define CSD_STRUCT_MSK      0xC0
#define CSD_VSN_1           0x00
#define CSD_VSN_2           0x40
#define CSD_BYTE_LEN        16

static uint8_t pvt_GetCardType(void)
{
  uint8_t cardType;

  CS_ASSERT;
  sd_SendCommand(SEND_CSD, 0);
  if (sd_GetR1() != OUT_OF_IDLE) 
  { 
    CS_DEASSERT; 
    return GET_CARD_TYPE_ERROR;             // R1 error. Failed to get the CSD
  }

  //
  // Get card type by reading the CSD register's CSD Structure field. This 
  // field provides the version of CSD (0,1,2,3) and corresponds to the to determine the version of the card - either SDHC or SDSC
  //
  for (uint16_t attempt = 0; ; ++attempt)
  {
    //
    // if max attempts reached without reading a valid CSD structure field
    // then read in enough bytes to get the rest of the CSD register. These 
    // fields are not used but this is just to clear them out of the response.
    //
    if (attempt >= MAX_CR_ATT)           // if max attempt exceeded
    { 
      // Read in rest of CSD bytes, though not used.
      for(int byteNum = 0; byteNum < CSD_BYTE_LEN - 1; ++byteNum) 
        sd_ReceiveByteSPI();
      CS_DEASSERT;
      return GET_CARD_TYPE_ERROR;           // max attempt limit fail                             
    }

    uint8_t resp = sd_ReceiveByteSPI();
    if ((resp & CSD_STRUCT_MSK) == CSD_VSN_1)
    { 
      cardType = SDSC; 
      break;
    }
    else if ((resp & CSD_STRUCT_MSK) == CSD_VSN_2) 
    { 
      cardType = SDHC; 
      break; 
    } 
  }

  // Read in rest of CSD, though not used.
  for(int byteNum = 0; byteNum < CSD_BYTE_LEN - 1; ++byteNum) 
    sd_ReceiveByteSPI();
  CS_DEASSERT;
  return cardType;                          // success
}
*/

