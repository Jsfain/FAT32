/*
 * File       : FAT_SD_IF.C
 * Version    : 0.1
 * License    : GNU GPLv3
 * Author     : Joshua Fain
 * Copyright (c) 2020 - 2026
 * 
 * This is an implementation of FAT_DISK_IF.H and is specific to the disk 
 * module used for accessing the raw data on the FAT32 formatted disk. It 
 * should include the definitions for the function prototypes in 
 * FAT_DISK_IF.H as well as any additional definitions required by the disk 
 * interface module itself. In the case here the disk module is the SD_SPI.
 */

#include <stdint.h>
#include "fat_disk_if.h"
#include "sd_spi_base.h"
#include "sd_spi_rwe.h"
#include "sd_spi_print.h"

/*
#include "avr_usart.h"
#include "prints.h"
static void (*g_outs)(uint8_t) = usart_Transmit;
*/
/*
 ******************************************************************************
 *                 DISK IMPLEMENTATION-SPECIFIC DEFINITIONS
 * 
 * This section is for additional global variables, macros, and functions
 * that are needed in order for the required FAT_DISK_IF.H functions to work
 * with the included disk module.
 ******************************************************************************
 */

//
// "global" static varible for the 'CardTypeVersion' struct defined in
// SD_SPI_BASE. This is set in the FindBootSector function below during SD card
// initialization but is needed for the read disk sector function.
//
static CTV g_ctv;

//
// "global" static variable for the initialization response of the SD Card 
//
static uint32_t g_sdInitResp;

// The number of times the module can attempt to initialize the SD card.
#define SD_CARD_INIT_ATTEMPTS_MAX      5    // set to any value <= 255.

//
// Used by fatDisk_FindBootSector to specify the range of blocks to search for 
// the boot sector. This limits the range because the process can be slow. 
//
#define BS_SEARCH_START_BLOCK         0    // specifies starting block
#define BS_MAX_NUM_BLKS_SEARCH_MAX    50   // specifies number of blocks

/* 
 * ----------------------------------------------------------------------------
 *                                       SD CARD INIT and GET CARD TYPE\VERSION
 *                                       
 * Description : Initializes the SD Card into SPI mode and sets the 
 *               CardTypeVersion ctv global variable. The card type is needed 
 *               to determine if the SD card is block or byte addressable.
 * 
 * Arguments   : *ctv - pointer to CardTypeVersion struct variable.
 * 
 * Returns     : void
 * 
 * Notes       : Possible Card Type settings - SDSC, SDHC, GET_CARD_TYPE_ERROR
 * ----------------------------------------------------------------------------
 */

static void pvt_SDCardInit(CTV *cardTypeVers)
{
  // Loop until SD card init succeeds or max attempts reached.
  for (uint8_t att = 0; att < SD_CARD_INIT_ATTEMPTS_MAX; ++att)
  {
    g_sdInitResp = sd_InitSpiMode(cardTypeVers); // init SD Card
    if (g_sdInitResp == OUT_OF_IDLE)             // If init fails try again
      break;
  }
}
/*************** END DISK IMPLEMENTATION-SPECIFIC DEFINITIONS ****************/ 



/*
 ******************************************************************************
 *                                 FUNCTIONS
 ******************************************************************************
 */

/*
 * ----------------------------------------------------------------------------
 *                                                             FIND BOOT SECTOR
 *                                 
 * Description : Finds address of the boot sector on a FAT-formatted SD card.
 *               This function is required by fat_SetBPB in fat.c. It
 *               performs a search for the block containing the Jump Boot and 
 *               Boot Signature bits and returns this block address. 
 * 
 * Arguments   : void
 * 
 * Returns     : Address of the boot sector on the SD card.
 * 
 * Notes       : The boot sector search will begin at BS_SEARCH_START_BLOCK,
 *               and search a total of BS_MAX_NUM_BLKS_SEARCH_MAX blocks. 
 *               These macros are defined here in this file. 
 * ----------------------------------------------------------------------------
 */
uint32_t fatDisk_FindBootSector(void)
{
  // Initialize the FAT formatted SD card 
  pvt_SDCardInit(&g_ctv);

  //
  // SD cards addressing is determined by card type. If type is SDHC then it is
  // block (sector) addressable. If SDSC then byte addressable. If SDSC, the 
  // block number is multiplied by the block length giving the address of the 
  // first byte in the block.
  //
  uint16_t addrMult = 1;                    // init for SDHC (assumed).
  if (g_ctv.type == SDSC)
    addrMult = SECTOR_LEN;

  // Send the READ_MULTIPLE_BLOCK command to confirm R1 Response is good.
  CS_ASSERT;
  sd_SendCommand(READ_MULTIPLE_BLOCK, BS_SEARCH_START_BLOCK * addrMult); 
  if (sd_GetR1() != OUT_OF_IDLE)
  {
    CS_DEASSERT;
    return FAILED_FIND_BOOT_SECTOR;
  }

  // loop to search for the boot sector and set blkNum to its address.
  for (uint32_t blkNum = BS_SEARCH_START_BLOCK * addrMult;
       blkNum < BS_SEARCH_START_BLOCK + BS_MAX_NUM_BLKS_SEARCH_MAX;
       ++blkNum)
  {   
    uint8_t blckArr[SECTOR_LEN];            // to hold the block data bytes

    //
    // loop until the 'Start Block Token' has been received from the SD card,
    // which indicates data from requested block is about to be sent.
    //
    for (uint16_t attempt = 0; sd_ReceiveByteSPI() != START_BLOCK_TKN;)
      if (++attempt >= MAX_CR_ATT)
      {
        CS_DEASSERT;
        //print_Str("\n\rFailed receive START_BLOCK_TOKEN from SD card.", outs);
        return FAILED_FIND_BOOT_SECTOR;
      }

    // load all bytes of the sector into the block array
    for (uint16_t byteNum = 0; byteNum < SECTOR_LEN; ++byteNum) 
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
          if (blckArr[BS_SIGN_LSB_POS] == BS_SIGN_LSB &&
             blckArr[BS_SIGN_MSB_POS] == BS_SIGN_MSB)
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
 *               address on the SD card into array, blckArr.
 *
 * Arguments   : blkNum    - Block number/address of the sector/block on the SD
 *                           card that should be read into blkArr.
 * 
 *               blkArr    - Pointer to the array that will be loaded with the 
 *                           contents of the sector/block on the SD card at the
 *                           block specified by blkNum.
 * 
 * Returns     : READ_SECTOR_SUCCES if successful.
 *               FAILED_READ_SECTOR if failure.
 * ----------------------------------------------------------------------------
 */
uint8_t fatDisk_ReadSector(uint32_t blkNum, uint8_t blkArr[])
{
  //
  // For SD cards, addressing is determined by the card type. If the card type 
  // is SDHC then it is block (sector) addressable. If it is SDSC then it is
  // byte addressable. The assumed default is SDHC, and the block is addressed
  // by the block number. If SDSC then the block number is multiplied by the
  // block length to deterime the byte address of the first by in the block.
  // 
  uint16_t addrMult = 1;                    // init for SDHC.
  if (g_ctv.type == SDSC)
    addrMult = SECTOR_LEN;

  // Load data block into array by passing the array to the Read Block function
  if (sd_ReadSingleBlock(blkNum * addrMult, blkArr) == READ_SUCCESS)
    return READ_SECTOR_SUCCESS; 
  return FAILED_READ_SECTOR;
};

