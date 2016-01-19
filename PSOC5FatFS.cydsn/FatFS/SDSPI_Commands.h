#ifndef SDSPI_COMMANDS_H
#define SDSPI_COMMANDS_H
    
/* SPI Commands for SD/MMC communications necessary for the use of FATFS */

#define SDCMD_APP_SPECIFIC_FLAG     0x80        // MSB in a command is set to indicate that the cmd to send is an application
                                                //   specific command (e.g. must be preceeded by CMD55)
#define Is_AppSpecificCmd(cmd)      (cmd & SDCMD_APP_SPECIFIC_FLAG) 

    
/* Resets all cards to idle state */
#define CMD0	(0)			/* GO_IDLE_STATE */
#define SDCMD_GO_IDLE_STATE     (0)         /* Resets all cards to idle state */


/* Used to distinguish beteeen SD/MMC */
/* 2.1mm SD Memory Card can be initialized using CMD1 and Thin (1.4mm) SD Memory Card can be initialized using CMD1 only after
firstly initialized by using CMD0 and ACMD41. In any of the cases CMD1 is not recommended because it may be difficult for the host
to distinguish between MultiMediaCard and SD Memory Card.
If the SD card is initialized by CMD1 and the host treat it as MMC card, not SD card, the Data of the card may be damaged because of
wrong interpretation of CSD and CID registers */
/* For the Thick (2.1 mm) SD Memory Card - CMD1 (SEND_OP_COND) is also valid - this means that in 
SPI mode, CMD1 and ACMD41 have the same behaviors, but the usage of ACMD41 is preferable since 
it allows easy distinction between an SD Memory Card and a MultiMediaCard. For the Thin (1.4 mm) 
Standard Size SD Memory Card, CMD1 (SEND_OP_COND) is an illegal command during the 
initialization that is done after power on. After Power On, once the card has accepted valid 
ACMD41, it will be able to also accept CMD1 even if used after re-initializing (CMD0) the card. It 
was defined in such way in order to be able to distinguish between a Thin SD Memory Card and a 
MultiMediaCard (that supports CMD1 as well).
*/
#define CMD1	(1)			/* SEND_OP_COND */
#define SDCMD_SEND_OP_COND       (1)
#define	ACMD41	(SDCMD_APP_SPECIFIC_FLAG | 41)	/* SD_SEND_OP_COND (SDC) */
#define SDCMD_SD_SEND_OP_COND   (SDCMD_APP_SPECIFIC_FLAG | 41)


/* Sends SD Memory Card interface condition, which includes host supply 
voltage information and asks the card whether card supports voltage. 
Reserved bits shall be set to '0'.*/
#define CMD8	(8)			/* SEND_IF_COND */
#define SDCMD_SEND_IF_COND      (8)         

/*Addressed card sends its card-specific 
data (CSD) on the CMD line */
#define CMD9	(9)			/* SEND_CSD */
#define SDCMD_SEND_CSD          (9)

/* Addressed card sends its card identification (CID) on CMD the line. */
#define CMD10	(10)		/* SEND_CID */
#define SDCMD_SEND_CID          (10)

/* Forces the card to stop transmission */
#define CMD12	(12)		/* STOP_TRANSMISSION */
#define SDCMD_STOP_TRANSMISSION     (12)

/* Addressed card sends its status register. */
#define CMD13	(13)		/* SEND_STATUS */
#define SDCMD_SEND_STATUS       (13)

/* Send the SD Status. The status fields  are given in Table 4-43. */
#define ACMD13	(SDCMD_APP_SPECIFIC_FLAG | 13)	/* SD_STATUS (SDC) */
#define SDCMD_SD_STATUS         (SDCMD_APP_SPECIFIC_FLAG | 13)


/* In the case of a Standard Capacity SD Memory Card, this command sets the 
block length (in bytes) for all following block commands (read, write, lock). 
Default block length is fixed to 512 Bytes. Set length is valid for memory 
access commands only if partial block read operation are allowed in CSD.
In the case of SDHC and SDXC Cards, block length set by CMD16 command 
does not affect memory read and write commands. Always 512 Bytes fixed 
block length is used. This command is effective for LOCK_UNLOCK command.
In both cases, if block length is set larger than 512Bytes, the card sets the 
BLOCK_LEN_ERROR bit.  In DDR50 mode, data is sampled on both edges of the clock. Therefore, block 
length shall always be even. */
#define CMD16	(16)		/* SET_BLOCKLEN */
#define SDCMD_SET_BLOCKLEN      (16)

/* In the case of a Standard Capacity SD Memory Card, this command, this 
command reads a block of the size selected by the SET_BLOCKLEN 
command.
In case of SDHC and SDXC Cards, block length is fixed 512 Bytes 
regardless of the SET_BLOCKLEN command.
*/
#define CMD17	(17)		/* READ_SINGLE_BLOCK */
#define SDCMD_READ_SINGLE_BLOCK     (17)

/* Continuously transfers data blocks from card to host until interrupted by a 
STOP_TRANSMISSION command. Block length is specified the same as 
READ_SINGLE_BLOCK command. */
#define CMD18	(18)		/* READ_MULTIPLE_BLOCK */
#define SDCMD_READ_MULTIPLE_BLOCK   (18)

/* Specify block count for CMD18 and CMD25. */
#define CMD23	(23)		/* SET_BLOCK_COUNT */
#define SDCMD_SET_BLOCK_COUNT       (23)

/* Set the number of write blocks to be pre-erased before writing (to be used 
for faster Multiple Block WR command). "1"=default (one wr block) 
Command STOP_TRAN (CMD12) shall be used to stop the transmission in Write Multiple
Block whether or not the preerase (ACMD23) feature is used. */
#define	ACMD23	(SDCMD_APP_SPECIFIC_FLAG | 23)	/* SET_WR_BLK_ERASE_COUNT (SDC) */
#define SDCMD_SET_WR_BLK_ERASE_CNT  (SDCMD_APP_SPECIFIC_FLAG | 23)

/* In case of SDSC Card, block length is set by the SET_BLOCKLEN command1. 
In case of SDHC and SDXC Cards, block length is fixed 512 Bytes 
regardless of the SET_BLOCKLEN command. */
#define CMD24	(24)		/* WRITE_BLOCK */
#define SDCMD_WRITE_BLOCK           (24)

/* Continuously writes blocks of data until a STOP_TRANSMISSION follows.
Block length is specified the same as WRITE_BLOCK command. */
#define CMD25	(25)		/* WRITE_MULTIPLE_BLOCK */
#define SDCMD_WRITE_MULTIPLE_BLOCK  (25)

/* Sets the address of the first write block to be erased. */
#define CMD32	(32)		/* ERASE_WR_BLK_START */
#define SDCMD_ERASE_WR_BLK_START        (32)

/* Sets the address of the last write block of the continuous range to be 
erased. */
#define CMD33	(33)		/* ERASE_WR_BLK_END */
#define SDCMD_ERASE_WR_BLK_END          (33)

/* Erases all previously selected write blocks. */
#define CMD38	(38)		/* ERASE */
#define SDCMD_ERASE             (38)

/* Indicates to the card that the next command is an application specific 
command rather than a standard command */
#define CMD55	(55)		/* APP_CMD */
#define SDCMD_APP_CMD           (55)

/* Multi-block read type. Refer to Section 5.7.2.4. */
#define CMD58	(58)		/* READ_EXTR_MULTI */
#define SDCMD_READ_EXTR_MULTI      (58)

typedef enum {
    GO_IDLE_STATE_Cmd0   = CMD0,
    SEND_OP_COND_Cmd1    = CMD1,
    SD_SEND_OP_COND_ACmd41 = ACMD41,
    SEND_IF_COND_Cmd8 = CMD8,
    SEND_CSD_Cmd9 = CMD9,
    SEND_CID_Cmd10 = CMD10,
    STOP_TRANSMISSION_Cmd12 = CMD12,
    SEND_STATUS_Cmd13 = CMD13,
    SET_BLOCKLEN_Cmd16 = CMD16,
    READ_SINGLE_BLOCK_Cmd17 = CMD17,
    READ_MULTIPLE_BLOCK_Cmd18 = CMD18,
    SET_BLOCK_COUNT_Cmd23 = CMD23,
    SET_WR_BLK_ERASE_CNT_ACmd23 = ACMD23,
    WRITE_BLOCK_Cmd24 = CMD24,
    WRITE_MULTIPLE_BLOCK_Cmd25 = CMD25,
    ERASE_WR_BLK_START_Cmd32 = CMD32,
    ERASE_WR_BLK_END_Cmd33 = CMD33,
    ERASE_Cmd38 = CMD38,
    APP_CMD_Cmd55 = CMD55,
    READ_EXTR_MULTI_Cmd58 = CMD58,
    SD_STATUS_ACmd13    = ACMD13
    
} SDCardCmd_t;    
    

#define SD_STOP_TRANS_TOKEN     0xFD
#define SD_DATA_START_TOKEN     0xFE
#define SD_DATA_MULTI_BLK_WRITE_TOKEN  0xFC
#define SD_DATA_IDLE            0xFF

#define SD_RESP_DATA_ACCEPTED   0x05
#define SD_RESP_DATA_CRC_ERR    0x0B
#define SD_RESP_DATA_WRITE_ERR  0x0D

// an R1 response with no errors
#define R1_RESPONSE_OK          0x00
#define R1_RESPONSE_IDLE        0x01

// used in CMD8 to indicate we support high capacity cards
#define HOST_CAPACITY_SUPPORT   (1UL << 30)

// flag for last byte (of 4) in OCR register indicating if the card is high capacity
#define CARD_CAPACITY_SUPPORT_FLAG  0x40

// a valid response (error or non-error alike) will have a 0 in the MSB
#define Is_ValidR1Response(resp)    (!(resp & 0x80))


#define Is_DiskUninitialized(drive)       (disk_status(drv) & STA_NOINIT)
    
#define DISK_STATUS_OK          0x00

#endif
