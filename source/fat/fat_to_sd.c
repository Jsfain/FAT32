/*
 * File       : FATtoSD.C
 * Version    : 2.0
 * Target     : ATMega1280
 * Compiler   : AVR-GCC 9.3.0
 * Downloader : AVRDUDE 6.3
 * License    : GNU GPLv3
 * Author     : Joshua Fain
 * Copyright (c) 2020, 2021
 * 
 * Implementation of FATtoDISK.H.
 */

#include <avr/io.h>
#include "spi.h"
#include "prints.h"
#include "sd_spi_base.h"
#include "sd_spi_rwe.h"
#include "fat_bpb.h"
#include "fat.h"
#include "fat_to_disk_if.h"

/*
 ******************************************************************************
 *                  "PRIVATE" FUNCTION PROTOTYPES and MACROS
 ******************************************************************************
 */
static uint8_t pvt_GetCardType(void);

// macros used in by pvt_GetCardType
#define GET_CARD_TYPE_ERROR 0xFF
#define CSD_STRUCT_MSK      0xC0
#define CSD_VSN_1           0x00
#define CSD_VSN_2           0x40
#define CSD_BYTE_LEN        16

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
 *               SD card. This function is used by fat_SetBPB from fat_bpb.c(h)
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
  //
  // Determine card type. If SDHC then the SD card is block addressable and
  // the block number will be the address of the block. If SDSC then card is
  // byte addressable, in which case the address of the block is the address
  // of the first byte in the block, thus the address would be found by
  // multiplying the number of the first byte in the block by BLOCK_LEN.
  // 
  uint16_t addrMult = 1;                    // init for SDHC. Block addressable
  if (pvt_GetCardType() == SDSC)            // SDSC is byte addressable
    addrMult = BLOCK_LEN;
  
  // Send the READ MULTIPLE BLOCK command and confirm R1 Response is good.
  CS_SD_LOW;
  sd_SendCommand(READ_MULTIPLE_BLOCK, FBS_SEARCH_START_BLOCK * addrMult); 
  if (sd_GetR1() != OUT_OF_IDLE)
  {
    CS_SD_HIGH;
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
    for (uint16_t timeout = 0; sd_ReceiveByteSPI() != START_BLOCK_TKN;)
      if (++timeout >= TIMEOUT_LIMIT)
      {
        CS_SD_HIGH;
        print_Str("\n\rSTART_TOKEN_TIMEOUT");
        return FAILED_FIND_BOOT_SECTOR;
      }

    // load all bytes of the sector into the block array
    for (uint16_t byteNum = 0; byteNum < BLOCK_LEN; ++byteNum) 
      blckArr[byteNum] = sd_ReceiveByteSPI();
    
    // 16-bit CRC. CRC is off (default) so these values do not matter.
    sd_ReceiveByteSPI(); 
    sd_ReceiveByteSPI(); 

    // confirm JMP BOOT and BOOT SIGNATURE bytes those of a FAT boot sector.
    if (((blckArr[0] == JMP_BOOT_1A && blckArr[2] == JMP_BOOT_3A) 
          || blckArr[0] == JMP_BOOT_1B)
          && (blckArr[BLOCK_LEN - 2] == BS_SIGN_1 
          &&  blckArr[BLOCK_LEN - 1] == BS_SIGN_2))
    {
      // Boot Sector has been found!
      sd_SendCommand(STOP_TRANSMISSION, 0); // stop sending data blocks.
      sd_ReceiveByteSPI();                  // R1B resp. Don't care.
      CS_SD_HIGH;
      return blkNum;                        // return success!
    }
  }

  // FAILED to find BS in the set block range.
  sd_SendCommand(STOP_TRANSMISSION, 0);     // stop sending data blocks.
  sd_ReceiveByteSPI();                      // R1B resp. Don't care.
  CS_SD_HIGH;
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
  // Determine card type. If SDHC then the SD card is block addressable and
  // the block number will be the address of the block. If SDSC then the card
  // is byte addressable, in which case the address of the block is the number
  // of the first byte in the block, thus the address would be found by
  // multiplying the number of the first byte in the block by BLOCK_LEN (=512).
  // 
  uint16_t addrMult = 1;                    // init for SDHC. Block addressable
  if (pvt_GetCardType() == SDSC)            // SDSC is block addressable
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

static uint8_t pvt_GetCardType(void)
{
  uint8_t cardType;

  CS_SD_LOW;
  sd_SendCommand(SEND_CSD, 0);
  if (sd_GetR1() != OUT_OF_IDLE) 
  { 
    CS_SD_HIGH; 
    return GET_CARD_TYPE_ERROR;             // R1 error. Failed to get the CSD
  }

  // Get CSD version to determine if card is SDHC or SDSC
  for (uint16_t timeout = 0; ; ++timeout)
  {
    if (timeout >= TIMEOUT_LIMIT)           // if timeout is reached
    { 
      // Read in rest of CSD bytes, though not used.
      for(int byteNum = 0; byteNum < CSD_BYTE_LEN - 1; ++byteNum) 
        sd_ReceiveByteSPI();
      CS_SD_HIGH;
      return GET_CARD_TYPE_ERROR;           // timeout fail                             
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
  CS_SD_HIGH;
  return cardType;                          // success
}
