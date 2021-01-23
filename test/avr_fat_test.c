/*
 * 
 *                                 TEST for AVR-FAT MODULE
 *
 * File   : AVR_FAT_TEST.C
 * Author : Joshua Fain
 * Target : ATMega1280
 * License : MIT LICENSE
 * Copyright (c) 2020 Joshua Fain
 * 
 * 
 * Contains main(). Used to test the functionality of the the AVR-FAT module.
 * 
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
 *  (2) ls <FIELDS>   : List directory contents based on specified <FILTERs>.
 *  (3) open <FILE>   : Print contents of <FILE> to a screen.
 *  (4) pwd           : Print the current working directory to screen. This 
 *                      actually prints the values of the FatDir instance's 
 *                      members at the time it is called.
 * 
 * DEFS: 
 * (1) cwd = current working directory
 * 
 * NOTES: 
 * (1)  The module only has READ capabilities.
 * (2)  'cd' cmd can be used to reset the cwd to the ROOT directory or change
 *      it to a CHILD or the PARENT of the current directory pointed to by cwd.
 * (3)  'open' will only work for files that are in the cwd directory. 
 * (4)  Pass ".." (without quotes) as the argument to 'cd' to change cwd to
 *      point to its parent directory.
 * (5)  Pass "~" (without quotes) as the argument to 'cd' to reset cwd to the
 *      ROOT directory.
 * (6)  Quotation marks should NOT be used in specifying a directory or file.
 * (7)  Directory and file arguments are all case sensitive.
 * (8)  If no arg is given to 'ls', then the long name of entries are printed. 
 * (9)  The following options are available as "fields" for the 'ls' command.
 *      Pass any combination of these:
 *       /LN : Print long name of each entry if it exists.
 *       /SN : Print short name of each entry.
 *       /H  : Print hidden entries.
 *       /T  : Print the entry type (file or dir). 
 *       /FS : Print the file size of the entry.
 *       /C  : Print entry creation date and time.
 *       /LM : Print last modified date and time.
 *       /LA : Print last access date.
 *       /A  : ALL - prints all entries and all fields.
 *
 * (10) NOT CURRENTLY FUNCTIONAL 
 *      Enter 'q' to exit the command line portion of this testing module.
 *      After exiting, access to the raw data of the physical disk is provided
 *      using the functionality of the AVR-SD Card module. Prompts are provided 
 *      there as instructions. This raw data access is not considered part of
 *      the AVR-FAT module. 
 * 
 */

#include <string.h>
#include <stdlib.h>
#include <avr/io.h>
#include "usart0.h"
#include "spi.h"
#include "prints.h"
#include "sd_spi_base.h"
#include "sd_spi_rwe.h"
#include "fat_bpb.h"
#include "fat.h"
#include "fat_to_disk.h"


// local function prototypes
void fat_print_fat_entry_members (FatEntry *entry);
uint32_t enterBlockNumber();


int main(void)
{
  // --------------------------------------------------------------------------
  //                                   USART, SPI, and SD CARD INITILIAIZATIONS
  
  usart_init();
  spi_masterInit();

  // SD card
  CTV *ctvPtr = malloc(sizeof(CTV));             // SD card type & version
  uint32_t sdInitResp;                           // SD card init error response


  // Attempt SD card init up to 5 times.
  for (uint8_t i = 0; i < 5; i++)
  {
    print_str ("\n\n\r >> SD Card Initialization Attempt "); 
    print_dec(i);
    sdInitResp = sd_spiModeInit (ctvPtr);        // init SD card into SPI mode

    if (sdInitResp != 0)
    {    
      print_str (": FAILED TO INITIALIZE SD CARD.");
      print_str (" Initialization Error Response: "); 
      sd_printInitError (sdInitResp);
      print_str (", R1 Response: "); 
      sd_printR1 (sdInitResp);
    }
    else
    {   
      print_str (": SD CARD INITIALIZATION SUCCESSFUL");
      break;
    }
  }
  //                                                        END INITILIAIZATION
  // --------------------------------------------------------------------------



  if (sdInitResp == 0)
  {          
    // ------------------------------------------------------------------------
    //                                                         FAT COMMAND-LINE
    
    // for returned errors
    uint8_t err;
    
    //
    // Create and set Bios Parameter Block instance. Members of this instance
    // are used to calculate where on the physical disk volume, the FAT 
    // sectors/blocks are located. This should only be set once here.
    //
    BPB *bpbPtr = malloc(sizeof(BPB));
    err = fat_setBPB (bpbPtr);
    if (err != BOOT_SECTOR_VALID)
    {
      print_str("\n\r fat_setBPB() returned ");
      fat_printBootSectorError(err);
    }
  
    //
    // Create and initialize a FatEntry instance. Members of this instance are
    // used for pointing to the location of a FAT entry within its directory.
    // The instance should be initialized with fat_initEntry() before using.
    //
    FatEntry *entPtr = malloc(sizeof(FatEntry));
    fat_initEntry (entPtr, bpbPtr);
   
    //
    // Create and set a FatDir instance. Members of this instance are used for
    // holding parameters of a FAT directory. This instance can be treated as
    // the current working directory. The instance should be initialized to 
    // the root directory with fat_setDirToRoot() prior to using anywhere else.
    //
    FatDir *cwdPtr = malloc(sizeof(FatDir));
    fat_setDirToRoot (cwdPtr, bpbPtr);
    
    // vars to implement cmd-line.
    const uint8_t cmdLineLenMax = 100;           // max char len of cmd/arg
    char    inputStr[cmdLineLenMax];             // hold cmd/arg str
    char    inputChar;                           // for input chars of cmd/arg
    char    cmdStr[cmdLineLenMax];               // separate cmd from inputStr
    char    argStr[cmdLineLenMax];               // separate arg from inputStr
    uint8_t argCnt;                              // count number of args
    uint8_t lastArgFlag = 0;                     // indicates last arg
    char    *spacePtr;                  // for parsing inputStr into cmd & args
    uint8_t numOfChars = 0;                      // number of chars in inputStr
    uint8_t fieldFlags = 0;     // specify which fields to print with 'ls' cmd.
    uint8_t quit = 0;                            // flag used to exit cmd-line       

    print_str("\n\n\n\r");
    do
    {
      // reset strings and vars
      for (int k = 0; k < cmdLineLenMax; k++) 
      {
        inputStr[k] = '\0';
        cmdStr[k]   = '\0';
        argStr[k]   = '\0';
      }
      fieldFlags = 0;
      err = 0;
      numOfChars = 0;
      
      // print cmd prompt to screen with cwd
      print_str("\n\r"); 
      print_str(cwdPtr->lnPathStr);
      print_str(cwdPtr->lnStr); 
      print_str (" > ");
      
      // ---------------------------------- Get and Parse Command and Arguments
      inputChar = usart_receive();
      while (inputChar != '\r')
      {
        //
        // Handle Backspace for mac. 
        // This section may need to be adjusted depending on what is being used
        // to interact with the usart in order to send chars to the terminal.
        // 
        // On my mac the backspace button is ascii 127, which is delete. 
        // True backspace is ascii 8 ('\b'). This section will handle this
        // by printing a backspace char, '\b', then printing a space char, ' ',
        // and then printing another '\b'. These steps are typically how I 
        // expect the backspace button to operate, which is backspace and clear
        // the character. 
        // 
        if (inputChar == 127)  
        {
          print_str ("\b \b");
          if (numOfChars > 0) 
            numOfChars--;
        }
        // if no backspace then print the last char entered
        else 
        { 
          usart_transmit (inputChar);
          inputStr[numOfChars] = inputChar;
          numOfChars++;
        }
      
        // get next char
        inputChar = usart_receive();
        if (numOfChars >= cmdLineLenMax) 
          break;
      }

      // Parse command / arguments into separate strings
      spacePtr = strchr (inputStr, ' ');
      if (*spacePtr != '\0')
      {
        *spacePtr = '\0';
        strcpy (argStr, spacePtr + 1);
      }
      strcpy (cmdStr, inputStr);

      // ------------------------------------------------------ Execute Command
      if (numOfChars < cmdLineLenMax) 
      {
        // ------------------------------- Command: "cd" (change directory)
        if ( !strcmp (cmdStr, "cd"))
        {   
          err = fat_setDir (cwdPtr, argStr, bpbPtr);
          if (err != SUCCESS) 
            fat_printError (err);
        }
        // ------------------------------- Command: "ls" (list dir contents)
        else if ( !strcmp(cmdStr, "ls"))
        {
          lastArgFlag = 0;
          argCnt = 0;
          do
          {
            // find first occurance of ' ' in argStr.
            spacePtr = strchr (argStr, ' ');
            if (spacePtr == NULL) 
              lastArgFlag = 1;

            *spacePtr = '\0';
      
            if (strcmp (argStr, "/LN") == 0) 
                  fieldFlags |= LONG_NAME;
            else if (strcmp (argStr, "/SN") == 0) 
                  fieldFlags |= SHORT_NAME;
            else if (strcmp (argStr, "/A" ) == 0) 
                  fieldFlags |= ALL;
            else if (strcmp (argStr, "/H" ) == 0) 
                  fieldFlags |= HIDDEN;
            else if (strcmp (argStr, "/C" ) == 0) 
                  fieldFlags |= CREATION;
            else if (strcmp (argStr, "/LA") == 0) 
                  fieldFlags |= LAST_ACCESS;
            else if (strcmp (argStr, "/LM") == 0) 
                  fieldFlags |= LAST_MODIFIED;
            else if (strcmp (argStr, "/FS") == 0) 
                  fieldFlags |= FILE_SIZE;
            else if (strcmp (argStr, "/T" ) == 0) 
                  fieldFlags |= TYPE;
            
            if (lastArgFlag) break;
            strcpy (argStr, spacePtr + 1);
          }
          while (argCnt++ < 10);

          // Send LONG_NAME as default argument.
          if ((fieldFlags & SHORT_NAME) != SHORT_NAME) 
            fieldFlags |= LONG_NAME;
          
          // Print column headings
          print_str("\n\n\r");
          if (CREATION & fieldFlags) 
            print_str(" CREATION DATE & TIME,");
          if (LAST_ACCESS & fieldFlags) 
            print_str(" LAST ACCESS DATE,");
          if (LAST_MODIFIED & fieldFlags) 
            print_str(" LAST MODIFIED DATE & TIME,");
          if (FILE_SIZE & fieldFlags) 
            print_str(" SIZE (Bytes),");
          if (TYPE & fieldFlags) 
            print_str(" TYPE,");

          print_str(" NAME");
          print_str("\n\r");

          err = fat_printDir (cwdPtr, fieldFlags, bpbPtr);
          if (err != END_OF_DIRECTORY) 
            fat_printError (err);
        }

        // ----------------------------- Command: "open" (print file to screen)
        else if (!strcmp(cmdStr, "open")) 
        { 
          err = fat_printFile (cwdPtr, argStr, bpbPtr);
          if (err != END_OF_FILE) 
            fat_printError (err);
        }

        // --------------------------- Command: "pwd" (print working directory)
        //
        // This will print the value of each FatDir member.
        //
        else if (!strcmp(cmdStr, "pwd"))
        {
          print_str ("\n\rsnStr = "); 
          print_str (cwdPtr->snStr);
          print_str ("\n\rsnPathStr = "); 
          print_str (cwdPtr->snPathStr);
          print_str ("\n\rlnStr = "); 
          print_str (cwdPtr->lnStr);
          print_str ("\n\rlnPathStr = "); 
          print_str (cwdPtr->lnPathStr);
          print_str ("\n\rfstClusIndx = "); 
          print_dec (cwdPtr->fstClusIndx);
        }

        // --------------------------------------- Command: "q" (exit cmd-line)
        else if (cmdStr[0] == 'q') 
        { 
          print_str ("\n\rquit\n\r"); 
          quit = 1; 
        }

        // --------------------------------------- Command: "Invalid Command"
        else  
          print_str ("\n\rInvalid command\n\r");
      }
      print_str ("\n\r");

      // ensure USART Data Register, URDn, is cleared.
      for (int k = 0; k < 10; k++) 
        UDR0; 
    }
    while (quit == 0);   

    //                                                    END COMMAND-LINE TEST
    // ------------------------------------------------------------------------
    


    // ------------------------------------------------------------------------
    //                                                       USER INPUT SECTION
    //
    // This section allows a user to interact with the function 
    // sd_printMultipleBlocks(). The user is asked which block number they 
    // would like to print first, and then how many blocks they would like to 
    // print. The sd_printMultipleBlocks() function is then called with these
    // parameters and the blocks specified by the user are printed.
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

    //                                                   END USER INPUT SECTION
    // ------------------------------------------------------------------------
    */
  }   
  
  // Something else to do. Print entered chars to screen.
  while(1)
    usart_transmit (usart_receive());
  
  return 0;
}


                       
/*
 ******************************************************************************
 *                             LOCAL FUNCTIONS
 ******************************************************************************
 */


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

/*
void
fat_print_fat_entry_members(FatEntry * entry)
{
  print_str("\n\rentry->lnStr             = ");print_str(entry->lnStr);
  print_str("\n\rentry->snStr             = ");print_str(entry->snStr);
  print_str("\n\rentry->snEnt             = { ");
  for (uint8_t i = 0; i < 31; i++)
  { 
    print_hex(entry->snEnt[i]);
    print_str(", ");
  }
  print_hex(entry->snEnt[31]);
  print_str("} ");
  print_str("\n\rentry->snEntClusIndx     = ");print_dec(entry->snEntClusIndx);
  print_str("\n\rentry->snEntSecNumInClus = ");print_dec(entry->snEntSecNumInClus);
  print_str("\n\rentry->entPos            = ");print_dec(entry->entPos);
  print_str("\n\rentry->lnFlags           = ");print_dec(entry->lnFlags);
  print_str("\n\rentry->snPosCurrSec      = ");print_dec(entry->snPosCurrSec);
  print_str("\n\rentry->snPosNextSec      = ");print_dec(entry->snPosNextSec);
}
*/
