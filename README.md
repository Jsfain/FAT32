# FAT32 File System module
Navigate and read the contents of a FAT32-formatted volume.

## Who can use
Anyone.

## How to use
 * The source and header files contain descriptions of each function available and how to use them.
 * If using against an AVR ATMega1280 target with an SD card as the FAT32-formatted volume, then simply clone/copy the repo, compile / build, and download to the AVR. 
 * If **NOT** using an AVR ATMega1280 then it will be necessary to either modify, or replace, the IO-specific files that have been included in the repo (AVR_SPI and AVR_USART), to support the desired target.
 * If **NOT** using an SD card as the physical disk then it will be necessary include a suitable disk driver for the desired disk layer and ensure that to implement the functions and definitions provided in ***FAT_TO_DISK_IF.H***.
 d

## Technology
* LANGUAGE      : C
* TARGET(S)     : ATmega1280 - only target tested. 
* COMPILER(S)   : AVR-GCC 9.3.0
* DOWNLOADER(S) : AVRDUDE 6.3
* Compiler and downloader used were available with the [AVR-Toolchain from Homebrew](https://github.com/osx-cross/homebrew-avr).


## Overview of Repo Structure
This repo is split into multiple functional directories with the attempt to make this as modular as possible. These functionalities include:

  1.  File System Layer
  2.  Disk Layer
  3.  IO / Target Layer
  
### File System Layer: FAT Module Files - all are required
 * The File System Layer is this FAT module, and only includes those files under the FAT directory. These are the only files maintained in this repo and include:

1. **FAT_BPB.C(H)**
  * The functions, macros, and structs in this set of source/header files are for locating and accessing the *Bios Parameter Block* (BPB) and storing its necessary fields in a BPB struct. A pointer to this struct must be passed to nearly all of the other functions in this module.
  * Before any other FAT function can be called the BPB struct instance must be created and set by using the *fat_SetBPB* function.

2. **FAT.C(H)**
  * The functions, macros, and structs defined here are for accessing and navigating the FAT volume directories and reading files.

3. **FAT_TO_DISK_IF.H**
  * In order to use this AVR-FAT module, a disk driver must be provided that can read the required disk sectors/blocks. This file provides prototypes of the functions required to interface with a disk driver for physical disk access.
  * The necessary requirements of the implementation of these prototyped functions are provided in this header file.
  * How the raw data on a physical disk is accessed is out of scope for this module, but an example of the implementation of these required interfacing functions can be found in FAT_TO_SD.C. This file implements these functions in order to interface between this AVR-FAT module and the AVR-SDCard module which provides sector/block raw data access to an SD card.

### Disk Layer:
In this repo, the files under **SD** are provided as a disk layer for use when the FAT32-formatted volume is hosted on an SD card. If this is not the case then the disk layer/driver will need to be provided and interface with the FAT module by implementing the functions and definitions found in ***FAT_TO_DISK_IF.H***. The SD card module is maintained in [SDCard](https://github.com/Jsfain/SDCard).

### IO / Target Layer:
In this repo, the files under **avrio** are provided as the target IO layer. The original intended target device was an AVR ATMega1280 microcontroller, and the SD card disk layer interfaced with the target through the SPI port using ***AVR_SPI.C(H)***.  Additionally, ***AVR_USART.C(H)*** is used by the **PRINTS.C(H)** helper functions for printing strings and integers to the console. These files are maintained in [AVR-IO](https://github.com/Jsfain/AVR-IO.git)

### Helper Files - required
**PRINTS.H(C)**: this is simply used to print integers (decimal, hex, binary) and strings to the screen via a UART. It is platform independent, but here uses AVR_USART.C(H) for all USART functionality against an ATMega1280 target. The functions provided in these files are used extensively for any printing to the console. This is maintained in [C-Helpers](https://github.com/Jsfain/C-Helpers).


## Physical disk layer interface details
As alluded to above, this FAT module is intended to be independent of a physical disk layer/driver and thus a disk driver is required to read in the raw data from any physical FAT32-formatted volume. The file **FAT_TO_DISK_IF.H** provides the two prototype functions that must be implemented in order for a disk driver to interface with this FAT module. These functions are:

1) uint32_t FATtoDisk_FindBootSector(void);
2) uint8_t FATtoDisk_ReadSingleSector(uint32_t address, uint8_t *array); 

The requirements for these disk driver interfacing functions can be found in their description in the FAT_TO_DISK_IF.H header file.

*NOTE: This project was tested by using the [SDCard module](https://github.com/Jsfain/SDCard) as the physical disk layer. As such, the necessary files from this module have been included in this repo for reference, but they are not considered part of the FAT32 module, and may or may not represent the most recent version of the SDCard module. Additionally, the SDCard module uses the AVR's SPI port and so the AVR_SPI.C / AVR_SPI.H files have also been included. These files are maintained in [AVR-IO](https://github.com/Jsfain/AVR-IO)*


## AVR_FAT_TEST.C 
Probably the best way to understand how to use this FAT32 module is to refer to the *AVR_FAT_TEST.C* file. This file contains main() and implements a command-line like interface for interacting with a FAT32-formatted volume hosted on an SD card and using an AVR ATMega1280 target. The program implements commands like 'cd' to change directory, 'ls' to list directory contents, 'open' to open/print files to a screen. See the file itself for specifics on the commands currently available. 


### Example
* The files themselves contain the full macro, struct, and function descriptions, but this short ***Example*** section below provides a brief overview of how this module can be implemented. 
* The sequence of required steps is to first set a BPB struct instance (see FAT_BPB.C/H) and then set a FatDir instance (see FAT.C/H).
* Nearly all of the FAT functions in FAT.C require an instance of BPB and FatDir to be passed to it.
    * The BPB struct instance will store the necessary parameters from the BIOS Parameter Block which are needed in order to navigate a FAT volume on a physical disk.
    * The FatDir struct instance is used as a 'Current Working Directory' (cwd), and before it can be used, it must first be initialized to the *Root Directory*.
* Once a BPB struct and FatDir struct instances have been created and set then the other FAT functions can be called. 

```
    uint8_t err;   // for returned errors
    
    // Create and set Bios Parameter Block (BPB) instance. 
    BPB bpb; 
    err = fat_SetBPB(&bpb);
    if (err != BOOT_SECTOR_VALID)
    {
      // failed to get/set BPB
      // to handle failure, either try again or exit.
      fat_PrintErrorBPB(err);
    }
   
    // Create and set a FatDir instance to the Root Directory.
    // The instance acts as the 'Current Working Directory'.
    FatDir cwd;
    fat_SetDirToRoot(&cwd, &bpb);

    // Print the 'long name' and 'file size' of the (non-hidden)
    // entries of the root directory (i.e. current setting of cwd)
    err = fat_PrintDir(&cwd, LONG_NAME | FILE_SIZE, &bpb);
    if (err != END_OF_DIRECTORY) 
    {
      // Failed.
      fat_PrintError(err);
    }

    // set cwd to a child of the root direcotry, i.e. "Child Directory A".
    err = fat_SetDir (&cwd, "Child Directory A", &bpb);
    if (err != SUCCESS) 
    {
      // Failure most likely due to "Child Directory A" not existing in cwd.
      fat_PrintError (err);
    }

    // print the contents of the file, "File A", to a terminal. This file must
    // be in the directory pointed to by cwd. Here this directory should be 
    // "Child Directory A".
    err = fat_PrintFile (&cwd, "File A", &bpb);
    if (err != END_OF_FILE) 
    {
      // failed
      fat_PrintError (err);
    }
```

## License
[GNU GPLv3](https://github.com/Jsfain/AVR-FAT/blob/master/LICENSE)


## Warnings & Disclaimers
1. This program is provided "AS IS". Use at your own risk and back up any data on a disk that is to be used with this module.
2. I made this for fun to pass the time during quarantine. Feel free to use it in accordance with the license, but no guarantees are made regarding its operation. 
3. This module was created by referencing the Microsoft FAT Specification, but there is no guarantee or claim made that it conforms fully to the specification.


## Limitations 
1. Only read operations are currently provided by the FAT module; there are no options to modify the contents of the FAT volume. This means that no files or directories nor any of their property fields can purposely be created or modified, nor can any FAT parameters be modified (i.e. boot sector/BPB, FAT, FSInfo, etc...). Even so, the SDCard module provided as an example disk layer in this repo does have raw data block write and erase capabilities. Be cautious and back up disks if there is any important data on them. See (1) under "Warnings & Disclaimers" above.
2. Though the FAT module is designed to operate independent of the physical disk layer, as long as the required interfacing functions are implemented correctly, this module has only been tested using FAT32-formatted 2GB and 4GB SD Cards using the SDCard module as the physical disk layer against an AVR ATMega1280 target.
3. In the current implementation, the module will only work if the boot sector is in block 0 on the disk. This is a limitation of the current implementation of fat_SetBPB.
