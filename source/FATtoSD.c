/*
*******************************************************************************
*                   INTERFACE FOR AVR-FAT to AVR-SD CARD MODULE
*
* File    : FATtoSD.C
* Version : 0.0.0.2
* Author  : Joshua Fain
* Target  : ATMega1280
* License : MIT
* Copyright (c) 2020 Joshua Fain
* 
*
* DESCRIPTION: 
* Implements required interface functions declared in FATtoDISK_INTERFACE.H. 
* These have been implemented here for accessing raw data on a FAT32-formatted
* SD Card using the AVR-SD Card module.
*******************************************************************************
*/



#include <avr/io.h>
#include "spi.h"
#include "prints.h"
#include "sd_spi_base.h"
#include "sd_spi_rwe.h"
#include "fat_bpb.h"
#include "fat.h"
#include "fattodisk_interface.h"





/*
*******************************************************************************
*******************************************************************************
 *                     
 *                      "PRIVATE" FUNCTION DECLARATIONS      
 *  
*******************************************************************************
*******************************************************************************
*/

uint8_t 
pvt_getCardType (void);



/*
*******************************************************************************
*******************************************************************************
 *                     
 *                           FUNCTION DEFINITIONS       
 *  
*******************************************************************************
*******************************************************************************
*/

/* 
------------------------------------------------------------------------------
|                              FIND BOOT SECTOR
|                                        
| Description : This function must be implemented to find address of the the 
|               boot sector on a FAT32-formatted disk. This function is used
|               by fat_setBPB();
|
| Returns     : address of the boot sector on the physical disk.
-------------------------------------------------------------------------------
*/

uint32_t 
FATtoDisk_findBootSector (void)
{
    uint8_t  block[512];
    uint16_t timeout = 0;
    uint8_t  r1;
    uint8_t  bsflag = 0;
    uint32_t startBlckNum = 0;
    uint32_t maxNumOfBlcksToChck = 10;
    uint32_t blckNum = 0;
    uint8_t  cardType;
    uint16_t addrMult = 0; // address multiplier

    cardType = pvt_getCardType();
    // SDHC is block addressable
    // SDSC byte addressable
    if (cardType == SDHC) addrMult = 1;
    else addrMult =  BLOCK_LEN;
    
    CS_SD_LOW;
    sd_sendCommand (READ_MULTIPLE_BLOCK, 
                    startBlckNum * addrMult); // CMD18
    r1 = sd_getR1();
    if (r1 > 0)
      {
        CS_SD_HIGH
        print_str("\n\r R1 ERROR = "); sd_printR1(r1);
      }

    if (r1 == 0)
      {
        blckNum = startBlckNum * addrMult;
        do
          {   
            timeout = 0;
            while (sd_receiveByteSPI() != 0xFE) // wait for start block token.
              {
                timeout++;
                if (timeout > 0x511) print_str ("\n\rSTART_TOKEN_TIMEOUT");
              }

            block[0] = sd_receiveByteSPI();
            if ((block[0] == 0xEB) || (block[0] == 0xE9))
              {
                for(uint16_t k = 1; k < BLOCK_LEN; k++) 
                  block[k] = sd_receiveByteSPI();
                if (((block[0] == 0xEB) && (block[2] == 0x90))
                      || block[0] == 0xE9)
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
                  sd_receiveByteSPI();
              }

            for (uint8_t k = 0; k < 2; k++) 
              sd_receiveByteSPI(); // CRC
            
            blckNum++;
          }
        while ((blckNum * addrMult) < (
               (startBlckNum + maxNumOfBlcksToChck) * addrMult));
        
        sd_sendCommand (STOP_TRANSMISSION, 0);
        sd_receiveByteSPI(); // R1b response. Don't care.
      }
    CS_SD_HIGH;

    if (bsflag == 1) 
      return blckNum;
    else 
      return 0xFFFFFFFF; // failed token
}



/* 
------------------------------------------------------------------------------
|                       READ SINGLE SECTOR FROM DISK
|                                        
| Description : This function must be implemented to load the contents of the
|               sector at a specified address on the physical address to an 
|               array.
|
| Arguments   : addr   - address of the sector on the physical disk that should
|                        be read into the array, *arr.
|             : *arr   - ptr to the array that will be loaded with the contents
|                        of the disk sector at the specified address.
|
| Returns     : 0 if successful, 1 if there is a read failure.
-------------------------------------------------------------------------------
*/

uint8_t
FATtoDisk_readSingleSector (uint32_t addr, uint8_t *arr)
{
    uint8_t  cardType;
    uint16_t err;
    uint8_t  db[512];
    uint32_t blckNum = addr;

    // determine if card is SDSC or SDHC/SDXC
    cardType = pvt_getCardType();

    // SDHC is block addressable SDSC byte addressable
    if (cardType == SDHC)
      err = sd_readSingleBlock (blckNum, db);
    else // SDSC
      err = sd_readSingleBlock (blckNum * BLOCK_LEN, db);

    if (err != READ_SUCCESS)
      return 1; // failed
    
    for (int k = 0; k < 512; k++)
      arr[k] = db[k];    
    return 0; // successful
};



/*
*******************************************************************************
*******************************************************************************
 *                     
 *                        "PRIVATE" FUNCTION DEFINITIONS        
 *  
*******************************************************************************
*******************************************************************************
*/

/* 
------------------------------------------------------------------------------
|                              GET SD CARD TYPE
|                                        
| Description : Determines and returns the SD card type. The card type 
|               determines if it is block or byte addressable.
|
| Returns     : SD card type - SDSC or SDHC.
-------------------------------------------------------------------------------
*/

uint8_t pvt_getCardType()
{
    uint8_t r1 = 0;
    uint8_t cardType;
    uint8_t resp;
    uint8_t timeout = 0;

    // Get CSD version to determine if card is SDHC or SDSC
    CS_SD_LOW;
    sd_sendCommand (SEND_CSD,0);
    r1 = sd_getR1();
    if (r1 > 0) 
      { 
        CS_SD_HIGH; 
        return 0xFF; // error getting CSD version
      }

    do
      {
        resp = sd_receiveByteSPI();
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
              sd_receiveByteSPI();
            CS_SD_HIGH; 
            return 0xFF; 
          }
      }
    while(1);

    // Not used, but must read in rest of CSD.
    for(int k = 0; k < 20; k++) 
      sd_receiveByteSPI();
    
    CS_SD_HIGH;

    return cardType;
}