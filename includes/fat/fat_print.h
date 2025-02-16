/*
 * File       : FAT_PRINT.H
 * Version    : 2.0
 * License    : GNU GPLv3
 * Author     : Joshua Fain
 * Copyright (c) 2020 - 2025
 * 
 * Provides printing functions for the FAT module.
 */

#ifndef FAT_PRINT_H
#define FAT_PRINT_H

/*
 * ----------------------------------------------------------------------------
 *                                       PRINT BIOS PARAMETER BLOCK ERROR FLAGS 
 * 
 * Description : Print the Bios Parameter Block Error Flag. 
 * 
 * Arguments   : err     BPB error flag(s).
 * 
 * Returns     : void
 * ----------------------------------------------------------------------------
 */
void fat_PrintErrorBPB(uint8_t err);

#endif // FAT_PRINT_H
