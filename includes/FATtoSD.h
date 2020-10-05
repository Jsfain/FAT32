#ifndef FATTOSD_H
#define FATTOSD_H

#include <avr/io.h>

uint8_t fat_ReadSingleSector( uint32_t address, uint8_t * arr );

uint32_t fat_FindBootSector();

#endif //FATTOSD_H