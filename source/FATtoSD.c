/*
***********************************************************************************************************************
*                                           INTERFACE FOR AVR-FAT to AVR-SDCARD
* 
* File   : FATtoSD.C
* Author : Joshua Fain
* Target : ATMega1280
*
*
* DESCRIPTION:
* This file in included as an interface between the AVR-FAT module and the AVR-SDCard module. A module/driver for
* accessing raw data on a FAT32-formatted volume is required. The AVR-FAT module is intended to be independent of the
* physical volume, and implementing a different raw data access driver would is possible as long as "non-private"
* functions are implemented that have the same functionality as those here. 
* 
* FUNCTIONS:
* (1) uint32_t FATtoDisk_FindBootSector();                                               
* (2) uint8_t  FATtoDisk_ReadSingleSector (uint32_t address, uint8_t *sectorByteArry)
*
*
*                                                      MIT LICENSE
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


#include <avr/io.h>
#include "../includes/spi.h"
#include "../includes/prints.h"
#include "../includes/sd_spi_base.h"
#include "../includes/sd_spi_data_access.h"
#include "../includes/fat.h"
#include "../includes/fattosd.h"



// Declaring "private" function.
uint8_t pvt_getCardType();



/*
***********************************************************************************************************************
*                                          REQUIRED "PUBLIC" FUNCTIONS
***********************************************************************************************************************
*/


// This function is required to interface with the physical disk hosting the FAT volume. It returns
// a value corresponding to the addressed location of the Boot Sector / Bios Parameter Block on the
// physical disk. This function is used by FAT_SetBiosParameterBlock().
uint32_t FATtoDisk_FindBootSector()
{
    uint8_t  block[512];
    uint16_t timeout = 0;
    uint8_t  r1;
    uint8_t  bsflag = 0;
    uint32_t startBlockNumber = 0;
    uint32_t maxNumberOfBlocksToCheck = 10;
    uint32_t blockNumber = 0;
    uint8_t  cardType;
    uint16_t addressMultiplier = 0;

    cardType = pvt_getCardType();
    // SDHC is block addressable
    // SDSC byte addressable
    if (cardType == SDHC) addressMultiplier = 1;
    else addressMultiplier =  BLOCK_LEN;
    
    CS_LOW;
    SD_SendCommand (READ_MULTIPLE_BLOCK, startBlockNumber * addressMultiplier); // CMD18
    r1 = SD_GetR1();
    if (r1 > 0)
      {
        CS_HIGH
        print_str("\n\r R1 ERROR = "); SD_PrintR1(r1);
      }

    if (r1 == 0)
      {
        blockNumber = startBlockNumber * addressMultiplier;
        do
          {   
            timeout = 0;
            while (SD_ReceiveByteSPI() != 0xFE) // wait for start block token.
              {
                timeout++;
                if (timeout > 0x511) print_str ("\n\rSTART_TOKEN_TIMEOUT");
              }

            block[0] = SD_ReceiveByteSPI();
            if ((block[0] == 0xEB) || (block[0] == 0xE9))
              {
                for(uint16_t k = 1; k < BLOCK_LEN; k++) 
                  block[k] = SD_ReceiveByteSPI();
                if (((block[0] == 0xEB) && (block[2] == 0x90)) || block[0] == 0xE9)
                  {
                    if ((block[510] == 0x55) && (block[511] == 0xAA)) 
                      { 
                        bsflag = 1; 
                        break; 
                      }
                  }
              }

            else
              {
                for (uint16_t k = 1; k < BLOCK_LEN; k++) 
                  SD_ReceiveByteSPI();
              }

            for (uint8_t k = 0; k < 2; k++) 
              SD_ReceiveByteSPI(); // CRC
            
            blockNumber++;
          }
        while ((blockNumber * addressMultiplier) < ((startBlockNumber + maxNumberOfBlocksToCheck) * addressMultiplier));
        
        SD_SendCommand (STOP_TRANSMISSION, 0);
        SD_ReceiveByteSPI(); // R1b response. Don't care.
      }
    CS_HIGH;

    if (bsflag == 1) 
      return blockNumber;
    else 
      return 0xFFFFFFFF; // failed token
}



// This function is required to interface with the physical disk hosting the FAT volume. This function
// should load the contents of the sector at the physical address specified in the address argument into
// the array pointed at by *sectorByteArray. This function is used by all FAT functions requiring access
// to the physical disk. The function will return 1 if there is a read failure and 0 if it is successful. 
uint8_t FATtoDisk_ReadSingleSector ( uint32_t address, uint8_t *sectorByteArray)
{
    uint8_t  cardType;
    uint16_t err;
    uint8_t  db[512];
    uint32_t blockNumber = address;

    // determine if card is SDSC or SDHC/SDXC
    cardType = pvt_getCardType();

    // SDHC is block addressable
    // SDSC byte addressable
    if (cardType == SDHC) 
      err = SD_ReadSingleBlock(blockNumber, db);
    else 
      err = SD_ReadSingleBlock(blockNumber * BLOCK_LEN, db);

    if (err != READ_SUCCESS) 
      return 1; // failed
    
    for (int k = 0; k < 512; k++)
      sectorByteArray[k] = db[k];
    
    return 0; // successful
};



/*
***********************************************************************************************************************
*                                                  "PRIVATE" FUNCTION
***********************************************************************************************************************
*/

// Used here by the two required "public" functions to get the SD card's type. The type 
// of the SD card determines how a card is addressed, i.e. byte or block addressable. 
uint8_t pvt_getCardType()
{
    uint8_t r1 = 0;
    uint8_t cardType;
    uint8_t resp;
    uint8_t timeout = 0;

    // Get CSD version to determine if card is SDHC or SDSC
    CS_LOW;
    SD_SendCommand (SEND_CSD,0);
    r1 = SD_GetR1();
    if (r1 > 0) 
      { 
        CS_HIGH; 
        return 0xFF; // error getting CSD version
      }

    do
      {
        resp = SD_ReceiveByteSPI();
        if ((resp >> 6) == 0) 
          { 
            cardType = SDSC; 
            break;
          }
        else if ((resp >> 6) == 1) 
          { 
            cardType = SDHC; 
            break; 
          } 
        
        if(timeout++ >= 0xFF)
          { 
            // Ensure any portion of CSD sent is read in.
            for(int k = 0; k < 20; k++) 
              SD_ReceiveByteSPI();
            CS_HIGH; 
            return 0xFF; 
          }
      }
    while(1);

    // Not used, but must read in rest of CSD.
    for(int k = 0; k < 20; k++) 
      SD_ReceiveByteSPI();
    
    CS_HIGH;

    return cardType;
}