# FAT Filesystem Module
Read a FAT32-formatted volume using an AVR microcontroller.


## Purpose
To provide a module to read data/files from a FAT32 formatted disk/volume from an AVR Microcontroller.


## How To Use
Use this module by copying the source files and compile, build, and download the module to an AVR microcontroller. See the MAKE.SH file included in the repo to see how I built the project using the AVR-Toolchain. You should also be able to implement this process easily in Atmel Studio, but I did not have this available on my system.

The best way to understand how to implement this module / FAT functions is to see the AVR_FAT_TEST.C file. This file contains main() and implements a command-line like interface using the functions in FAT.C/H.


## Details
* TARGET: ATmega1280 - This is expected to be easily implemented on other AVR targets with modification of port assignments and settings, and assuming memory and other resources are sufficient.
* LANGUAGE: C
* [AVR-Toolchain](https://github.com/osx-cross/homebrew-avr) 9.3.0 , This includes: 
  * AVR-GCC 9.3.0: required to compile/build the module.
  * AVRDUDE 6.3: required to download the to the AVR.


## Overview of included files

### AVR-FAT Module Files
This module provides a set of functions that can be used to read files and navigate directories on a FAT32 formatted disk using an AVR microcontroller. The files included with the module are:

1) FAT.C / FAT.H
2) FATtoDISK_INTERFACE.H

FAT.C and its header FAT.H define/declare the specific FAT functions for setting the current working directory, reading directory contents, and reading file contents. The specifics of how each function operates is provided in the source files. The best way to see how the FAT functions are intended to be used is to see the AVR_FAT_TEST.C.


### Physical disk layer
This FAT module is intended to be independent of a physical disk layer/driver and thus a disk layer is required to read in the raw data from the physical FAT32-formatted disk itself. The file FATtoDISK_INTERFACE.H declares two functions that must be defined to interface the AVR-FAT module to the physical disk layer. These functions are 

1) uint32_t FATtoDisk_FindBootSector();
2) uint8_t FATtoDisk_ReadSingleSector (uint32_t address, uint8_t * arr); 

The requirements for these physical disk interfacing functions can be found in their description in the header file.

NOTE: This project was tested by using the [AVR-SDCard module](https://github.com/Jsfain/AVR-SDCard) as the physical disk layer. I've included source files from that module in this repo for reference, but they are not considered part of the AVR-FAT module.


### Additional Files
Other files included are:
1) USART.C/H - this file is used for printing to a screen.
2) PRINTS.C/H provides a few simple functions for printing strings and integers in decimal (positve numbers only), hexadecimal, and binary forms.
3) SPI.C/H is included with this specific implementation because it is required by the AVR-SDCard module.
 

## Warning
Use at your own risk and back up any data on the disk that is to be used with this module. The module does not implement any 'write/modify' operations, but things happen and it is possible that a disk could get into a bad state and become corrupted and requiring reformatting.


## Disclaimers / Limitations 
* I made this for fun to pass the time during quarantine. That is all. Feel free to use however you wish, but no guarantees are made regarding its operability or that any bug finds will be addressed, though feel free to point any out if you find them.
* Currently only read operations are provided by the module; there are no options to modify the contents of the FAT volume. This means that no files or directories nor any of their property fields can be created or modified, nor can any FAT parameters be modified (i.e. boot sector/BPB, FAT, FSInfo, etc...).
* Though the AVR-FAT module is designed to operate independent of the physical disk layer as long as the required interfacing functions are implemented correctly, this module has only been tested using FAT32-formatted 2 and 4GB SD Cards using the AVR-SDCard module as the physical disk layer.
* This module was created by referencing the Microsoft FAT Specification, but there is no guarantee that it conforms to the standard.


## License
[MIT license](https://github.com/Jsfain/AVR-FAT/blob/master/LICENSE)


## Requirements
Mac / Linux - [AVR Toolchain](https://github.com/osx-cross/homebrew-avr)
Windows - Atmel studio