#include <avr/io.h>
#include "../includes/spi.h"
#include "../includes/prints.h"
#include "../includes/sd_spi_base.h"
#include "../includes/sd_spi_data_access.h"
#include "../includes/fattosd.h"


uint32_t fat_FindBootSector()
{
    uint8_t block[512];
    uint16_t timeout = 0;
    uint8_t r1;
    uint8_t bsflag = 0;
    uint32_t startBlockAddress = 8190;
    uint32_t maxNumberOfBlocksToCheck = 10;
    uint32_t blockNumber = 0;

    for ( blockNumber = startBlockAddress; blockNumber < startBlockAddress + maxNumberOfBlocksToCheck; blockNumber++ )
    {
        CS_LOW;
        SD_SendCommand(READ_SINGLE_BLOCK, blockNumber); //CMD17
        r1 = SD_GetR1();

        if(r1 > 0)
        {
            CS_HIGH;
            //return ( R1_ERROR | r1 );
            print_str("\n\r R1 ERROR = "); SD_PrintR1(r1);
        }

        if(r1 == 0)
        {
            timeout = 0;
            uint8_t sbt = SD_ReceiveByteSPI(); 
            while(sbt != 0xFE) // wait for Start Block Token
            {
                sbt = SD_ReceiveByteSPI();
                timeout++;
                if(timeout > 0xFE) 
                { 
                    CS_HIGH;
                    //return ( START_TOKEN_TIMEOUT | r1 );
                    print_str("\n\rSTART_TOKEN_TIMEOUT");
                    break;
                }
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
                CS_HIGH
                for(uint16_t k = 1; k < BLOCK_LEN; k++)
                {
                    SD_ReceiveByteSPI();
                }
            }

            for(uint8_t k = 0; k < 2; k++) 
                SD_ReceiveByteSPI(); // CRC

            SD_ReceiveByteSPI(); // clear any remaining bytes from SPDR
        }

        CS_HIGH;
    }
    //return ( READ_SUCCESS | r1 );
    if(bsflag == 1) return blockNumber;
    else return 0xFFFFFFFF;

    /*
    CS_LOW;
    SD_SendCommand(READ_MULTIPLE_BLOCK, startBlockAddress); // CMD18
    r1 = SD_GetR1();
    if(r1 > 0)
    {
        CS_HIGH
        //return ( R1_ERROR | r1 );
        print_str("\n\r R1 ERROR = "); SD_PrintR1(r1);
    }

    if(r1 == 0)
    {
        for ( blockNumber = startBlockAddress; blockNumber < startBlockAddress + maxNumberOfBlocksToCheck; blockNumber++ )
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
                for(uint16_t k = 1; k < BLOCK_LEN; k++)
                {
                    SD_ReceiveByteSPI();
                }
            }

            for(uint8_t k = 0; k < 2; k++) 
                SD_ReceiveByteSPI(); // CRC
        }
        
        SD_SendCommand(STOP_TRANSMISSION,0);
        SD_ReceiveByteSPI(); // R1b response. Don't care.
    }

    print_str("blockNumber = ");print_dec(blockNumber); 
    CS_HIGH;
    if(bsflag == 1) return blockNumber;
    else return 0xFFFFFFFF;

    //return READ_SUCCESS;
    */
    /*
    uint8_t sectorArray[BLOCK_LEN];
    uint32_t blockNumber = 8100;
    uint8_t bsflag = 0;

    do
    {
        fat_ReadSingleSector(blockNumber,sectorArray);
        if( ((sectorArray[0] == 0xEB) && (sectorArray[2] == 0x90)) || (sectorArray[0] == 0xE9) )
        {
            print_str("\n\rhere");
            if( (sectorArray[510] == 0x55) && (sectorArray[511] == 0xAA) ) { bsflag = 1; break; }
        }
        blockNumber++;

    }while(blockNumber < 8200);

    if(bsflag == 1) return blockNumber;
    else return 0xFFFFFFFF;
    */
}

void fat_ReadSingleSector( uint32_t address, uint8_t *sectorByteArray)
{
    uint8_t db[512];

    // Use CSD to determine if SDHC or SDSC
     uint8_t r1 = 0;

    
    // SEND_CSD (CMD9)
    CS_LOW;
    SD_SendCommand(SEND_CSD,0);
    r1 = SD_GetR1(); // Get R1 response
    if(r1 > 0) { CS_HIGH; }

    // ***** Get rest of the response bytes which are the CSD Register and CRC.

    // Other variables
    uint8_t resp;
    uint8_t timeout = 0;
    uint8_t type;
    uint16_t err;
    uint32_t blockNumber;

    do{ // CSD_STRUCTURE - Must be 0 for SDSC types.
        resp = SD_ReceiveByteSPI();
        if( (resp >> 6) == 0 ) { type = SDSC; break; }
        else if((resp >> 6) == 1 ) { type = SDHC; break; } 
        
        if(timeout++ >= 0xFF){ CS_HIGH; }
    }while(1);

    for(int k = 0; k < 20; k++) SD_ReceiveByteSPI(); // read in rest of CSD to clear the register.

    CS_HIGH;
    

    blockNumber = address;
    if (type == SDHC) // SDHC is block addressable
        { 
            err = SD_ReadSingleBlock(blockNumber, db);
        }
    else // SDSC byte addressable
        { err = SD_ReadSingleBlock(blockNumber * BLOCK_LEN, db); }

    if(err != READ_SUCCESS)
    { 
        print_str("\n\r >> SD_ReadSingleBlock() returned ");
        if(err & R1_ERROR)
        {
            print_str("R1 error: ");
            SD_PrintR1(err);
        }
        else 
        { 
            print_str(" error "); 
            SD_PrintReadError(err);
        }
    }
    for(int k = 0; k < 512; k++) { sectorByteArray[k] = db[k]; }
};