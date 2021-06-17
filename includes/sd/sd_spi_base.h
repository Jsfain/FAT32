/*
 * File       : SD_SPI_BASE.H
 * Version    : 1.0
 * License    : GNU GPLv3
 * Author     : Joshua Fain
 * Copyright (c) 2020, 2021
 * 
 * SD_SPI_BASE.H provides the basic function declarations and macros used to 
 * access an SD card in SPI mode.
 */

#ifndef SD_SPI_BASE_H
#define SD_SPI_BASE_H

#include "sd_spi_car.h"

/*
 ******************************************************************************
 *                                   MACROS
 ******************************************************************************
 */

// Macros used for the Send Command
#define TX_CMD_BITS     0x40                // Transmit bits (msb = 01)
#define STOP_BIT        0x01                // final bit sent in a cmd/arg

//
// used for timeouts while waiting for expected SD card responses. The value
// does not correspond to a "time" but a "number of attempts".
//
#define TIMEOUT_LIMIT   0xFE  

// dummy token sent via SPI port when waiting for, or initiating a response
#define DMY_TKN         0xFF

// Card Versions
#define VERSION_1       1
#define VERSION_2       2

// Card Types
#define SDHC            1                   // High Cap - block addressable
#define SDSC            0                   // Std. Cap - byte addressable



/* 
 * ----------------------------------------------------------------------------
 *                                                        HOST CAPACITY SUPPORT
 * 
 * Description : Specifies the card type(s) that the host will support.
 *        
 * Notes       : Can be set to either SDHC or SDSC, but recommend setting to 
 *               SDHC, which will also support SDSC. Nothing should be lost by
 *               setting it this way. This is just to inform the card what the 
 *               host can support.
 * ----------------------------------------------------------------------------
 */
#define HOST_CAPACITY_SUPPORT  SDHC

/* 
 * ----------------------------------------------------------------------------
 *                                                                 BLOCK LENGTH
 *
 * Description : The SD card block length (in bytes) assumed by the host. 
 *       
 * Warning     : This should always be set to 512. If not, the implementation 
 *               should be unexpected to produce unexpected results and/or fail.
 * ----------------------------------------------------------------------------
 */
#define BLOCK_LEN       512

/* 
 * ----------------------------------------------------------------------------
 *                                                                  CHIP SELECT
 *
 * Description : defines the chip select pin to use for the device's SPI port.
 * 
 * Notes       : (1) These definitions depend on the target/host being used.
 * ----------------------------------------------------------------------------
 */
// define CS port, pin, and data direction pin
#define CS_SD_PORT      SPI_PORT
#define CS_SD_PIN       SS
#define CS_SD_DDR       DDR_SPI
#define CS_SD_DD        DD_SS

#define CS_SD_LOW    CS_SD_PORT = (CS_SD_PORT & ~(1 << CS_SD_PIN)) // assert
#define CS_SD_HIGH   CS_SD_PORT |=  (1 << CS_SD_PIN)               // deassert

/* 
 * ----------------------------------------------------------------------------
 *                                                   INITIALIZATION ERROR FLAGS
 * 
 * Description : Flags returned by sd_InitModeSPI.
 *        
 * Notes       : The lowest byte returned by sd_InitModeSPI is the most recent
 *               R1 Response.
 * ----------------------------------------------------------------------------
 */
#define FAILED_GO_IDLE_STATE    0x00100     // CMD0 error
#define FAILED_SEND_IF_COND     0x00200     // CMD8 error
#define UNSUPPORTED_CARD_TYPE   0x00400     // CMD8 and CMD58 error
#define FAILED_CRC_ON_OFF       0x00800     // CMD59 error
#define FAILED_APP_CMD          0x01000     // CMD55 error
#define FAILED_SD_SEND_OP_COND  0x02000     // ACMD41 error
#define OUT_OF_IDLE_TIMEOUT     0x04000     // ACMD41 error
#define FAILED_READ_OCR         0x08000     // CMD58 error
#define POWER_UP_NOT_COMPLETE   0x10000     // CMD58 error


/*
 ******************************************************************************
 *                      OPERATION CONDITIONS REGISTER (OCR)
 ******************************************************************************
 */
#define POWER_UP_BIT_MASK     0x80
#define CCS_BIT_MASK          0x40          // Card Capacity Support
#define UHSII_BIT_MASK        0x20          // UHS-II Card Status
#define CO2T_BIT_MASK         0x10          // Over 2TB support status
#define S18A_BIT_MASK         0x08          // switching to 1.8V accepted

// Volt Range Accepted by card: 2.7 - 3.6V. Only this range currently supported
#define VRA_OCR_MASK          0xFF80

/*
 ******************************************************************************
 *                                   STRUCTS
 ******************************************************************************
 */

/* 
 * ----------------------------------------------------------------------------
 *                                                        CARD TYPE and VERSION
 * 
 * Members  : 1) version - SD Card's version. Either 1 or 2.
 *            2) type    - SD Card's type. Either SDSC or SDHC.
 * 
 * Notes    : 1) The members of this struct should only be set by passing a 
 *               pointer of an instance to the init routine, sd_InitModeSPI.
 *            2) The value of 'type' is necessary for determining how a card's 
 *               blocks are addressed.
 * 
 * Warnings : Only version 2 cards have been tested.
 * ----------------------------------------------------------------------------
 */
typedef struct CardTypeVersion{
    uint8_t version;
    uint8_t type;
} CTV;

/*
 ******************************************************************************
 *                           FUNCTION PROTOTYPES
 ******************************************************************************
 */

/*
 * ----------------------------------------------------------------------------
 *                                                       SD CARD INITIALIZATION
 *
 * Description : Implements the SD Card SPI mode initialization routine and 
 *               sets the members of the CTV (Card Type and Version) struct
 *               instance. 
 *
 * Arguments   : ctv   - ptr to a CTV instance whose members will be set here.
 * 
 * Returns     : Initialization Error Response. This includes an Initialization
 *               Error Flag in bits 8 to 19 and the most recent R1 response in
 *               the lowest byte.
 * 
 * Warnings    : A CTV instance should ONLY be set by this function.
 * ----------------------------------------------------------------------------
 */
uint32_t sd_InitModeSPI(CTV *ctv);

/*
 * ----------------------------------------------------------------------------
 *                                                                    SEND BYTE
 * 
 * Description : Sends a single 8-bit byte to the SD card via the SPI port.
 * 
 * Arguments   : byte   - 8-bit byte to be sent to the SD Card via SPI.
 * 
 * Notes       : 1) Call as many times as required to send the complete data 
 *                  packet, token, command, etc...
 *               2) This function and sd_ReceiveByteSPI(), are the only direct
 *                  SPI interfacing functions in the SD card module.
 * ----------------------------------------------------------------------------
 */
void sd_SendByteSPI(uint8_t byte);

/*
 * ----------------------------------------------------------------------------
 *                                                                 RECEIVE BYTE
 * 
 * Description : Receives and returns single byte from the SD card via SPI.
 * 
 * Returns     : 8-bit byte received from the SD card.
 * 
 * Notes       : 1) Call as many times as necessary to get the complete data
 *                  packet, token, error response, etc... from the SD card.
 *               2) This function and sd_SendByteSPI(), are the only direct
 *                  SPI interfacing functions in the SD card module.
 * ----------------------------------------------------------------------------
 */
uint8_t sd_ReceiveByteSPI(void);

/*
 * ----------------------------------------------------------------------------
 *                                                                 SEND COMMAND
 * 
 * Description : Send a command and argument to the SD Card.
 * 
 * Arguments   : cmd   - SD Card command. See sd_spi_car.h.
 *               arg   - 32-bit argument to be sent with the SD command.
 * ----------------------------------------------------------------------------
 */
void sd_SendCommand(uint8_t cmd, uint32_t arg);

/*
 * ----------------------------------------------------------------------------
 *                                                              GET R1 RESPONSE
 * 
 * Description : Gets the R1 response from the SD card after it has been sent 
 *               an SD command.
 * 
 * Returns     : R1 response flag(s). See SD_SPI_BASE.H.
 * 
 * Notes       : 1) always call immediately after sd_SendCommand().
 *               2) pass the return value to sd_PrintR1() to print R1 response.
 *               3) if R1_TIMEOUT is returned, then the SD Card did not return
 *                  a response.
 * ----------------------------------------------------------------------------
 */
uint8_t sd_GetR1(void);

/*
 * ----------------------------------------------------------------------------
 *                                                      PRINT R1 RESPONSE FLAGS
 * 
 * Description : Prints the R1 response flag(s) returned by sd_GetR1().
 * 
 * Arguments   : r1   - The R1 response flag(s) byte returned by sd_GetR1().
 * ----------------------------------------------------------------------------
 */
void sd_PrintR1(uint8_t r1);

/*
 * ----------------------------------------------------------------------------
 *                                          PRINT INITIALIZATION RESPONSE FLAGS
 * 
 * Description : Prints Initialization Error Flag portion of the response 
 *               returned by sd_InitModeSPI.
 * 
 * Arguments   : initResp   - The Initialization Error Response returned by the
 *                            initialization routine, sd_InitModeSPI.
 * 
 * Notes       : This will only interpret bits 8 to 19 of the sd_InitModeSPI 
 *               function's returned value. Though the entire returned value
 *               can be passed to the function, bits 0 to 7 will be ignored
 *               as these are the R1 Response portion of the Initialization
 *               Response. To read the R1 portion of initResp, pass it to 
 *               sd_PrintR1().
 * ----------------------------------------------------------------------------
 */
void sd_PrintInitError(uint32_t initErr);

/*
 * ----------------------------------------------------------------------------
 *                                              WAIT SPECIFIED SPI CLOCK CYCLES
 * 
 * Description : Used to wait a specified number of SPI clock cycles. While
 *               doing so, it sends all 1's (DMY_TKN) on the SPI port.
 * 
 * Arguments   : clckCycles   - min num of clock cycles to wait.
 * ----------------------------------------------------------------------------
 */
void sd_WaitSendDummySPI(uint16_t clckCycles);

#endif //SD_SPI_BASE_H
