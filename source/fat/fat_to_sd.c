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

uint8_t pvt_getCardType (void);


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
 *               SD card. This function is used by fat_setBPB().
 * 
 * Arguments   : void
 * 
 * Returns     : Address of the boot sector on the SD card.
 * ----------------------------------------------------------------------------
 */

uint32_t FATtoDisk_findBootSector (void)
{
  uint8_t  block[512];
  uint16_t timeout = 0;
  uint8_t  r1;
  uint8_t  bsflag = 0;
  uint32_t startBlckNum = 0;
  uint32_t maxNumOfBlcksToChck = 10;
  uint32_t blckNum = 0;
  uint8_t  cardType;
  uint16_t addrMult;                             // address multiplier

  // Determine card type.
  cardType = pvt_getCardType();
  if (cardType == SDHC)                          // SDHC is block addressable
    addrMult = 1;
  else                                           // SDSC byte addressable
    addrMult = BLOCK_LEN;
  
  CS_SD_LOW;
  sd_sendCommand (READ_MULTIPLE_BLOCK, startBlckNum * addrMult); 
  r1 = sd_getR1();
  if (r1 > 0)
  {
    CS_SD_HIGH
    print_str ("\n\r R1 ERROR = "); 
    sd_printR1 (r1);
  }

  if (r1 == 0)
  {
    blckNum = startBlckNum * addrMult;
    do
    {   
      timeout = 0;
      while (sd_receiveByteSPI() != 0xFE)        // wait for start block token
      {
        timeout++;
        if (timeout > 0xFE) 
          print_str ("\n\rSTART_TOKEN_TIMEOUT");
      }

      block[0] = sd_receiveByteSPI();
      if (block[0] == 0xEB || block[0] == 0xE9)
      {
        for (uint16_t k = 1; k < BLOCK_LEN; k++) 
          block[k] = sd_receiveByteSPI();

        if ((block[0] == 0xEB && block[2] == 0x90) || block[0] == 0xE9)
        {
          if (block[510] == 0x55 && block[511] == 0xAA) 
          { 
            bsflag = 1; 
            break; 
          }
        }
      }

      else
      {
        for (uint16_t k = 1; k < BLOCK_LEN; k++) 
          sd_receiveByteSPI();
      }

      // get CRC
      for (uint8_t k = 0; k < 2; k++) 
        sd_receiveByteSPI(); 
      
      blckNum++;
    }
    while (blckNum * addrMult 
           < (startBlckNum + maxNumOfBlcksToChck) * addrMult);
    
    sd_sendCommand (STOP_TRANSMISSION, 0);

    // Get the R1b response. Value doesn't matter.
    sd_receiveByteSPI(); 
  }
  CS_SD_HIGH;

  if (bsflag == 1) 
    return blckNum;                              // success
  else 
    return 0xFFFFFFFF;                           // failed token
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
 * Returns     : 0 if successful.
 *               1 if failure.
 * ----------------------------------------------------------------------------
 */

uint8_t FATtoDisk_readSingleSector (uint32_t addr, uint8_t *arr)
{
  uint8_t  cardType;
  uint16_t err;
  uint8_t  db[512];
  uint32_t blckNum = addr;

  // determine if card is SDSC or SDHC/SDXC
  cardType = pvt_getCardType();

  
  if (cardType == SDHC)                          // SDHC is block addressable
    err = sd_readSingleBlock (blckNum, db);
  else                                           // SDSC is byte addressable
    err = sd_readSingleBlock (blckNum * BLOCK_LEN, db);

  // failed
  if (err != READ_SUCCESS)
    return 1;                                  
  
  // load contents of data block into *arr
  for (int k = 0; k < 512; k++)
    arr[k] = db[k];
        
  // success
  return 0;
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

uint8_t pvt_getCardType()
{
  uint8_t r1 = 0;
  uint8_t cardType;
  uint8_t resp;
  uint8_t timeout = 0;

  CS_SD_LOW;
  sd_sendCommand (SEND_CSD, 0);
  r1 = sd_getR1();

  // error getting CSD
  if (r1 > 0) 
  { 
    CS_SD_HIGH; 
    return 0xFF;                             
  }

  // Get CSD version to determine if card is SDHC or SDSC
  do
  {
    resp = sd_receiveByteSPI();
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
    
    if (timeout++ >= 0xFF)
    { 
      // Ensure any portion of CSD sent is read in to clear out register.
      for(int k = 0; k < 20; k++) 
        sd_receiveByteSPI();

      CS_SD_HIGH;
      // timeout fail
      return 0xFF;                               
    }
  }
  while(1);

  // Not used, but should read in rest of CSD.
  for(int k = 0; k < 20; k++) 
    sd_receiveByteSPI();
  
  CS_SD_HIGH;

  return cardType;
}
