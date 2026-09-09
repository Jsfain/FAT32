# FAT32 File System module
Use this module to navigate and read the contents of a FAT32-formatted volume.

**Notes** 
1. This repo maintains the FAT32 File System Module files located in the **includes/**, **src/**, and **test/** folders. 
2. Additional drivers are required to handle disk, i/o, and print operations are located in the **lib/** as object and header files.
3) The drivers included in **lib/** are maintained in their own separate repos.
4) If using this FAT32 module against an ATmega1280 target, the drivers in **lib/** can be used without modification. 
5) Using against alternative targets will likely require modification to the I/O drivers at a minimum.

## Who can use
Anyone.

## Technology
* LANGUAGE      : C
* TARGET(S)     : ATmega1280 - only target tested. 
* COMPILER(S)   : AVR-GCC 9.3.0
* DOWNLOADER(S) : AVRDUDE 6.3
* Compiler and downloader available with the [AVR-Toolchain from Homebrew](https://github.com/osx-cross/homebrew-avr).

## Overview of Repo Contents
The FAT32 module files maintained in this repo are found under the **includes/**, **src/**, and **test/** paths. These files are specifically meant to support operations within the context of a File System Layer while additional modules/drivers (maintained in their own repos) are meant to handle disk, io, and print operations. 
  
### FAT Module Files

1. **FAT.C(H)**
  * All FAT-specific operations are carried out through this file. 
  * It defines functions, macros, and structs for locating and accessing the *Bios Parameter Block* (BPB), navigating the FAT32 volume directories, and reading files.
  * Any program using this module should only need to call functions defined in this file.

2. **FAT_DISK_IF.H**
  * Interface between the FAT32 module and the disk driver.
  * This file provides prototypes of the functions required for this FAT32 module to interface with a disk driver.
  * These functions are called indirectly from the functions in FAT.C and do not need to be called from the main program.
  * Currently, the only functions that need to be implemented are a fatDisk_FindBootSector and fatDisk_ReadSector. See this header file for specific details.
  * A source file (***FAT_SDCARD_IF.C***) implementing the prototypes in this header has been included in the repo for operation against a FAT32 formatted SD Card.

3. **FAT_PRINT.C(H)**
  * Use the functions defined here to print various FAT-related error messages or files

4. **AVR_FAT_TEST.C** 
  * A test file implmementing a basic command line interface to demonstrate the FAT32-formatted module.


### Included Library Files

**I/O Layer**
The target I/O layer included in **lib/** are SPI (for disk ops) and USART (for print ops) modules. These are maintained in [AVR-IO](https://github.com/Jsfain/AVR-IO)

**Disk Layer**
The disk layer included in **lib/** is an SD Card (SPI mode) reader. It is maintained in [SDCard](https://github.com/Jsfain/SDCard).

**Print Layer**
The print operations layer included in **lib/** is used to print numbers (decimal, hex, binary) and strings to an output stream (e.g. usart transmit). FAT_PRINT.C requires an implementation of these functions. This is maintained in [C-Helpers](https://github.com/Jsfain/C-Helpers).


## How to use
 * The source and header files contain descriptions of each function available and how to use them.
 * Use as-is with the included libary object and header files if using on an AVR ATMega1280 target against a FAT32-formatted SD card operating in SPI mode, while a USART port is used to print to screen.
 * If **NOT** using an AVR ATMega1280 then it will likely be necessary to replace the IO-specific files to support the desired target and disk.
 * If **NOT** using an SD card (in SPI mode) as the disk then it will be necessary include a suitable disk driver and ensure the function prototypes in  ***FAT_DISK_IF.H*** are properly implemented.


### Test File
Refer to *AVR_FAT_TEST.C* for a look at how this module can be implemented. This file contains main() and implements a very basic command line interface for interacting with a FAT32-formatted volume when implementing the included **lib/** files. The program implements commands like 'cd' to change directory, 'ls' to list directory contents, and 'open' to open/print files to a screen. See the file itself for specifics on the commands currently available. 


### Example
This section provides a brief overview of how this module can be implemented, but see the files themselves for complete descriptions of available functions, macros, structs.  

1. After any I/O-related initialization has taken place, the first FAT-specific operation must be to set an instance of a FatBPB (Bios Parameter Block) struct. A valid instance of FatBPB is required by all the other FAT functions. Note the ***g_outs*** argument in the print functions is the output stream. In the accompanying test file, this is a pointer to the usart_transmit() function.
    
  // Create and set Bios Parameter Block (BPB) instance. 
  FatBPB bpb;
  err = fat_SetBPB(&bpb);
  if (err != BPB_VALID)
  {
    print_Str("\n\r fat_SetBPB() returned ", g_outs);
    fat_PrintErrorBPB(err, g_outs);
  }

2. Once a valid BPB instance has been set, use the BPB to set a FatDir instance to the root directory. A valid FatDir instance is required by most other FAT functions.

  // Create and set current working directory instance to the root directory 
  FatDir cwd;
  fat_SetDirToRoot(&cwd, &bpb);

3. Once the FatBPB and FatDir structs have been set, most of the other navigation FAT functions can be called (see FAT.C(H)). 



## License
[GNU GPLv3](https://github.com/Jsfain/AVR-FAT/blob/master/LICENSE)


## Warnings & Disclaimers
1. This program is provided "AS IS". Use at your own risk. Back up any valuable data the disk before using with this module.
2. This was started as a project to pass the time during quarantine. Feel free to use it in accordance with the license, but no guarantees are made regarding its operation. 
3. This module was created by referencing the Microsoft FAT Specification, but there is no guarantee or claim made that it conforms fully to the specification.


## Limitations 
1. Only read operations are currently provided by the FAT module; there are no options to modify the contents of the FAT volume. This means that no files or directories nor any of their property fields can purposely be created or modified, nor can any FAT parameters be modified (i.e. boot sector/BPB, FAT, FSInfo, etc...). Even so, the SDCard module provided as an example disk layer in this repo does have raw data block write and erase capabilities. Be cautious and back up disks if there is any important data on them. See (1) under "Warnings & Disclaimers" above.
2. The FAT module is designed to operate independent of the physical disk layer as long as the required interfacing functions are implemented correctly, however, this module has only been tested using FAT32-formatted 2GB and 4GB SD Cards (in SPI mode) against an AVR ATMega1280 target.