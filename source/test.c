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


  // ******************* SD CARD INITILIAIZATION ****************************
  
  // ctv is a type used specifically by some sd card-specific routines. This
  // is not explicitely used in the FAT module, but the SD card initializing
  // routine requires it to be passed as an argument. The results will
  // specify whether the card is type SDHC or SDSC, which determines how it 
  // is addressed.
  CardTypeVersion ctv;

  uint32_t initResponse;

  // Attempt, up to 10 times, to initialize the SD card.
  for (int i = 0; i < 10; i++)
    {
      print_str ("\n\n\r SD Card Initialization Attempt # "); print_dec (i);
      initResponse = SD_InitializeSPImode (&ctv);
      if (((initResponse & 0xFF) != OUT_OF_IDLE)
            &&((initResponse & 0xFFF00) != INIT_SUCCESS))
        {    
          print_str ("\n\n\r FAILED INITIALIZING SD CARD");
          print_str ("\n\r Initialization Error Response: "); 
          SD_PrintInitError (initResponse);
          print_str ("\n\r R1 Response: "); SD_PrintR1 (initResponse);
        }
      else
        {   
          print_str ("\n\r SUCCESSFULLY INITIALIZED SD CARD");
          break;
        }
    }
  // END SD CARD INITIALIZATION
  // ************************************************************************



  if (initResponse == OUT_OF_IDLE)
    {          
      // error variable for errors returned by any of the FAT functions.
      uint8_t err; 
      
      // Create and set Bios Parameter Block instance. These members will 
      // assist in pointing to region locations in the FAT volume. This only 
      // needs to be called set once.
      BPB bpb;
      err = FAT_SetBiosParameterBlock (&bpb);
      if (err != BOOT_SECTOR_VALID)
        {
          print_str ("\n\r SetBiosParameterBlock() returned ");
          FAT_PrintBootSectorError (err);
        }
    
      // Create a Fat Directory instance. This will hold members which
      // specify a current working directory. This instance should first be
      // set to point to the root directory. Afterwards it can be operated
      // on by the other FAT functions.
      FatDir cwd;
      FAT_SetDirectoryToRoot(&cwd, &bpb);
      

      // This section implements a command-line like interface for navigating
      // the FAT volume, and printing files to a screen.
   
      uint8_t cmdLineLenMax = 100;
      char    inputStr[cmdLineLenMax];
      char    cmd[cmdLineLenMax];
      char    arg[cmdLineLenMax];
      uint8_t lastArgFlag = 0;
      char    *tempArg;
      uint8_t argCnt;
      char    input;
      char    *spacePtr;
      uint8_t numOfChars = 0;
      uint8_t flag = 0;
      uint8_t quit = 0;

      do
        {
          // reset strings and variables
          for (int k = 0; k < cmdLineLenMax; k++) inputStr[k] = '\0';
          for (int k = 0; k < cmdLineLenMax; k++) cmd[k] = '\0';
          for (int k = 0; k < cmdLineLenMax; k++) arg[k] = '\0';
          flag = 0;
          err = 0;
          numOfChars = 0;
          
          // print cmd prompt to screen
          print_str ("\n\r"); print_str (cwd.longParentPath);
          print_str (cwd.longName); print_str (" > ");
          
          // Enter commands / arguments
          input = USART_Receive();
          while (input != '\r')
            {
              // handle backspaces
              if (input == 127)  
                {
                  print_str ("\b \b");
                  if (numOfChars > 0) numOfChars--;
                }

              // print last char entered
              else 
                { 
                  USART_Transmit (input);
                  inputStr[numOfChars] = input;
                  numOfChars++;
                }
              
              // get next char
              input = USART_Receive();
              if (numOfChars >= cmdLineLenMax) break;
            }

          // Parse command / arguments into separate strings
          spacePtr = strchr (inputStr, ' ');
          if (*spacePtr != '\0')
            {
              *spacePtr = '\0';
              strcpy (arg, spacePtr + 1);
            }
          strcpy (cmd, inputStr);

          // Execute command
          if (numOfChars < cmdLineLenMax) 
            {
              // Command: change directory
              if ( !strcmp (cmd, "cd"))
                {   
                  err = FAT_SetDirectory (&cwd, arg, &bpb);
                  if (err != SUCCESS) FAT_PrintError (err);
                }
                
              // Command: list directory contents
              else if ( !strcmp(cmd, "ls"))
                {
                  lastArgFlag = 0;
                  argCnt = 0;
                  do
                    {
                      // find first occurance of ' ' in arg.
                      tempArg = strchr (arg, ' ');
                      if (tempArg == NULL) lastArgFlag = 1;
                      *tempArg = '\0';
                      
                           if (strcmp ( arg, "/LN") == 0) flag |= LONG_NAME;
                      else if (strcmp ( arg, "/SN") == 0) flag |= SHORT_NAME;
                      else if (strcmp ( arg, "/A" ) == 0) flag |= ALL;
                      else if (strcmp ( arg, "/H" ) == 0) flag |= HIDDEN;
                      else if (strcmp ( arg, "/C" ) == 0) flag |= CREATION;
                      else if (strcmp ( arg, "/LA") == 0) flag |= LAST_ACCESS;
                      else if (strcmp ( arg, "/LM") == 0) flag |= LAST_MODIFIED;
                      else if (strcmp ( arg, "/FS") == 0) flag |= FILE_SIZE;
                      else if (strcmp ( arg, "/T" ) == 0) flag |= TYPE;
                     
                      if (lastArgFlag) break;
                      strcpy (arg, tempArg + 1);
                    }
                  while (argCnt++ < 10);
  
                  // Send LONG_NAME as default argument.
                  if ((flag & SHORT_NAME) != SHORT_NAME) flag |= LONG_NAME; 
                  err = FAT_PrintDirectory (&cwd, flag, &bpb);
                  if (err != END_OF_DIRECTORY) FAT_PrintError (err);
                }
              
              // Command: open file and print it to screen
              else if (!strcmp(cmd, "open")) 
                { 
                  err = FAT_PrintFile (&cwd, arg, &bpb);
                  if (err != END_OF_FILE) FAT_PrintError (err);
                }

              // Command: print FatDir members
              else if (!strcmp(cmd, "cwd"))
                {
                  print_str ("\n\rshortName = "); print_str (cwd.shortName);
                  print_str ("\n\rshortParentPath = "); print_str (cwd.shortParentPath);
                  print_str ("\n\rlongName = "); print_str (cwd.longName);
                  print_str ("\n\rlongParentPath = "); print_str (cwd.longParentPath);
                  print_str ("\n\rFATFirstCluster = "); print_dec (cwd.FATFirstCluster);
                }
              
              // quit the cmd line interface.
              else if (cmd[0] == 'q') { print_str ("\n\rquit\n\r"); quit = 1; }
              
              else  print_str ("\n\rInvalid command\n\r");
            }
          print_str ("\n\r");

          // ensure USART Data Register is cleared of any remaining garbage bytes.
          for (int k = 0; k < 10; k++) UDR0; 
        }
      while (quit == 0);      
      // END command line interface.  





      uint32_t startBlockNumber;
      uint32_t numberOfBlocks;
      uint8_t answer;
      do
        {
            do
              {
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
              }
            while(answer != 'y');

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
        }
      while(answer != 'q');
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
  
  while (c!='\r')
    {
      if ((c >= '0') && (c <= '9'))
        {
          x = c - '0';
          blockNumber = blockNumber * 10;
          blockNumber += x;
        }
      else if (c == 127) // backspace
        {
          print_str ("\b ");
          blockNumber = blockNumber/10;
        }

      print_str ("\r");
      print_dec (blockNumber);
      
      if (blockNumber >= 4194304)
        {
          blockNumber = 0;
          print_str ("\n\rblock number is too large.");
          print_str ("\n\rEnter value < 4194304\n\r");
        }
      c = USART_Receive();
    }
  return blockNumber;
}