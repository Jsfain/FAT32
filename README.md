# AVR-FAT File System
Navigate and read contents of a FAT32-formatted volume using an AVR microcontroller.


## Purpose
To provide the capability to read directories and files from a FAT32 formatted volume using an AVR microcontroller.


## Technology
* Target     : ATmega1280. Should be easily portable to other AVR targets with correct PORT reassignments, provided sufficient resources available, but have not tried this.
* Language   : C
* Compiler   : AVR-GCC 9.3.0
* Downloader : AVRDUDE 6.3
* Compiler and downloader used are available with the [AVR-Toolchain from Homebrew](https://github.com/osx-cross/homebrew-avr).


## Overview
In this current implementation of the AVR-FAT module, the primary functions, macros, and structs are separated into two separate source/header files, both of which are required for any use of this module.  The first is FAT_BPB.C/H which is used simply for locating and accessing the Bios Parameter Block (BPB) / Boot Sector.  The second set of source/header files (FAT.C/H) is for accessing and navigating the FAT directory hierarchy and reading files which requires the BPB. Since it is intended that this AVR-FAT module be independent of the physical disk, a third header file (FAT_TO_DISK_IF.H) is also included which provides function prototypes for interfacing with a disk driver which should be used in order to access the raw sector/block data on a physical FAT-formatted disk. These functions MUST be implemented. An example of their implementation when using the AVR-SDCard module for raw data access is provided FAT_TO_SD.C. See the ***Physical Disk Layer*** section below.

### AVR-FAT Module Files - all are required
1. **FAT_BPB.C(H)**
  * The functions, macros, and structs in this set of source/header files are for locating and accessing the *Bios Parameter Block* (BPB) and storing its necessary fields in a BPB struct. A pointer to this struct must be passed to nearly all of the other functions in this module.
  * Before any other FAT function can be called the BPB struct instance must be created and set by using the *fat_SetBPB* function.

2. **FAT.C(H)**
  * The functions, macros, and structs defined here are for accessing and navigating the FAT volume directories and reading files.

3. **FAT_TO_DISK_IF.H**
  * In order to use this AVR-FAT module, a disk driver must be provided that can read the required disk sectors/blocks. This file provides prototypes of the functions required to interface with a disk driver for physical disk access.
  * The necessary requirements of the implementation of these prototyped functions are provided in this header file.
  * How the raw data on a physical disk is accessed is out of scope for this module, but an example of the implementation of these required interfacing functions can be found in FAT_TO_SD.C. This file implements these functions in order to interface between this AVR-FAT module and the AVR-SDCard module which provides sector/block raw data access to an SD card.

### Additional Included Files
The following source/header files are also required by the module, and thus included in the repository but they are maintained in [AVR-General](https://github.com/Jsfain/AVR-General.git)

1. USART.C(H)   : required to interface with the AVR's USART port used to print messages and data to a terminal.
2. PRINTS.H(C)  : required to print integers (decimal, hex, binary) and strings to the screen via the USART.
3. SPI.C(H)     : this is required by the AVR-SDCard module which interfaces with an SD Card via the AVR's SPI port.

### Physical disk layer
As mentioned above, this FAT module is intended to be independent of a physical disk layer/driver and thus a disk driver is required to read in the raw data from any physical FAT32-formatted volume. The file FAT_TO_DISK_IF.H provides the two prototypes functions that must be implemented in order for a disk driver to interface with this AVR-FAT module. These functions are:

1) uint32_t FATtoDisk_FindBootSector(void);
2) uint8_t FATtoDisk_ReadSingleSector(uint32_t address, uint8_t *array); 

The requirements for these disk driver interfacing functions can be found in their description in the FAT_TO_DISK_IF.H header file.

*NOTE: This project was tested by using the [AVR-SDCard module](https://github.com/Jsfain/AVR-SDCard) as the physical disk layer. As such, the necessary files from this module have been included in this repo for reference, but they are not considered part of the AVR-FAT module, and may or may not represent the most recent version of the AVR-SDCard module. Additionally, the AVR-SDCard module uses the AVR's SPI port and so the SPI.C and SPI.H files have also been included. These files are maintained in [AVR-General](https://github.com/Jsfain/AVR-General)*


## How To Use
 * Clone the repo and/or copy the required source files, then build and download to the AVR using your preferred method (Atmel Studio, AVR Toolchain, etc...). 


### AVR_FAT_TEST.C 
Probably the best way to understand how to use this AVR-FAT module is to refer to the *AVR_FAT_TEST.C* file. This file contains main() and implements a command-line like interface for interacting with a FAT32-formatted volume. The program implements commands like 'cd' to change directory, 'ls' to list directory contents, 'open' to open/print files to a screen. See the file itself for specifics on the commands currently available. 


### Example
* The files themselves contain the full macro, struct, and function descriptions, but this short ***Example*** section below provides a brief overview of how to this module can be implemented. 
* The sequence of required steps is to first set a BPB struct instance (see FAT_BPB.C/H) and then set a FatDir instance (see FAT.C/H).
* Nearly all of the FAT functions in FAT.C require an instance of BPB and FatDir to be passed to it.
    * The BPB struct instance will store the necessary parameters from the BIOS Parameter Block which are needed in order to navigate a FAT volume on a physical disk.
    * The FatDir struct instance is used as a 'Current Working Directory' (cwd), and before it can be used, it must first be initialized to the *Root Directory*.
* Once a BPB struct and FatDir struct instance have been created and set then the other FAT functions can be called. 

```
    uint8_t err;   // for returned errors
    
    // Create and set Bios Parameter Block (BPB) instance. 
    BPB bpb; 
    err = fat_SetBPB(&bpb);
    if (err != BOOT_SECTOR_VALID)
    {
      // failed to get/set BPB
      // to hangle failure can either try again or exit.
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
      // Failure most likely due to Child Directory A not existing in cwd.
      fat_PrintError (err);
    }

    // print the contents of the file, *File A*, to a terminal. This file must
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
1. Only read operations are currently provided by the AVR-FAT module; there are no options to modify the contents of the FAT volume. This means that no files or directories nor any of their property fields can purposely be created or modified, nor can any FAT parameters be modified (i.e. boot sector/BPB, FAT, FSInfo, etc...). Even so, the AVR-SDCard module provided for disk access examples does have write and erase capabilities. Be cautious and back up disks if there is any important data on them. See (1) under "Warnings & Disclaimers" above.
2. Though the AVR-FAT module is designed to operate independent of the physical disk layer, as long as the required interfacing functions are implemented correctly, this module has only been tested using FAT32-formatted 2GB and 4GB SD Cards using the AVR-SDCard module as the physical disk layer.
3. In the current implementation, the module will only work if the boot sector is in block 0 on the disk. This is a limitation of the current implementation of sd_SetBPB.
