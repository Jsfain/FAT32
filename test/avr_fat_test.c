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
 * (1) 'cwd' = current working directory
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
 * (10) Enter 'q' to exit the command-line. After exiting, the explicit FAT
 *      functionality however, there is included a raw data access section
 *      if you do use the AVR-SD card module along with the AVR-FAT module.
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


// local function prototype
uint32_t enterNumber();


int main(void)
{
  // --------------------------------------------------------------------------
  //                                   USART, SPI, and SD CARD INITILIAIZATIONS
  
  usart_Init();
  spi_MasterInit();

  // SD card initialization
  CTV *ctvPtr = malloc(sizeof(CTV));             // SD card type & version
  uint32_t sdInitResp;                           // SD card init error response


  // Attempt SD card init up to 5 times.
  for (uint8_t i = 0; i < 5; i++)
  {
    print_Str("\n\n\r >> SD Card Initialization Attempt "); 
    print_Dec(i);
    sdInitResp = sd_InitModeSPI(ctvPtr);        // init SD card into SPI mode

    if (sdInitResp != 0)
    {    
      print_Str (": FAILED TO INITIALIZE SD CARD.");
      print_Str (" Initialization Error Response: "); 
      sd_PrintInitError (sdInitResp);
      print_Str (", R1 Response: "); 
      sd_PrintR1 (sdInitResp);
    }
    else
    {   
      print_Str (": SD CARD INITIALIZATION SUCCESSFUL");
      break;
    }
  }
  //                                                        END INITILIAIZATION
  // --------------------------------------------------------------------------



  if (sdInitResp == 0)
  {          
    // ------------------------------------------------------------------------
    //                                                         FAT COMMAND-LINE
    
    uint8_t err;                                 // for returned errors
    
    //
    // Create and set Bios Parameter Block instance. Members of this instance
    // are used to calculate where on the physical disk volume, the FAT 
    // sectors/blocks are located. This should only be set once here.
    //
    
    BPB *bpbPtr = (BPB *)malloc(sizeof(BPB));
    if (bpbPtr == NULL)
      print_Str("\nFailed to create pointer to BPB object\n");
      
    err = fat_SetBPB (bpbPtr);
    if (err != BPB_VALID)
    {
      print_Str("\n\r fat_SetBPB() returned ");
      fat_PrintErrorBPB(err);
    }
    

    //
    // Create and set a FatDir instance. Members of this instance are used for
    // holding parameters of a FAT directory. This instance can be treated as
    // the current working directory. The instance should be initialized to 
    // the root directory with fat_SetDirToRoot() prior to using anywhere else.
    //
    FatDir *cwdPtr = malloc(sizeof(FatDir));
    fat_SetDirToRoot (cwdPtr, bpbPtr);
    
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

    print_Str("\n\n\n\r");
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
      print_Str("\n\r"); 
      print_Str(cwdPtr->lnPathStr);
      print_Str(cwdPtr->lnStr); 
      print_Str (" > ");
      
      // ---------------------------------- Get and Parse Command and Arguments
      inputChar = usart_Receive();
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
          print_Str ("\b \b");
          if (numOfChars > 0) 
            numOfChars--;
        }
        // if no backspace then print the last char entered
        else 
        { 
          usart_Transmit (inputChar);
          inputStr[numOfChars] = inputChar;
          numOfChars++;
        }
      
        // get next char
        inputChar = usart_Receive();
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
          err = fat_SetDir (cwdPtr, argStr, bpbPtr);
          if (err != SUCCESS) 
            fat_PrintError (err);
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
          print_Str("\n\n\r");
          if (CREATION & fieldFlags) 
            print_Str(" CREATION DATE & TIME,");
          if (LAST_ACCESS & fieldFlags) 
            print_Str(" LAST ACCESS DATE,");
          if (LAST_MODIFIED & fieldFlags) 
            print_Str(" LAST MODIFIED DATE & TIME,");
          if (FILE_SIZE & fieldFlags) 
            print_Str(" SIZE (Bytes),");
          if (TYPE & fieldFlags) 
            print_Str(" TYPE,");

          print_Str(" NAME");
          print_Str("\n\r");

          err = fat_PrintDir (cwdPtr, fieldFlags, bpbPtr);
          if (err != END_OF_DIRECTORY) 
            fat_PrintError (err);
        }

        // ----------------------------- Command: "open" (print file to screen)
        else if (!strcmp(cmdStr, "open")) 
        { 
          err = fat_PrintFile (cwdPtr, argStr, bpbPtr);
          if (err != END_OF_FILE) 
            fat_PrintError (err);
        }

        // --------------------------- Command: "pwd" (print working directory)
        //
        // This will print the value of each FatDir member.
        //
        else if (!strcmp(cmdStr, "pwd"))
        {
          print_Str ("\n\rsnStr = "); 
          print_Str (cwdPtr->snStr);
          print_Str ("\n\rsnPathStr = "); 
          print_Str (cwdPtr->snPathStr);
          print_Str ("\n\rlnStr = "); 
          print_Str (cwdPtr->lnStr);
          print_Str ("\n\rlnPathStr = "); 
          print_Str (cwdPtr->lnPathStr);
          print_Str ("\n\rfstClusIndx = "); 
          print_Dec (cwdPtr->fstClusIndx);
        }

        // --------------------------------------- Command: "q" (exit cmd-line)
        else if (cmdStr[0] == 'q') 
        { 
          print_Str ("\n\rquit\n\r"); 
          quit = 1; 
        }

        // --------------------------------------- Command: "Invalid Command"
        else  
          print_Str ("\n\rInvalid command\n\r");
      }
      print_Str ("\n\r");

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
    // This section allows a user to view raw data directly using the 
    // sd_ReadSingleBlock() and sd_PrintSingleBlock() functions available in
    // the SD card module.
    
    uint32_t startBlck;
    uint32_t numOfBlcks;
    uint8_t  answer;
    uint16_t sdErr = 0;
    uint8_t  blckArr[BLOCK_LEN];
    
    do
    {
      do
      {
        print_Str ("\n\n\n\rEnter Start Block\n\r");
        startBlck = enterNumber();
        print_Str ("\n\rHow many blocks do you want to print?\n\r");
        numOfBlcks = enterNumber();
        print_Str ("\n\rYou have selected to print "); 
        print_Dec(numOfBlcks);
        print_Str (" blocks beginning at block number "); 
        print_Dec(startBlck);
        print_Str ("\n\rIs this correct? (y/n)");
        answer = usart_Receive();
        usart_Transmit (answer);
        print_Str ("\n\r");
      }
      while (answer != 'y');

      // Print blocks
      for (uint32_t blck = startBlck; blck < startBlck + numOfBlcks; blck++)
      {
        print_Str("\n\rBLOCK: ");
        print_Dec(blck);
        // SDHC is block addressable
        if (ctvPtr->type == SDHC)
          //sdErr = sd_printMultipleBlocks(startBlockNum, numOfBlocks);
          sdErr = sd_ReadSingleBlock(blck, blckArr);
        // SDSC is byte addressable
        else 
          sdErr = sd_ReadSingleBlock(blck * BLOCK_LEN, blckArr);
        
        if (sdErr != READ_SUCCESS)
          { 
            print_Str ("\n\r >> sd_printMultipleBlocks() returned ");
            if (sdErr & R1_ERROR)
              {
                print_Str ("R1 error: ");
                sd_PrintR1 (sdErr);
              }
            else 
              { 
                print_Str (" error "); 
                sd_PrintReadError (sdErr);
              }
          }
        sd_PrintSingleBlock(blckArr);
      }
      print_Str ("\n\rPress 'q' to quit: ");
      answer = usart_Receive();
      usart_Transmit (answer);
    }
    while (answer != 'q');

    //                                                   END USER INPUT SECTION
    // ------------------------------------------------------------------------
    
  }   
  
  // Something else to do. Print entered chars to screen.
  while(1)
    usart_Transmit (usart_Receive());
  
  return 0;
}


                       
/*
 ******************************************************************************
 *                             LOCAL FUNCTIONS
 ******************************************************************************
 */


// local function for taking user inputChar to specify a block
// number. If nothing is entered then the block number is 0.
uint32_t enterNumber()
{
  uint8_t x;
  uint8_t c;
  uint32_t num = 0;

  c = usart_Receive();
  
  while (c!='\r')
  {
    if ((c >= '0') && (c <= '9'))
    {
      x = c - '0';
      num *= 10;
      num += x;
    }
    else if (c == 127) // backspace
    {
      print_Str ("\b ");
      num /= 10;
    }

    print_Str ("\r");
    print_Dec (num);
    
    if (num >= 4194304)
    {
      num = 0;
      print_Str ("\n\rblock number is too large.");
      print_Str ("\n\rEnter value < 4194304\n\r");
    }
    c = usart_Receive();
  }
  return num;
}

