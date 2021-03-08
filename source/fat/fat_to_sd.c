/*
 * File    : FATtoSD.C
 * Version : 0.0.0.2
 * Author  : Joshua Fain
 * Target  : ATMega1280
 * License : MIT
 * Copyright (c) 2020 Joshua Fain
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
#include "fat_to_disk.h"

/*
 ******************************************************************************
 *                      "PRIVATE" FUNCTION PROTOTYPES
 ******************************************************************************
 */
static uint8_t pvt_GetCardType(void);

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
 *               SD card. This function is used by fat_SetBPB().
 * 
 * Arguments   : void
 * 
 * Returns     : Address of the boot sector on the SD card.
 * 
 * Notes       : The search for the boot sector will search a total of
 *               'maxNumOfBlcksToChck' starting at 'startBlckNum'.
 * ----------------------------------------------------------------------------
 */
uint32_t FATtoDisk_FindBootSector(void)
{
  // search range variables
  const uint32_t maxNumOfBlcksToChck = 50;  // total num of blocks to search
  const uint32_t startBlckNum = 0;          // block num to start the search

  // other
  uint32_t blckNum;                         // return value. Block num of BS
  uint8_t  r1;                              // R1 response

  //
  // Determine card type. If SDHC then the SD card is block addressable and
  // the block number will be the address of the block. If SDSC then the card
  // is byte addressable, in which case the address of the block is the number
  // of the first byte in the block, thus the address would be found by
  // multiplying the number of the first byte in the block by BLOCK_LEN (=512).
  // 
  uint16_t addrMult = 1;                    // init for SDHC. Block addressable
  if (pvt_GetCardType() == SDSC)            // SDSC --> byte addressable
    addrMult = BLOCK_LEN;
  
  // Send the READ MULTIPLE BLOCK command and confirm R1 Response is good.
  CS_SD_LOW;
  sd_SendCommand(READ_MULTIPLE_BLOCK, startBlckNum * addrMult); 
  if ((r1 = sd_GetR1()) > OUT_OF_IDLE)
  {
    CS_SD_HIGH
    //print_Str("\n\r R1 ERROR = "); 
    //sd_PrintR1(r1);
    return FAILED_FIND_BOOT_SECTOR;
  }
  else
  {  
    blckNum = startBlckNum * addrMult;      // block address / number
    do
    {   
      uint8_t  blckArr[BLOCK_LEN];          // to hold the block data bytes
      uint16_t timeout = 0;

      //
      // 0xFE is the Start Block Token. This token is sent by the
      // SD Card to signal data that it is about to start sending data.
      //
      while (sd_ReceiveByteSPI() != 0xFE)
      {
        timeout++;
        if (timeout > TIMEOUT_LIMIT)
        { 
          print_Str("\n\rSTART_TOKEN_TIMEOUT");
          return FAILED_FIND_BOOT_SECTOR;
        }
      }

      // load all bytes of the sector into the block array
      for (uint16_t byteNum = 0; byteNum < BLOCK_LEN; byteNum++) 
        blckArr[byteNum] = sd_ReceiveByteSPI();
      
      // 16-bit CRC. CRC is off (default) so these values do not matter.
      sd_ReceiveByteSPI(); 
      sd_ReceiveByteSPI(); 

      //
      // The values in theses byte array positions must be set as indicated
      // or this is either not the boot sector / BPB, or it is corrupt.
      //
      if (((blckArr[0] == 0xEB && blckArr[2] == 0x90) || blckArr[0] == 0xE9)
            && (blckArr[510] == 0x55 && blckArr[511] == 0xAA))
      {
        // Successfully found the boot sector!
        sd_SendCommand(STOP_TRANSMISSION, 0);   // stop sending data blocks.
        sd_ReceiveByteSPI();                     // R1B resp. Don't care.
        CS_SD_HIGH;
        return blckNum;                          // return success!
      }

      blckNum++;
    }
    while (blckNum < startBlckNum + maxNumOfBlcksToChck);
  }
  
  // FAILED to find BS in the set block range.
  sd_SendCommand(STOP_TRANSMISSION, 0);          // stop sending data blocks.
  sd_ReceiveByteSPI();                           // R1B resp. Don't care.
  CS_SD_HIGH;
  return FAILED_FIND_BOOT_SECTOR;                // return failed token
}

/* 
 * ----------------------------------------------------------------------------
 *                                                 READ SINGLE SECTOR FROM DISK
 *                                       
 * Description : Loads the contents of the sector/block at the specified 
 *               address on the SD card into the array, *arr.
 *
 * Arguments   : addr     Address of the sector/block on the SD card that 
 *                        should be read into the array, *arr.
 * 
 *               arr      Pointer to the array that will be loaded with the 
 *                        contents of the sector/block on the SD card at 
 *                        address, addr.
 * 
 * Returns     : READ_SECTOR_SUCCES if successful.
 *               READ_SECTOR_FAILED if failure.
 * ----------------------------------------------------------------------------
 */
uint8_t FATtoDisk_ReadSingleSector(uint32_t blckNum, uint8_t *blckArr)
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
  if (sd_ReadSingleBlock(blckNum * addrMult, blckArr) == READ_SUCCESS)
    return READ_SECTOR_SUCCESS; 
  else
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
 * Returns     : SD card type - SDSC or SDHC.
 * ----------------------------------------------------------------------------
 */

static uint8_t pvt_GetCardType(void)
{
  uint8_t cardType;
  uint8_t resp;
  uint8_t timeout = 0;

  CS_SD_LOW;
  sd_SendCommand(SEND_CSD, 0);
  if (sd_GetR1() > OUT_OF_IDLE) 
  { 
    CS_SD_HIGH; 
    return 0xFF;                            // R1 error. Failed to get the CSD                       
  }
  // Get CSD version to determine if card is SDHC or SDSC
  do
  {
    resp = sd_ReceiveByteSPI();
    // if bit 6 of first CSD byte is 0 --> SDSC or 1 --> SDHC. 
    if (resp >> 6 == 0) 
    { 
      cardType = SDSC; 
      break;
    }
    else if (resp >> 6 == 1) 
    { 
      cardType = SDHC; 
      break; 
    } 
    // if timeout is reached
    if (timeout++ >= TIMEOUT_LIMIT)
    { 
      // Receive rest of CSD bytes. No used.
      for(int byteNum = 0; byteNum < 20; byteNum++) 
        sd_ReceiveByteSPI();

      CS_SD_HIGH;

      // timeout fail
      return 0xFF;                               
    }
  }
  while(1);

  // Not used, but should read in rest of CSD.
  for(int byteNum = 0; byteNum < 20; byteNum++) 
    sd_ReceiveByteSPI();
  
  CS_SD_HIGH;

  // success
  return cardType;
}
