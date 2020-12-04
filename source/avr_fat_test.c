/*
***********************************************************************************************************************
*                                                  TEST for AVR-FAT MODULE
*
*                      Contains main(). Used to test the functionality of the the AVR-FAT module.
* 
* File   : AVR_FAT_TEST.C
* Author : Joshua Fain
* Target : ATMega1280
*
*
* DESCRIPTION: 
* This file currently implements a simple command-line like interface to navigate a FAT volume using the functions 
* available in the AVR-FAT module (i.e. FAT.C / FAT.H). This particular 'test' implementation uses the AVR-SD Card 
* module as a physical disk driver to access the raw data contents of a FAT32-formatted SD card. This driver is 
* included in the repo, but the AVR-SD Card module should not be considered part of this AVR-FAT module.
*
*
* COMMANDS:
*  (1) cd <DIR>                       :    Change directory to the directory specified by <DIR>.
*  (2) ls <FILTER_1> ... <FILTER_n>   :    List directory contents based on <FILTER>'s.
*  (3) open <FILE>                    :    Print contents of <FILE> to a screen.
*  (4) pwd                            :    Print the current working directory to screen. This actually prints the
*                                          values of the FatDir instances members at the time it is called.
*
* NOTES: 
* (1) This only has READ capabilities. No file or directories can be created with the module, and nothing in the FAT
*     volume can be modified.
* (2) The 'cd' command can only change the directory to a child or the parent of the current directory.
* (3) 'open' will only work for files that are in the current directory. 
* (4) Pass ".." (without quotes) as the argument to cd to change to the parent directory.
* (5) Quotation marks should NOT be used in specifying a directory or file, even if spaces are in the name. 
* (6) Directory and file arguments are all case sensitive.
* (7) If no argument is given to 'ls' then the long name of non-hidden entries is printed to the screen.
* (8) The following options are available as "filters" for the 'ls' command. Pass any combination of these:
*      /LN : Print long name of each entry if it exists, else its short name is printed.
*      /SN : Print short name of the entry.
*      /H  : Print hidden entries.
*      /T  : Print the entry type (file or directory). 
*      /FS : Print the file size of the entry. Currently rounded down to the nearest KB.
*      /C  : Print entries creation date and time.
*      /LM : Print last modified date and time.
*      /LA : Print last access date.
*      /A  :  = /C /LM /LA
*
* (9) To exit the command line portion of this testing module, enter 'q'.  After exiting, access to the raw data of the
*     physical disk is provided using the functionality of the AVR-SD Card module. Prompts are provided there as 
*     instructions, but again, this functionality is not considered part of the AVR-FAT but only specific to how it is
*     implemented here. 
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


#include <string.h>
#include <avr/io.h>
#include "../includes/usart.h"
#include "../includes/spi.h"
#include "../includes/prints.h"
#include "../includes/sd_spi_base.h"
#include "../includes/sd_spi_data_access.h"
#include "../includes/fat.h"
#include "../includes/fattodisk_interface.h"


// local function used for raw data access sd card access.
// only used by this test file. In the SD Card Raw Data Access 
// section. Not the AVR-FAT demo section. 
uint32_t enterBlockNumber();



// ********************************************************************
//                                 MAIN()

int main(void)
{
  USART_Init();
  SPI_MasterInit();


  // ***********************************************************************
  //                            SD Card Initialization 
  
  // ctv is a type used specifically by some sd card-specific routines. This
  // is not explicitely used in the FAT module, but the SD card initializing
  // routine requires it to be passed as an argument. The results will
  // specify whether the card is type SDHC or SDSC, which is used by the
  // SD Card routines to determines how it is shoudl be addressed.
  CTV ctv;

  uint32_t initResponse;

  // Attempt, up to 10 times, to initialize the SD card.
  for (int i = 0; i < 10; i++)
    {
      print_str ("\n\n\r SD Card Initialization Attempt # "); print_dec (i);
      initResponse = sd_spi_mode_init (&ctv);
      if (((initResponse & 0xFF) != 0) && ((initResponse & 0xFFF00) != 0))
        {    
          print_str ("\n\n\r FAILED INITIALIZING SD CARD");
          print_str ("\n\r Initialization Error Response: "); 
          sd_spi_print_init_error (initResponse);
          print_str ("\n\r R1 Response: "); sd_spi_print_r1 (initResponse);
        }
      else
        {   
          print_str ("\n\r SUCCESSFULLY INITIALIZED SD CARD");
          break;
        }
    }

  //                           END SD Card Initialization
  // ************************************************************************




  // if SD card initialization is successful
  if (initResponse == 0)
    {          
      // ********************************************************************
      //                       FAT "Command-Line" Section 
      
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
      char    inputChar;
      char    cmdStr[cmdLineLenMax];
      char    argStr[cmdLineLenMax];
      uint8_t argCnt;
      uint8_t lastArgFlag = 0;
      char    *spacePtr;
      uint8_t numOfChars = 0;
      uint8_t filter = 0; // used with FAT_PrintDirectory()
      uint8_t quit = 0;

      print_str("\n\n\n\r");
      do
        {
          // reset strings and variables
          for (int k = 0; k < cmdLineLenMax; k++) inputStr[k] = '\0';
          for (int k = 0; k < cmdLineLenMax; k++) cmdStr[k]   = '\0';
          for (int k = 0; k < cmdLineLenMax; k++) argStr[k]   = '\0';
          filter = 0;
          err = 0;
          numOfChars = 0;
          
          // print cmdStr prompt to screen
          print_str ("\n\r"); print_str (cwd.longParentPath);
          print_str (cwd.longName); print_str (" > ");
          
          // Enter commands / arguments
          inputChar = USART_Receive();
          while (inputChar != '\r')
            {
              // handle backspaces
              if (inputChar == 127)  
                {
                  print_str ("\b \b");
                  if (numOfChars > 0) numOfChars--;
                }

              // print last char entered
              else 
                { 
                  USART_Transmit (inputChar);
                  inputStr[numOfChars] = inputChar;
                  numOfChars++;
                }
              
              // get next char
              inputChar = USART_Receive();
              if (numOfChars >= cmdLineLenMax) break;
            }

          // Parse command / arguments into separate strings
          spacePtr = strchr (inputStr, ' ');
          if (*spacePtr != '\0')
            {
              *spacePtr = '\0';
              strcpy (argStr, spacePtr + 1);
            }
          strcpy (cmdStr, inputStr);

          // Execute command
          if (numOfChars < cmdLineLenMax) 
            {
              // Command: change directory
              if ( !strcmp (cmdStr, "cd"))
                {   
                  err = FAT_SetDirectory (&cwd, argStr, &bpb);
                  if (err != SUCCESS) FAT_PrintError (err);
                }
                
              // Command: list directory contents
              else if ( !strcmp(cmdStr, "ls"))
                {
                  lastArgFlag = 0;
                  argCnt = 0;
                  do
                    {
                      // find first occurance of ' ' in argStr.
                      spacePtr = strchr (argStr, ' ');
                      if (spacePtr == NULL) lastArgFlag = 1;
                      *spacePtr = '\0';
                      
                           if (strcmp ( argStr, "/LN") == 0) filter |= LONG_NAME;
                      else if (strcmp ( argStr, "/SN") == 0) filter |= SHORT_NAME;
                      else if (strcmp ( argStr, "/A" ) == 0) filter |= ALL;
                      else if (strcmp ( argStr, "/H" ) == 0) filter |= HIDDEN;
                      else if (strcmp ( argStr, "/C" ) == 0) filter |= CREATION;
                      else if (strcmp ( argStr, "/LA") == 0) filter |= LAST_ACCESS;
                      else if (strcmp ( argStr, "/LM") == 0) filter |= LAST_MODIFIED;
                      else if (strcmp ( argStr, "/FS") == 0) filter |= FILE_SIZE;
                      else if (strcmp ( argStr, "/T" ) == 0) filter |= TYPE;
                     
                      if (lastArgFlag) break;
                      strcpy (argStr, spacePtr + 1);
                    }
                  while (argCnt++ < 10);
  
                  // Send LONG_NAME as default argument.
                  if ((filter & SHORT_NAME) != SHORT_NAME) filter |= LONG_NAME; 
                  
                  err = FAT_PrintDirectory (&cwd, filter, &bpb);
                  if (err != END_OF_DIRECTORY) FAT_PrintError (err);
                }
              
              // Command: open file and print it to screen
              else if (!strcmp(cmdStr, "open")) 
                { 
                  err = FAT_PrintFile (&cwd, argStr, &bpb);
                  if (err != END_OF_FILE) FAT_PrintError (err);
                }

              // Command: print FatDir members
              else if (!strcmp(cmdStr, "pwd"))
                {
                  print_str ("\n\rshortName = "); print_str (cwd.shortName);
                  print_str ("\n\rshortParentPath = "); print_str (cwd.shortParentPath);
                  print_str ("\n\rlongName = "); print_str (cwd.longName);
                  print_str ("\n\rlongParentPath = "); print_str (cwd.longParentPath);
                  print_str ("\n\rFATFirstCluster = "); print_dec (cwd.FATFirstCluster);
                }
              
              // quit the cmdStr line interface.
              else if (cmdStr[0] == 'q') { print_str ("\n\rquit\n\r"); quit = 1; }
              
              else  print_str ("\n\rInvalid command\n\r");
            }
          print_str ("\n\r");

          // ensure USART Data Register is cleared of any remaining garbage bytes.
          for (int k = 0; k < 10; k++) UDR0; 
        }
      while (quit == 0);   

      //                      END FAT "Command-Line" Section
      // **********************************************************************



      // **********************************************************************
      //                         SD Card raw data access

      uint32_t startBlockNum;
      uint32_t numOfBlocks;
      uint8_t  answer;
      uint16_t sdErr = 0;
      do
        {
          do
            {
              print_str ("\n\n\n\rEnter Start Block\n\r");
              startBlockNum = enterBlockNumber();
              print_str ("\n\rHow many blocks do you want to print?\n\r");
              numOfBlocks = enterBlockNumber();
              print_str ("\n\rYou have selected to print "); 
              print_dec(numOfBlocks);
              print_str (" blocks beginning at block number "); 
              print_dec(startBlockNum);
              print_str ("\n\rIs this correct? (y/n)");
              answer = USART_Receive();
              USART_Transmit (answer);
              print_str ("\n\r");
            }
          while (answer != 'y');

          // Print blocks

          // SDHC is block addressable
          if (ctv.type == SDHC) sdErr = sd_spi_print_multiple_blocks (startBlockNum, numOfBlocks);
          // SDSC is byte addressable
          else sdErr = sd_spi_print_multiple_blocks (startBlockNum * BLOCK_LEN, numOfBlocks);
          
          if (sdErr != READ_SUCCESS)
            { 
              print_str ("\n\r >> sd_spi_print_multiple_blocks () returned ");
              if (sdErr & R1_ERROR)
                {
                  print_str ("R1 error: ");
                  sd_spi_print_r1 (sdErr);
                }
              else 
                { 
                  print_str (" error "); 
                  sd_spi_print_read_error (sdErr);
                }
            }
          print_str ("\n\rPress 'q' to quit: ");
          answer = USART_Receive();
          USART_Transmit (answer);
        }
      while (answer != 'q');

      //                    END SD Card raw data access                         
      // **********************************************************************
    }   
  

  // Something else to do. Print entered chars to screen.
  while(1)
      USART_Transmit (USART_Receive());
  
  return 0;
}
//                               END MAIN()                      
// **********************************************************************



                       
// **********************************************************************
//                             LOCAL FUNCTIONS
// **********************************************************************



// local function for taking user inputChar to specify a block
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