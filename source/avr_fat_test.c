/*
*******************************************************************************
*                                 TEST for AVR-FAT MODULE
*
*   Contains main(). Used to test the functionality of the the AVR-FAT module.
* 
* File   : AVR_FAT_TEST.C
* Author : Joshua Fain
* Target : ATMega1280
* License : MIT LICENSE
* Copyright (c) 2020 Joshua Fain
*
* DESCRIPTION: 
* Implements a simple command line-like interface to navigate and read a FAT 
* volume. This particular 'test' implementation uses the AVR-SD Card module as
* a physical disk driver to access the raw data contents of a FAT32-formatted 
* SD card. This SD card driver is included in the repo, but is not considered 
* part of the AVR-FAT module.
*
*
* COMMANDS:
*  (1) cd <DIR>      : Change directory to the directory specified by <DIR>.
*  (2) ls FILTERs>   : List directory contents based on specified <FILTERs>.
*  (3) open <FILE>   : Print contents of <FILE> to a screen.
*  (4) pwd           : Print the current working directory to screen. This 
*                      actually prints the values of the FatDir instances 
*                      members at the time it is called.
*
* NOTES: 
* (1) The module only has READ capabilities.
* (2) 'cd' cmd can only change the dir to a child/parent of the cwd.
* (3) 'open' will only work for files that are in the cwd. 
* (4) Pass ".." (without quotes) as the argument to 'cd' for parent dir.
* (5) Quotation marks should NOT be used in specifying a directory or file. 
* (6) Directory and file arguments are all case sensitive.
* (7) If no arg is given to 'ls', then the long name of entries are printed. 
* (8) The following options are available as "filters" for the 'ls' command. 
*     Pass any combination of these:
*       /LN : Print long name of each entry if it exists.
*       /SN : Print short name of each entry.
*       /H  : Print hidden entries.
*       /T  : Print the entry type (file or dir). 
*       /FS : Print the file size of the entry.
*       /C  : Print entry creation date and time.
*       /LM : Print last modified date and time.
*       /LA : Print last access date.
*       /A  :  = /C /LM /LA
*
* (9) Enter 'q' to exit the command line portion of this testing module. After
*     exiting, access to the raw data of the physical disk is provided using 
*     the functionality of the AVR-SD Card module. Prompts are provided there 
*     as instructions. This raw data functionality is not considered part of 
*     the AVR-FAT module.
*******************************************************************************
*/


#include <string.h>
#include <avr/io.h>
#include "usart0.h"
#include "spi.h"
#include "prints.h"
#include "sd_spi_base.h"
#include "sd_spi_rwe.h"
#include "fat_bpb.h"
#include "fat.h"
#include "fat_to_disk.h"


void
fat_print_fat_entry_members (FatEntry * entry);

void
fat_print_fat_directory_members (FatDir * dir);


// local function used for raw data access sd card access.
// only used by this test file. In the SD Card Raw Data Access 
// section. Not the AVR-FAT demo section. 
uint32_t enterBlockNumber();



// ****************************************************************************
//                                 MAIN()

int main(void)
{
  usart_init();
  spi_masterInit();


  // **************************************************************************
  //                            SD Card Initialization 
  
  // Initialize SD card and set ctv instance using the intialization routine.

  CTV ctv;

  uint32_t initResponse;

  // Attempt, up to 5 times, to initialize the SD card.
  for (uint8_t i = 0; i < 5; i++)
    {
      print_str ("\n\n\r SD Card Initialization Attempt # "); print_dec(i);
      initResponse = sd_spiModeInit (&ctv);

      if (((initResponse & 0xFF) != 0) && ((initResponse & 0xFFF00) != 0))
        {    
          print_str ("\n\n\r FAILED INITIALIZING SD CARD");
          print_str ("\n\r Initialization Error Response: "); 
          sd_printInitError (initResponse);
          print_str ("\n\r R1 Response: "); sd_printR1 (initResponse);
        }
      else
        {   
          print_str ("\n\r SUCCESSFULLY INITIALIZED SD CARD");
          break;
        }
    }
  //                           END SD Card Initialization
  // **************************************************************************




  // if SD card initialization is successful
  if (initResponse == 0)
    {          
      // **********************************************************************
      //                       FAT "Command-Line" Section 
      
      // error variable for errors returned by any of the FAT functions.
      uint8_t err; 
      

      // Create and set Bios Parameter Block instance. These members will 
      // assist in pointing to region locations in the FAT volume. This only 
      // needs to be called set once.
      BPB bpb;
      err = fat_setBPB (&bpb);
      if (err != BOOT_SECTOR_VALID)
        {
          print_str("\n\r fat_setBPB() returned ");
          fat_printBootSectorError(err);
        }
    

      // Holds parameters/state of an entry in a FAT directory.
      // Initialize by passing it to fat_initEntry();
      FatEntry ent;
      fat_initEntry(&ent, &bpb);


      // Holds parameters to reference a FAT directory.
      // Must be initialized to the root directory.
      FatDir cwd;
      fat_setDirToRoot(&cwd, &bpb);
      

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
      uint8_t filter = 0; // used with fat_printDir()
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
          print_str("\n\r"); 
          print_str(cwd.longParentPath);
          print_str(cwd.longName); 
          print_str (" > ");
          
          // Enter commands / arguments
          inputChar = usart_receive();
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
                  usart_transmit (inputChar);
                  inputStr[numOfChars] = inputChar;
                  numOfChars++;
                }
              
              // get next char
              inputChar = usart_receive();
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
                  err = fat_setDir (&cwd, argStr, &bpb);
                  if (err != SUCCESS) fat_printError (err);
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
                      
                      if (strcmp (argStr, "/LN") == 0) 
                            filter |= LONG_NAME;
                      else if (strcmp (argStr, "/SN") == 0) 
                            filter |= SHORT_NAME;
                      else if (strcmp (argStr, "/A" ) == 0) 
                            filter |= ALL;
                      else if (strcmp (argStr, "/H" ) == 0) 
                            filter |= HIDDEN;
                      else if (strcmp (argStr, "/C" ) == 0) 
                            filter |= CREATION;
                      else if (strcmp (argStr, "/LA") == 0) 
                            filter |= LAST_ACCESS;
                      else if (strcmp (argStr, "/LM") == 0) 
                            filter |= LAST_MODIFIED;
                      else if (strcmp (argStr, "/FS") == 0) 
                            filter |= FILE_SIZE;
                      else if (strcmp (argStr, "/T" ) == 0) 
                            filter |= TYPE;
                     
                      if (lastArgFlag) break;
                      strcpy (argStr, spacePtr + 1);
                    }
                  while (argCnt++ < 10);
  
                  // Send LONG_NAME as default argument.
                  if ((filter & SHORT_NAME) != SHORT_NAME) 
                    filter |= LONG_NAME;
                  
                  err = fat_printDir (&cwd, filter, &bpb);
                  if (err != END_OF_DIRECTORY) fat_printError (err);
                }
              
              // Command: open file and print it to screen
              else if (!strcmp(cmdStr, "open")) 
                { 
                  err = fat_printFile (&cwd, argStr, &bpb);
                  if (err != END_OF_FILE) fat_printError (err);
                }

              // Command: print FatDir members
              else if (!strcmp(cmdStr, "pwd"))
                {
                  print_str ("\n\rshortName = "); 
                  print_str (cwd.shortName);
                  print_str ("\n\rshortParentPath = "); 
                  print_str (cwd.shortParentPath);
                  print_str ("\n\rlongName = "); 
                  print_str (cwd.longName);
                  print_str ("\n\rlongParentPath = "); 
                  print_str (cwd.longParentPath);
                  print_str ("\n\rFATFirstCluster = "); 
                  print_dec (cwd.FATFirstCluster);
                }
              
              // quit the cmdStr line interface.
              else if (cmdStr[0] == 'q') 
                { 
                  print_str ("\n\rquit\n\r"); 
                  quit = 1; 
                }
              else  
                print_str ("\n\rInvalid command\n\r");
            }
          print_str ("\n\r");

          // ensure USART Data Register is cleared.
          for (int k = 0; k < 10; k++) UDR0; 
        }
      while (quit == 0);   

      //                      END FAT "Command-Line" Section
      // **********************************************************************
      


      // **********************************************************************
      //                         SD Card raw data access
/*
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
              answer = usart_receive();
              usart_transmit (answer);
              print_str ("\n\r");
            }
          while (answer != 'y');

          // Print blocks

          // SDHC is block addressable
          if (ctv.type == SDHC) 
            sdErr = sd_printMultipleBlocks (
                      startBlockNum, numOfBlocks);
          // SDSC is byte addressable
          else 
            sdErr = sd_printMultipleBlocks (
                      startBlockNum * BLOCK_LEN, numOfBlocks);
          
          if (sdErr != READ_SUCCESS)
            { 
              print_str ("\n\r >> sd_printMultipleBlocks() returned ");
              if (sdErr & R1_ERROR)
                {
                  print_str ("R1 error: ");
                  sd_printR1 (sdErr);
                }
              else 
                { 
                  print_str (" error "); 
                  sd_printReadError (sdErr);
                }
            }
          print_str ("\n\rPress 'q' to quit: ");
          answer = usart_receive();
          usart_transmit (answer);
        }
      while (answer != 'q');

      //                    END SD Card raw data access                         
      // **********************************************************************
  */
    }   
  
  // Something else to do. Print entered chars to screen.
  while(1)
      usart_transmit (usart_receive());
  
  return 0;
}
//                               END MAIN()                      
// ****************************************************************************



                       
// ****************************************************************************
//                             LOCAL FUNCTIONS
// ****************************************************************************



// local function for taking user inputChar to specify a block
// number. If nothing is entered then the block number is 0.
uint32_t enterBlockNumber()
{
  uint8_t x;
  uint8_t c;
  uint32_t blockNumber = 0;

  c = usart_receive();
  
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
      c = usart_receive();
    }
  return blockNumber;
}


void
fat_print_fat_entry_members(FatEntry * entry)
{
  print_str("\n\rentry->longName                   = ");print_str(entry->longName);
  print_str("\n\rentry->shortName                  = ");print_str(entry->shortName);
  print_str("\n\rentry->snEnt                      = { ");
  for (uint8_t i = 0; i < 31; i++)
    { 
      print_hex(entry->snEnt[i]);
      print_str(", ");
    }
  print_hex(entry->snEnt[31]);
  print_str("} ");
  print_str("\n\rentry->snEntClusIndx              = ");print_dec(entry->snEntClusIndx);
  print_str("\n\rentry->snEntSecNumInClus          = ");print_dec(entry->snEntSecNumInClus);
  print_str("\n\rentry->entPos                     = ");print_dec(entry->entPos);
  print_str("\n\rentry->lnFlags                    = ");print_dec(entry->lnFlags);
  print_str("\n\rentry->snPosCurrSec               = ");print_dec(entry->snPosCurrSec);
  print_str("\n\rentry->snPosNextSec               = ");print_dec(entry->snPosNextSec);
}


void
fat_print_fat_directory_members(FatDir * dir)
{
  print_str ("\n\rshortName = "); print_str (dir->shortName);
  print_str ("\n\rshortParentPath = "); print_str (dir->shortParentPath);
  print_str ("\n\rlongName = "); print_str (dir->longName);
  print_str ("\n\rlongParentPath = "); print_str (dir->longParentPath);
  print_str ("\n\rFATFirstCluster = "); print_dec (dir->FATFirstCluster);
}