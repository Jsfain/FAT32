/*
***********************************************************************************************************************
* File   : FATtoSD.C
* Author : Joshua Fain
* Target : ATMega1280
*
*
* DESCRIPTION: 
* 
*                                                
*                                                       MIT LICENSE
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


// declare
uint8_t pvt_getCardType();

uint32_t fat_FindBootSector()
{
    uint8_t block[512];
    uint16_t timeout = 0;
    uint8_t r1;
    uint8_t bsflag = 0;
    uint32_t startBlockNumber = 0;
    uint32_t maxNumberOfBlocksToCheck = 10;
    uint32_t blockNumber = 0;
    uint8_t cardType;
    uint16_t addressMultiplier = 0;

    cardType = pvt_getCardType();

    if (cardType == SDHC) // SDHC is block addressable
        addressMultiplier = 1;
    else // SDSC byte addressable
        addressMultiplier =  BLOCK_LEN;
    
    CS_LOW;
    SD_SendCommand(READ_MULTIPLE_BLOCK, startBlockNumber * addressMultiplier); // CMD18
    r1 = SD_GetR1();
    if(r1 > 0)
    {
        CS_HIGH
        //return ( R1_ERROR | r1 );
        print_str("\n\r R1 ERROR = "); SD_PrintR1(r1);
    }

    if(r1 == 0)
    {
        blockNumber = startBlockNumber * addressMultiplier;
        do
        {   
            timeout = 0;
            while( SD_ReceiveByteSPI() != 0xFE ) // wait for start block token.
            {
                timeout++;
                if(timeout > 0x511) //return (START_TOKEN_TIMEOUT | r1);
                    print_str("\n\rSTART_TOKEN_TIMEOUT");
            }

            block[0] = SD_ReceiveByteSPI();
            if( (block[0] == 0xEB) || (block[0] == 0xE9) )
            {
                for(uint16_t k = 1; k < BLOCK_LEN; k++)
                {
                    block[k] = SD_ReceiveByteSPI();
                }
                if ( ((block[0] == 0xEB) && (block[2] == 0x90)) || block[0] == 0xE9)
                {
                    if( (block[510] == 0x55) && (block[511] == 0xAA) ) { bsflag = 1; break; }
                }
            }

            else
            {
                for(uint16_t k = 1; k < BLOCK_LEN; k++) SD_ReceiveByteSPI();
            }

            for(uint8_t k = 0; k < 2; k++) 
                SD_ReceiveByteSPI(); // CRC
            
            blockNumber++;
        }while( (blockNumber * addressMultiplier) < ((startBlockNumber + maxNumberOfBlocksToCheck) * addressMultiplier) );
        
        SD_SendCommand(STOP_TRANSMISSION,0);
        SD_ReceiveByteSPI(); // R1b response. Don't care.
    }
    CS_HIGH;
    if(bsflag == 1) return blockNumber;
    else return 0xFFFFFFFF;

    //return READ_SUCCESS;
}



uint8_t fat_ReadSingleSector( uint32_t address, uint8_t *sectorByteArray)
{
    uint8_t cardType;
    uint16_t err;
    uint8_t db[512];
    uint32_t blockNumber = address;

    // determine if card is SDSC or SDHC/SDXC
    cardType = pvt_getCardType();

    if (cardType == SDHC) // SDHC is block addressable
        err = SD_ReadSingleBlock(blockNumber, db);
    else // SDSC byte addressable
        err = SD_ReadSingleBlock(blockNumber * BLOCK_LEN, db);

    if(err != READ_SUCCESS) return READ_SECTOR_ERROR;
    
    for(int k = 0; k < 512; k++) { sectorByteArray[k] = db[k]; }
    
    return READ_SECTOR_SUCCESSFUL;
};



uint8_t pvt_getCardType()
{
    uint8_t r1 = 0;
    uint8_t cardType;
    uint8_t resp;
    uint8_t timeout = 0;

    // Get CSD version to determine if card is SDHC or SDSC
    CS_LOW;
    SD_SendCommand(SEND_CSD,0);
    r1 = SD_GetR1();
    if(r1 > 0) 
    { 
        CS_HIGH; 
        return 0xFF; // error getting CSD version
    }

    do{
        resp = SD_ReceiveByteSPI();
        if( (resp >> 6) == 0 ) { cardType = SDSC; break; }
        else if((resp >> 6) == 1 ) { cardType = SDHC; break; } 
        
        if(timeout++ >= 0xFF)
        { 
            // Ensure any portion of CSD sent is read in.
            for(int k = 0; k < 20; k++) SD_ReceiveByteSPI();
            CS_HIGH; 
            return 0xFF; 
        }
    }while(1);

    // Not used, but must read in rest of CSD.
    for(int k = 0; k < 20; k++) SD_ReceiveByteSPI();
    
    CS_HIGH;

    return cardType;
}