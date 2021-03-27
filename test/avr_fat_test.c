/*
 *                           Test file for AVR-FAT Module
 *
 * File       : AVR_FAT_TEST.C
 * Author     : Joshua Fain
 * Target     : ATMega1280
 * Compiler   : AVR-GCC 9.3.0
 * Downloader : AVRDUDE 6.3
 * License    : GNU GPLv3
 * Author     : Joshua Fain
 * Copyright (c) 2020, 2021
 * 
 * DESCRIPTION: 
 * Implements a simple command line-like interface that can be used to navigate
 * and read a FAT volume. This 'test' implementation uses the AVR-SD Card 
 * module as the physical disk driver to access the raw data contents of a 
 * FAT32-formatted SD card. This SD card driver is included in the repo, but 
 * not considered part of the AVR-FAT module.
 *
 * COMMANDS:
 *  (1) cd <DIR>      : Change directory to the directory specified by <DIR>.
 *  (2) ls <FIELDS>   : List directory contents based on specified <FILTERs>.
 *  (3) open <FILE>   : Print contents of <FILE> to a screen.
 *  (4) pwd           : Print the current working directory to screen.
 * 
 * NOTES: 
 * (1)  The module only has READ capabilities.
 * (2)  Quotation marks should NOT surround file or directory names even if 
 *      a space exists in the name.
 * (3)  Directory and file name arguments are case sensitive.
 * (4)  'cd' cmd can be used to reset cwd (current working directory) to point
 *      to the ROOT directory or change it to a CHILD or the PARENT of the 
 *      current directory pointed at by cwd.
 * (5)  'open' will only work for files that are in the cwd directory. 
 * (6)  Pass ".." (without quotes) as the argument to 'cd' to point cwd to
 *      its parent directory.
 * (7)  Pass "~" (without quotes) as the argument to 'cd' to reset cwd to point
 *      to the ROOT directory.
 * (8)  If no arg is given to 'ls', then the long names of non-hidden entries 
 *      in the directory pointed at by cwd will be printed. 
 * (9)  The following options are available as "fields" for the 'ls' command.
 *      Pass any combination of these:
 *       /LN : Print long name of each entry if it exists (default).
 *       /SN : Print short name of each entry.
 *       /H  : Print hidden entries.
 *       /T  : Print the entry type (file or dir). 
 *       /FS : Print the file size of the entry.
 *       /C  : Print entry creation date and time.
 *       /LM : Print last modified date and time.
 *       /LA : Print last access date.
 *       /A  : ALL - prints all entries and all fields.
 *
 * (10) Enter 'q' to exit the command-line. If the SD_CARD_READ_DATA macro is
 *      set then there an SD Card raw data access section will also be entered.
 */

#include <string.h>
#include <stdint.h>
#include "usart0.h"
#include "spi.h"
#include "prints.h"
#include "sd_spi_base.h"
#include "sd_spi_rwe.h"
#include "fat_bpb.h"
#include "fat.h"
#include "fat_to_disk_if.h"

#define SD_CARD_INIT_ATTEMPTS_MAX      5  
#define CMD_LINE_MAX_CHAR              100  // max num of chars of a cmd/arg
#define MAX_ARG_CNT                    10   // max num of CL arguments
#define BACKSPACE                      127  // used for keyboard backspace here

//
// setting this to 1 enables the SD Card Raw Data block read and prints section
// at the end of the test file as well as the necessary local functions and
// macros used by this section.
//
#define SD_CARD_READ_DATA              0

// macros and functions used by the SD_CARD_READ_BLOCK_DATA section.
#if SD_CARD_READ_DATA
#define MAX_DATA_BYTES_32_BIT          2147483648 
#define MAX_BLOCK_NUM_32_BIT           MAX_DATA_BYTES_32_BIT / BLOCK_LEN    
static uint32_t enterBlockNumber();          
#endif // SD_CARD_READ_DATA          

int main(void)
{
  // Initializat usart and spi ports.
  usart_Init();
  spi_MasterInit();

  //
  // SD card initialization
  //
  CTV ctv;          
  uint32_t sdInitResp;

  // Loop will continue until SD card init succeeds or max attempts reached.
  for (uint8_t att = 0; att < SD_CARD_INIT_ATTEMPTS_MAX; ++att)
  {
    print_Str("\n\n\r >> SD Card Initialization Attempt "); 
    print_Dec(att);
    sdInitResp = sd_InitModeSPI(&ctv);      // init SD Card

    if (sdInitResp != OUT_OF_IDLE)          // Fail to init if not OUT_OF_IDLE
    {    
      print_Str(": FAILED TO INITIALIZE SD CARD."
                " Initialization Error Response: "); 
      sd_PrintInitError(sdInitResp);
      print_Str(" R1 Response: "); 
      sd_PrintR1(sdInitResp);
    }
    else
    {   
      print_Str(": SD CARD INITIALIZATION SUCCESSFUL");
      break;
    }
  }

  //
  // Implement command line
  //
  if (sdInitResp == OUT_OF_IDLE)
  {          
    uint8_t err;                            // for returned errors
    uint8_t quitCL = 0;                     // flag used to exit cmd line  
    //
    // Create and set Bios Parameter Block instance. Members of this instance
    // are used to calculate where on the disk, the FAT sectors are located. 
    // This should only be set here.
    //
    BPB bpb;
    err = fat_SetBPB(&bpb);
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
    FatDir cwd;
    fat_SetDirToRoot(&cwd, &bpb);

    print_Str("\n\n\n\r");
    do
    {
      char inputChar;                       // for input chars of cmd/arg
      char inputStr[CMD_LINE_MAX_CHAR];     // hold cmd/arg str
      char cmdStr[CMD_LINE_MAX_CHAR];       // separate cmd from inputStr
      char argStr[CMD_LINE_MAX_CHAR];       // separate arg from inputStr
      uint8_t charCnt = 0;                  // number of chars in inputStr
      uint8_t fieldFlags = 0;               // fields printed with 'ls' cmd

      // print cmd prompt to screen with cwd
      print_Str("\n\r");
      print_Str(cwd.lnStr);
      print_Str(" > ");

      // 
      // get (from user) and parse command and arguments
      //
      inputChar = usart_Receive();
      while (inputChar != '\r')
      {
        //
        // Handles backspace \ delete chars. BACKSPACE macro above is set to 
        // the value of the backspace key on my keyboard, which is ascii 'del'
        // (127). Ascii backspace is 8 ('\b'). To behave as expected when 
        // backspace is pressed, i.e. delete previous char, this section prints
        // backspace ('\b'), space (' '), backspace ('\b').
        // 
        if (inputChar == BACKSPACE)         // if backspace entered  
        {
          print_Str ("\b \b");              // perform backspace 'delete' op.
          if (charCnt > 0)                  // reduce char count if > 0
            --charCnt;
        }
        else                                // non-backspace, print last char entered
        { 
          usart_Transmit(inputChar);        // print char
          inputStr[charCnt] = inputChar;    // load char into array
          ++charCnt;                        // increase char count
        }
      
        // get next char
        inputChar = usart_Receive();
        if (charCnt >= CMD_LINE_MAX_CHAR) 
          break;
      }
      inputStr[charCnt] = '\0';

      // split command and arguments into separate strings
      char *splitPtr = strchr(inputStr, ' ');
      if (splitPtr != NULL)
      {
        *splitPtr = '\0';
        strcpy(argStr, ++splitPtr);
      }
      strcpy(cmdStr, inputStr);
 
      //
      // Execute Command
      //
      if (charCnt < CMD_LINE_MAX_CHAR) 
      {
        //
        // Command: "cd" (change directory)
        //
        if (!strcmp(cmdStr, "cd"))
        {   
          err = fat_SetDir(&cwd, argStr, &bpb);
          if (err != SUCCESS) 
            fat_PrintError (err);
        }
        //
        // Command: "ls" (list dir contents)
        //
        else if (!strcmp(cmdStr, "ls"))
        {
          for (uint8_t argCnt = 0, lastArgFlag = 0; 
               argCnt < MAX_ARG_CNT || !lastArgFlag; ++argCnt)
          {
            char *argStrPtr = strchr(argStr, ' '); // find next argument
            if (argStrPtr == NULL)           // no more arguments
              lastArgFlag = 1;
            else
              *argStrPtr = '\0';             // null-term for substring args.
            
            // set flags to print fields according to the arguments specified
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
            
            strcpy(argStr, ++argStrPtr);    // start argStr at next arg 
          }

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

          err = fat_PrintDir(&cwd, fieldFlags, &bpb);
          if (err != END_OF_DIRECTORY) 
            fat_PrintError (err);
        }
       
        //
        // Command: "open" (print file to screen)
        //
        else if (!strcmp(cmdStr, "open")) 
        { 
          err = fat_PrintFile(&cwd, argStr, &bpb);
          if (err != END_OF_FILE) 
            fat_PrintError(err);
        }
        
        //
        // Command: "pwd" (print working directory)
        //
        else if (!strcmp(cmdStr, "pwd"))
        {
          print_Str("\n\r");
          print_Str (cwd.lnPathStr);
          print_Str (cwd.lnStr);
        }

        //
        // Command: "q" (exit cmd-line)
        //
        else if (cmdStr[0] == 'q') 
        { 
          print_Str ("\n\rquit\n\r"); 
          quitCL = 1; 
        }
        
        // 
        // Invalid Command
        //
        else
          print_Str ("\n\rInvalid command\n\r");
      }
      print_Str ("\n\r");

      // ensure USART0 Data Register, URD0, is cleared.
      for (int k = 0; k < 10; k++) 
        UDR0; 
    }
    while (!quitCL);
    // END of COMMAND-LINE TEST                
   
    #if SD_CARD_READ_DATA
    // 
    // This section allows a user to view raw block data directly using the 
    // sd_ReadSingleBlock() and sd_PrintSingleBlock() functions available in
    // the SD card module. This is just something extra if this AVR-FAT module
    // is being used with the AVR-SD Card module.
    //
    uint8_t  answer;
    do
    {
      uint32_t startBlck;
      uint32_t numOfBlcks;
      uint16_t sdErr = 0;
      uint8_t  blckArr[BLOCK_LEN];

      do
      {
        print_Str("\n\n\n\rEnter Start Block\n\r");
        startBlck = enterBlockNumber();
        print_Str("\n\rHow many blocks do you want to print?\n\r");
        numOfBlcks = enterBlockNumber();
        print_Str("\n\rYou have selected to print "); 
        print_Dec(numOfBlcks);
        print_Str(" blocks beginning at block number "); 
        print_Dec(startBlck);
        print_Str("\n\rIs this correct? (y/n)");
        answer = usart_Receive();
        usart_Transmit(answer);
        print_Str("\n\r");
      }
      while (answer != 'y');

      // Print blocks
      for (uint32_t blck = startBlck; blck < startBlck + numOfBlcks; ++blck)
      {
        print_Str("\n\rBLOCK: ");
        print_Dec(blck);
        if (ctv.type == SDHC)               // SDHC is block addressable
          sdErr = sd_ReadSingleBlock(blck, blckArr);
        else                                // SDSC is byte addressable
          sdErr = sd_ReadSingleBlock(blck * BLOCK_LEN, blckArr);
        
        if (sdErr != READ_SUCCESS)
        { 
          print_Str("\n\r >> sd_ReadSingleBlock returned ");
          if (sdErr & R1_ERROR)
          {
            print_Str("R1 error: ");
            sd_PrintR1(sdErr);
          }
          else 
          { 
            print_Str(" error "); 
            sd_PrintReadError(sdErr);
          }
        }
        sd_PrintSingleBlock(blckArr);
      }
      print_Str("\n\rPress 'q' to quit: ");
      answer = usart_Receive();
      usart_Transmit(answer);
    }
    while (answer != 'q');

    #endif // SD_CARD_READ_DATA
  }   
  
  // Something else to do. Print user-entered chars to screen.
  while(1)
    usart_Transmit (usart_Receive());
  
  return 0;
}


                       
/*
 ******************************************************************************
 *                             LOCAL FUNCTIONS
 ******************************************************************************
 */

#if SD_CARD_READ_DATA
//
// local function used by the SD_CARD_READ_BLOCK_DATA that gets and returns the
// number/address of the block on the SD card that should be read and printed.
//
static uint32_t enterBlockNumber()
{
  uint8_t  decDigit;
  uint8_t  asciiChar;
  uint32_t blkNum = 0;
  uint8_t  radix = 10;                      // decimal radix

  asciiChar = usart_Receive();
  
  while (asciiChar != '\r')
  {
    if (asciiChar >= '0' && asciiChar <= '9') // if decimal ascii char entered
    {
      decDigit = asciiChar - '0';           // convert ascii to decimal digit
      blkNum *= radix;
      blkNum += decDigit;
    }
    else if (asciiChar == BACKSPACE)        // if backspace on keyboard entered
    {
      print_Str("\b ");                     // print backspace and space chars
      blkNum = blkNum / radix;       // reduce current blkNum by factor of 10
    }
    print_Str("\r");
    print_Dec(blkNum);
    
    if (blkNum >= MAX_BLOCK_NUM_32_BIT)
    {
      blkNum = 0;                           // reset block number
      print_Str("\n\rblock number too large. Enter value < ");
      print_Dec(MAX_BLOCK_NUM_32_BIT);
      print_Str("\n\r");  
    }
    asciiChar = usart_Receive();
  }
  return blkNum;
}
#endif // SD_CARD_READ_DATA
