/*
 * File       : FAT_PRINT.C
 * Version    : 2.0
 * License    : GNU GPLv3
 * Author     : Joshua Fain
 * Copyright (c) 2025
 * 
 * Implementation of FAT_PRINT.H
 */

#include <stdint.h>
#include "prints.h"
#include "fat_bpb.h"

/*
 ******************************************************************************
 *                                   FUNCTIONS   
 ******************************************************************************
 */

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
void fat_PrintErrorBPB(uint8_t err)
{  
  switch(err)
  {
    case BPB_VALID:
      print_Str("BPB_VALID ");
      break;
    case CORRUPT_BPB:
      print_Str("CORRUPT_BPB ");
      break;
    case NOT_BPB:
      print_Str("NOT_BPB ");
      break;
    case INVALID_BYTES_PER_SECTOR:
      print_Str("INVALID_BYTES_PER_SECTOR");
      break;
    case INVALID_SECTORS_PER_CLUSTER:
      print_Str("INVALID_SECTORS_PER_CLUSTER");
      break;
    case BPB_NOT_FOUND:
      print_Str("BPB_NOT_FOUND");
      break;
    case FAILED_READ_BPB:
      print_Str("FAILED_READ_BPB");
      break;
    default:
      print_Str("UNKNOWN_ERROR");
      break;
  }
}
