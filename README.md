# AVR-FAT File System
Read a FAT32-formatted volume using an AVR microcontroller.

## Purpose
Provide capability to read directories and files from a FAT32 formatted volume using an AVR microcontroller.


## Details
* TARGET: *ATmega1280* - can be easily extended to other AVR targets with sufficient memory/resources. May require modification of port assignments and settings.
* LANGUAGE: *C*
* Built on a macOS using the [AVR-Toolchain](https://github.com/osx-cross/homebrew-avr) 9.3.0, includes: 
  * compiler: *AVR-GCC - 9.3.0*
  * downloader: *AVRDUDE - 6.3*


## Overview of included files
In this current implementation of the AVR-FAT module, the primary functions, macros, and structs are separated into two separate source/header files, both of which are required for any use of this module.  The first is FAT_BPB.C/H which is used simply for locating and accessing the Bios Parameter Block (BPB) / Boot Sector.  The second set of source/header (FAT.C/H) is for accessing and navigating the FAT directory hierarchy and reading files which requires the BPB. Since it is intended that this AVR-FAT module be independent of a physical disk, a third header file (FAT_TO_DISK.H) has is also included which provides the prototypes for the few functions that MUST be implemented by a physical disk driver in order to access the raw data.

### AVR-FAT Module Files - all are required
1. **FAT_BPB.C and FAT_BPB.H**
  * The functions, macros, and structs in this set of source/header files are for locating and accessing the *Bios Parameter Block* (BPB) and add storing its necessary fields in a BPB struct which is used by nearly all of the other functions in this module.
  * Before any other FAT function can be called the BPB struct instance must be created and set by using the *fat_setBPB(*BPB)* function.

2. **FAT.C and FAT.H**
  * The functions, macros, and structs defined here are for accessing and navigating the FAT volume directories and reading files.

3. **FAT_TO_DISK.H**
  * In order to use this AVR-FAT module, a physical disk driver must be provided that can read the 512 byte disk sector/blocks. 
  * This file is used to provide prototypes of the two functions required for physical disk access.
  * How the raw data on a physical disk is accessed is considered out-of-scope for this module, but an example is provided in FAT_TO_SD.C which implements the prototype functions in FAT_TO_DISK.H here as an interface between this AVR-FAT module and an SD card raw data access module.


### Additional Files
The USART.C/H and PRINTS.C/H are also included in the repo. Though they are not necessarily required for FAT access, they are needed in order to print anything to a terminal. Thus, they are needed in order to print the contents of a directory or file, and used for any 'print error' functions as well. 


### Physical disk layer
As mentioned above, this FAT module is intended to be independent of a physical disk layer/driver and thus a disk layer is required to read in the raw data from any physical FAT32-formatted disk itself. The file FAT_TO_DISK.H provides the two prototypes for functions that must be implemented in order for a disk driver to interface with this AVR-FAT module. These functions are:

1) uint32_t FATtoDisk_findBootSector(void);
2) uint8_t FATtoDisk_readSingleSector (uint32_t address, uint8_t *array); 

The requirements for these physical disk interfacing functions can be found in their description in the header file.

*NOTE: This project was tested by using the [AVR-SDCard module](https://github.com/Jsfain/AVR-SDCard) as the physical disk layer. As such, the necessary files from this module have been included in this repo for reference, but they are not considered part of the AVR-FAT module, and may or may not represent the most recent version of the AVR-SD Card module. Additionally, the AVR-SDCard module uses the AVR's SPI port and so the SPI.C and SPI.H files have also been included. These files are maintained in [AVR-General](https://github.com/Jsfain/AVR-General)*


## How To Use
 * Clone the repo and/or copy the required source files, then build and download to the AVR using your preferred method (Atmel Studio, AVR Toolchain, etc...). 
 * The files themselves contain the full macro, struct, and function descriptions, however, the section below provides a brief example of how to implement this. 
  
### AVR_FAT_TEST.C 
Probably the best way to understand how to use this AVR-FAT module is to refer to the *AVR_FAT_TEST.C* file. This file contains main() and implements a command-line like interface for interacting with a FAT32-formatted volume. The program implements commands like 'cd' to change directory, 'ls' to list directory contents, 'open' to open files and print them to the screen. See the file itself for specifics on the commands currently available. 

### Example
 * The sequence of required steps is to first set a BPB struct instance (see FAT_BPB.C/H) and then set a FatDir instance (see FAT.C/H).
 * Nearly all of the FAT functions in FAT.C require an instance of BPB and FatDir to be passed to it.
      * The BPB struct instance will store the necessary parameters from the BIOS Parameter Block which are needed in order to navigate a FAT volume on a physical disk.
      * The FatDir struct instance is used as a 'Current Working Directory' (cwd), and before it can be used, it must first be initialized to the *Root Directory*.
 * Once a BPB struct and FatDir struct instance have been created and set then the other FAT functions can be called. 

```
    uint8_t err;   // for returned errors
    
    // Create and set Bios Parameter Block (BPB) instance. 
    BPB bpb; 
    err = fat_setBPB (&bpb);
    if (err != BOOT_SECTOR_VALID)
    {
      // failed to get/set BPB
      fat_printBootSectorError(err);
    }
   
    // Create and set a FatDir instance to the Root Directory.
    // The instance is the 'Current Working Directory'.
    FatDir cwd;
    fat_setDirToRoot (&cwd, &bpb);

    // Print the *long name* and *file size* of the (non-hidden) contents of the root directory
    err = fat_printDir (&cwd, LONG_NAME | FILE_SIZE, &bpb);
    if (err != END_OF_DIRECTORY) 
    {
      // failed
      fat_printError (err);
    }

    // set *cwd* to a child of the root direcotry, i.e. "Child Directory A".
    err = fat_setDir (&cwd, "Child Directory A", &bpb);
    err = fat_setDir (cwdPtr, argStr, bpbPtr);
    if (err != SUCCESS) 
    {
      // failed
      fat_printError (err);
    }

    // print the contents fo the file, *File A*, to a terminal. This file must be in the
    // directory pointed to by cwd. Here this directory would be "Child Directory A".
    err = fat_printFile (&cwd, "File A", &bpb);
    if (err != END_OF_FILE) 
    {
      // failed
      fat_printError (err);
    }


```


## Warnings / Disclaimers / Limitations 
1. This program is provided AS IS. I take no responsibility for any damage that may result from its use. Use it at your own risk and back up any data on a disk that is to be used with this module.
2. This module was created by referencing the Microsoft FAT Specification, but there is no guarantee that it conforms to the standard.
3. I made this for fun to pass the time during quarantine. That is all. Feel free to use it however you wish, but no guarantees are made regarding its operability.
4. Currently only read operations are provided by the module; there are no options to modify the contents of the FAT volume. This means that no files or directories nor any of their property fields can purposely be created or modified, nor can any FAT parameters be modified (i.e. boot sector/BPB, FAT, FSInfo, etc...). Even so, things can happen. See (1) above.
5. Though the AVR-FAT module is designed to operate independent of the physical disk layer, as long as the required interfacing functions are implemented correctly, this module has only been tested using FAT32-formatted 2GB and 4GB SD Cards using the AVR-SDCard module as the physical disk layer.



## License
[GNU GPLv3](https://github.com/Jsfain/AVR-FAT/blob/master/LICENSE)