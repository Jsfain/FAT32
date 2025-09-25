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

/*
 * ----------------------------------------------------------------------------
 *                                            PRINT DIRECTORY ENTRIES TO SCREEN
 *                                       
 * Description : Prints a list of file and directory entries within a directory
 *               specified by a FatDir instance, along with any of its fields
 *               requested.
 * 
 * Arguments   : dir        - Pointer to a FatDir instance. This directory's
 *                            entries will be printed to the screen.
 *               entFlds    - Any combination of the FAT ENTRY FIELD FLAGS.
 *                            These specify which entry types, and which of
 *                            their fields, will be printed to the screen.
 *               bpb        - Pointer to the BPB struct instance.
 *
 * Returns     : A FAT Error Flag. If any value other than END_OF_DIRECTORY is
 *               returned then there was an issue.
 *  
 * Notes       : 1) LONG_NAME and/or SHORT_NAME must be passed in the entFlds
 *                  argument. If not, then no entries will be printed.
 *               2) If both LONG_NAME and SHORT_NAME are passed then both
 *                  the short and long names for each entry will be printed.
 *                  For any entry that does not have a long name, the short 
 *                  name is also stored in the long name string of the struct
 *                  and so the short name will effectively be printed twice -
 *                  Once for the long name and once for the short name.
 * ----------------------------------------------------------------------------
 */
uint8_t fat_PrintDir(const FatDir *dir, uint8_t entFlds, const BPB *bpb);

/*
 * ----------------------------------------------------------------------------
 *                                                         PRINT FILE TO SCREEN
 *                                       
 * Description : Prints the contents of any file entry to the screen. 
 * 
 * Arguments   : dir        - Pointer to a FatDir instance. This directory must
 *                            contain the entry for the file to be printed.
 *               fileStr    - Pointer to a string. This is the name of the file
 *                            who's contents will be printed.
 *               bpb        - Pointer to the BPB struct instance.
 *
 * Returns     : FAT Error Flag. If any value other than END_OF_FILE is 
 *               returned, then an issue has occurred.
 *  
 * Notes       : fileStr must be a long name unless a long name for a given
 *               entry does not exist, in which case it must be a short name.
 * ----------------------------------------------------------------------------
 */
uint8_t fat_PrintFile(const FatDir *dir, const char fileStr[], const BPB *bpb);

/*
 *-----------------------------------------------------------------------------
 *                                                         PRINT FAT ERROR FLAG
 * 
 * Description : Prints the FAT Error Flag passed as the arguement. 
 *
 * Arguments   : err   - An error flag returned by one of the FAT functions.
 * 
 * Returns     : void
 * ----------------------------------------------------------------------------
 */
void fat_PrintError(uint8_t err);

#endif // FAT_PRINT_H
