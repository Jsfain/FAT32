/******************************************************************************
 * Author: Joshua Fain
 * Date:   7/5/2020
 * 
 * Contians main()
 * File used to test out the sd card functions. Changes regularly
 * depending on what is being tested.
 * ***************************************************************************/

#include <string.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include "../includes/usart.h"
#include "../includes/spi.h"
#include "../includes/prints.h"
#include "../includes/sd_spi_base.h"
#include "../includes/sd_spi_data_access.h"
#include "../includes/fat.h"
#include "../includes/fattosd.h"


uint32_t enterBlockNumber();

//  *******   MAIN   ********  
int main(void)
{
    USART_Init();
    SPI_MasterInit();


    // ******************* SD CARD INITILIAIZATION ****************
    //
    // Initializing ctv. These members will be set by the SD card's
    // initialization routine. They should only be set there.
    
    CardTypeVersion ctv;

    uint32_t initResponse;

    // Attempt, up to 10 times, to initialize the SD card.
    for(int i = 0; i < 10; i++)
    {
        print_str("\n\n\r SD Card Initialization Attempt # "); print_dec(i);
        initResponse = SD_InitializeSPImode(&ctv);
        if( ( (initResponse & 0xFF) != OUT_OF_IDLE) && 
            ( (initResponse & 0xFFF00) != INIT_SUCCESS ) )
        {    
            print_str("\n\n\r FAILED INITIALIZING SD CARD");
            print_str("\n\r Initialization Error Response: "); 
            SD_PrintInitError(initResponse);
            print_str("\n\r R1 Response: "); SD_PrintR1(initResponse);
        }
        else
        {   print_str("\n\r SUCCESSFULLY INITIALIZED SD CARD");
            break;
        }
    }
    int t = -1;
    print_str("\n\n\r integer test"); print_dec(t);

    if(initResponse==OUT_OF_IDLE) // initialization successful
    {          
        print_str("\n\rGetting BPB");
        BPB bpb;
        uint16_t err;
        err = FAT_SetBiosParameterBlock(&bpb);
        print_str("\n\r SetBiosParameterBlock() returned ");
        FAT_PrintBootSectorError((uint8_t)err);
      /*
        print_str("\n\n\r **** BIOS PARAMTERS ****");
        print_str("\n\r bytesPerSec    = "); print_dec(bpb.bytesPerSec);
        print_str("\n\r secPerClus = "); print_dec(bpb.secPerClus);
        print_str("\n\r rsvdSecCnt = "); print_dec(bpb.rsvdSecCnt);
        print_str("\n\r numOfFats = "); print_dec(bpb.numOfFats);
        print_str("\n\r fatSize32 = "); print_dec(bpb.fatSize32);
        print_str("\n\r rootClus = "); print_dec(bpb.rootClus);

        print_str("\n\r bootSecAddr = "); print_dec(bpb.bootSecAddr);
        print_str("\n\r dataRegionFirstSector = "); print_dec(bpb.dataRegionFirstSector);
      */
        //uint32_t bootSectorLocation;
        //bootSectorLocation = fat_FindBootSector();
        //print_str("\n\r boot sector is at block number "); print_dec(bootSectorLocation);
        //initialize current working directory to the root directory
        FatDir cwd = {"/","","/","", bpb.rootClus};
        //uint16_t err = 0;
        //FAT_PrintDirectory(&cwd, LONG_NAME|HIDDEN);
        //FAT_PrintError(err);

        //print_str("\n\rGetFatRootClus() = "); print_dec(GetFatRootClus());
        
        int quit = 0;   
        int len = 64;
        char str[len];
        char temp;
        int p;
        int s;

        char c[len];
        char a[len];
        
        uint8_t flag = 0;

        int noa = 0; //num of arguements
        int loc[len];
        int i = 0;
        //uint16_t err = 0;


        do
        {
            flag = 0;
            err = 0;

            for(int k = 0; k < len; k++) str[k] = '\0';
            for(int k = 0; k < len; k++)   c[k] = '\0';
            for(int k = 0; k < len; k++)   a[k] = '\0';
            
            for(int k = 0; k < len; k++) loc[k] = 0;            
            noa = 0; //num of arguements
        
            print_str("\n\r");print_str(cwd.longParentPath);print_str(cwd.longName);print_str(" > ");
            temp = USART_Receive();
  
            i = 0;
            while(temp != '\r')
            {
                
                if(temp == 127)  // compensate for lack of backspace on MAC
                {
                    print_str("\b \b");
                    if(i > 0) i--;
                }

                else 
                { 
                    USART_Transmit(temp);
                    str[i] = temp;
                    i++;
                }

                temp = USART_Receive();
                if(i >= len) break;
            }


            for(p = 0; p < i; p++)
            {
                c[p] = str[p];
                if( c[p] == ' ' ) { c[p] = '\0'; break; }
                if( c[p] == '\0') break;
            }

            for(s = 0; s < i - p; s++)
            {
                a[s] = str[s+p+1];
                if( a[s] == '\0') break;
            }

            if (i < len) 
            {
                if(!strcmp(c,"cd"))
                {   
                    err = FAT_SetDirectory(&cwd, a, &bpb);
                    if (err != SUCCESS) FAT_PrintError(err);
                }
                
                else if (!strcmp(c,"ls"))
                {

                    loc[noa] = 0;
                    noa++;

                    for(int t = 0; t < len; t++)
                    {
                        if( a[t] == '\0') { break; }

                        if( a[t] ==  ' ')
                        {
                            a[t] = '\0';
                            loc[noa] = t+1;
                            noa++;
                        }
                    }

                    for(int t = 0; t < noa; t++)
                    {
                             if (strcmp(&a[loc[t]],"/LN") == 0 ) flag |= LONG_NAME;
                        else if (strcmp(&a[loc[t]],"/SN") == 0 ) flag |= SHORT_NAME;
                        else if (strcmp(&a[loc[t]],"/A")  == 0 ) flag |= ALL;
                        else if (strcmp(&a[loc[t]],"/H")  == 0 ) flag |= HIDDEN;
                        else if (strcmp(&a[loc[t]],"/C")  == 0 ) flag |= CREATION;
                        else if (strcmp(&a[loc[t]],"/LA") == 0 ) flag |= LAST_ACCESS;
                        else if (strcmp(&a[loc[t]],"/LM") == 0 ) flag |= LAST_MODIFIED;
                        else if (strcmp(&a[loc[t]],"/FS") == 0 ) flag |= FILE_SIZE;
                        else if (strcmp(&a[loc[t]],"/T")  == 0 ) flag |= TYPE;
                        else { print_str("\n\rInvalid Argument"); break; }
                    }
                    if((flag&SHORT_NAME) != SHORT_NAME) { flag |= LONG_NAME; } //long name is default
                    err = FAT_PrintDirectory(&cwd, flag, &bpb);
                    if (err != END_OF_DIRECTORY) FAT_PrintError(err);
                }
                
                else if (!strcmp(c,"open")) 
                { 
                    err = FAT_PrintFile(&cwd,a,&bpb);
                    if (err != END_OF_FILE) FAT_PrintError(err);
                    
                }
                else if (!strcmp(c,"cwd"))
                {
                    print_str("\n\rshortName = "); print_str(cwd.shortName);
                    print_str("\n\rshortParentPath = "); print_str(cwd.shortParentPath);
                    print_str("\n\rlongName = "); print_str(cwd.longName);
                    print_str("\n\rlongParentPath = "); print_str(cwd.longParentPath);
                    print_str("\n\rFATFirstCluster = "); print_dec(cwd.FATFirstCluster);
                }
                else if (c[0] == 'q') {  print_str("\n\rquit\n\r"); quit = 1; }
                
                else  print_str("\n\rInvalid command\n\r");
            }
        
            print_str("\n\r");
        
            for (int k = 0; k < 10; k++) UDR0; // ensure USART Data Register is cleared of any remaining garbage bytes.
        
        }while (quit == 0);      
    

        uint32_t startBlockNumber;
        uint32_t numberOfBlocks;
        uint8_t answer;
        do{
            do{
                print_str("\n\n\n\rEnter Start Block\n\r");
                startBlockNumber = enterBlockNumber();
                print_str("\n\rHow many blocks do you want to print?\n\r");
                numberOfBlocks = enterBlockNumber();
                print_str("\n\rYou have selected to print "); print_dec(numberOfBlocks);
                print_str(" blocks beginning at block number "); print_dec(startBlockNumber);
                print_str("\n\rIs this correct? (y/n)");
                answer = USART_Receive();
                USART_Transmit(answer);
                print_str("\n\r");
            }while(answer != 'y');

            // READ amd PRINT post write
            if (ctv.type == SDHC) // SDHC is block addressable
                err = SD_PrintMultipleBlocks(startBlockNumber,numberOfBlocks);
            else // SDSC byte addressable
                err = SD_PrintMultipleBlocks(
                                startBlockNumber * BLOCK_LEN, 
                                numberOfBlocks);
            
            if(err != READ_SUCCESS)
            { 
                print_str("\n\r >> SD_PrintMultipleBlocks() returned ");
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

            print_str("\n\rPress 'q' to quit: ");
            answer = USART_Receive();
            USART_Transmit(answer);
        }while(answer != 'q');
    }
    
    // Something to do after SD card testing has completed.
    while(1)
        USART_Transmit(USART_Receive());
    
    return 0;
}




// local function for taking user input to specify a block
// number. If nothing is entered then the block number is 0.
uint32_t enterBlockNumber()
{
    uint8_t x;
    uint8_t c;
    uint32_t blockNumber = 0;

    c = USART_Receive();
    
    while(c!='\r')
    {
        if( (c >= '0') && (c <= '9') )
        {
            x = c - '0';
            blockNumber = blockNumber * 10;
            blockNumber += x;
        }
        else if ( c == 127) // backspace
        {
            print_str("\b ");
            blockNumber = blockNumber/10;
        }

        print_str("\r");
        print_dec(blockNumber);
        
        if(blockNumber >= 4194304)
        {
            blockNumber = 0;
            print_str("\n\rblock number is too large.");
            print_str("\n\rEnter value < 4194304\n\r");
        }
        c = USART_Receive();
    }
    return blockNumber;
}