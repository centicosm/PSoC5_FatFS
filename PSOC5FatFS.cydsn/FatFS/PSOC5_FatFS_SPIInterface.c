#include <project.h>
#include <cytypes.h>
#include <stdbool.h>
#include "FatFS/diskio.h"
#include "FatFS/SDSPI_Commands.h"
#include "FatFS/FatFS_PrettyMacros.h"
#include "FatFSCmdInterface.h"


// used to track the status of the disk
static FatFS_DiskStatus_t _DiskStatus = STA_NOINIT;	


static uint8_t _CardType;			/* 0000 (Block) (SD2) (SD1) (MMC) */
#define Is_CardTypeBlock()        (_CardType & CARDTYPE_BLOCK)
#define Is_CardTypeSD2()          (_CardType & CARDTYPE_SD2)
#define Is_CardTypeSD1()          (_CardType & CARDTYPE_SD1)
#define Is_CardTypeSDC()          (_CardType & CARDTYPE_SDC)

#define SDSPI_DUMMY_BYTE                0xFF
#define Does_SdspiRxFifoHaveData()      (SDSPI_RX_STATUS_REG & SDSPI_STS_RX_FIFO_NOT_EMPTY)


// A convenience function to perform a blocking exchange of a byte of data over SPI
uint8_t SDSPI_ExchangeByte(uint8_t data) {
    SDSPI_WriteByte(data);
    while (!Does_SdspiRxFifoHaveData()) {};
    return SDSPI_ReadRxData();    
}


// Check if the sd card is ready, if not, wait for a period of time for it to become ready
//   timeOut is in increments of 100us
static bool Is_CardReady(uint32_t timeOut) {
    uint8_t statusByte;

    // keep checking if the card becomes active over the timeout perod
    for (/*timeout*/; timeOut != 0; timeOut--) {	
        
        // query the sd card by sending a dummy byte
        statusByte = SDSPI_ExchangeByte(SDSPI_DUMMY_BYTE);
        
        // 0xFF response indicates that the slave pulled MISO high and is active
        if (statusByte == SD_DATA_IDLE) return true;
        CyDelayUs(100);
    }

    // we timed out without the slave pulling MISO high
    return false;
}



// Deselect the SD Card
static void Release_SDCard(void) {

    SS_Write(1);
    
    // write a dummy byte so the card releases MISO in case there are multiple slaves on the bus
    SDSPI_WriteTxData(SDSPI_DUMMY_BYTE); 
}


// Assert SS on the SD card and wait for it to 
static bool Select_SDCard(void)	{
    SS_Write(0);
 
    // write a dummy byte so the card acquires MISO in case there are multiple slaves on the bus
    SDSPI_ExchangeByte(SDSPI_DUMMY_BYTE);
    
    // Check if the card is ready
    if (Is_CardReady(5000)) return true;

    // if not, release is SS
    Release_SDCard();
    return false;	
}




// send a buffer of the specified size to the card
static void Send_BufferToSDCard(const uint8_t *buf, uint32_t size) {
        uint32_t index = 0;
           
        // throw the contents of our buffer into the SPI tx
        while (size) {
            SDSPI_WriteTxData(buf[index++]);
            --size;
        }
        
        // wait for the transmit to complete
        while (!(SDSPI_ReadTxStatus() & SDSPI_STS_SPI_DONE)) {};
}


// clock out size bytes of data from the sd card
static void Receive_DataBuf(uint8_t *buf, uint32_t size) {
    uint32_t i = 0;
    
    SDSPI_ClearRxBuffer();
    
    do {
        buf[i++] = SDSPI_ExchangeByte(SDSPI_DUMMY_BYTE);
    } while (--size);    
}


// receive a data block of a given size and store it in buf
//  this is called when we we are doing a read and
//  need to check for the SD_DATA_START_TOKEN and expect a CRC
// this is usually done on a read block/multiblock command
// but also happens with the read CSD and read CID commands
static bool Receive_DataBlock(uint8_t *buf, uint32_t size) {
    uint8_t token;
    
    // poll the sd card for a data start token
    for (uint16_t retries = 1000; retries != 0; retries--) {	
        
        token = SDSPI_ExchangeByte(SDSPI_DUMMY_BYTE);
        
        // as soon as we get a non-idle token, leave the loop
        if (token != SD_DATA_IDLE) break;
        CyDelayUs(25);
    }
    
    // make sure the token we received is the data block start token
    if (token != SD_DATA_START_TOKEN) return false;		

    // Now clock out the data
    Receive_DataBuf(buf, size);
    
    // throw away the crc
    SDSPI_ExchangeByte(SDSPI_DUMMY_BYTE);
    SDSPI_ExchangeByte(SDSPI_DUMMY_BYTE);
    
    return true;
}



// send a block of 512 bytes to the sd card
static bool Write_DataBlock(const uint8_t *buf, uint8_t token) {
    
    if (!Is_CardReady(5000)) return 0;

    SDSPI_ExchangeByte(token);

    Send_BufferToSDCard(buf, 512);	

    SDSPI_ClearRxBuffer();

    // send dummy CRC bytes
    SDSPI_ExchangeByte(SDSPI_DUMMY_BYTE);
    SDSPI_ExchangeByte(SDSPI_DUMMY_BYTE);
        
    // now wait for the write to be accepted
    uint32_t writeWait = 10000;
    do {
        uint8_t response = SDSPI_ExchangeByte(SDSPI_DUMMY_BYTE);
            
        // if we get a write accepted response, return success
        if ((response & 0x1F) == SD_RESP_DATA_ACCEPTED) {
            return true;
        }
    } while (--writeWait);
        
    // otherwise we got no response or SD_RESP_DATA_CRC_ERR or SD_RESP_DATA_WRITE_ERR
    //Print_ToUSBUart("Failed to accept write block\n");
    return false;
}





// send a command with arguments to the sd card and return the response
//  generally the value returned for all the commands used will be an R1 type
//  response (as the first byte of R3 and R7 responses is a R1 response).
//  Any additional response bytes are handled by the caller
static uint8_t Send_SDCmd(SDCardCmd_t cmd,	uint32_t cmdArg) {		
    uint8_t cmdBuf[6];

    // handle the case we are sending an app specific command
    if (Is_AppSpecificCmd(cmd)) {
                
        // send the application specific command preamble
        uint8_t cmdResp = Send_SDCmd(APP_CMD_Cmd55, 0);
        if (cmdResp > 1) return cmdResp;
        
        // And clear the app specific bit flag
        cmd &= ~(SDCMD_APP_SPECIFIC_FLAG);
    }

    // Select the card and wait for ready except to stop multiple block read
    if (cmd != STOP_TRANSMISSION_Cmd12) {
        Release_SDCard();
        if (!Select_SDCard()) return 0xFF;
    }

    // Build and send the command packet
    cmdBuf[0] = 0x40 | cmd;     			    /* Start + Command index */
    cmdBuf[1] = (uint8_t)(cmdArg >> 24);		/* Argument[31..24] */
    cmdBuf[2] = (uint8_t)(cmdArg >> 16);		/* Argument[23..16] */
    cmdBuf[3] = (uint8_t)(cmdArg >> 8);		    /* Argument[15..8] */
    cmdBuf[4] = (uint8_t)cmdArg;				/* Argument[7..0] */
    cmdBuf[5]  = 0x01;						    /* Dummy CRC + Stop */
    if (cmd == GO_IDLE_STATE_Cmd0) {
        cmdBuf[5]  = 0x95;		// CRC for CMD0 
    }
    else if (cmd == SEND_IF_COND_Cmd8) {
        cmdBuf[5]  = 0x87;		//  CRC for CMD8
    }
    Send_BufferToSDCard(cmdBuf, 6);
    
    // Receive command response 
    if (cmd == STOP_TRANSMISSION_Cmd12) {
        //  as the card has preloaded the next byte of data already, there is a dummy byte in the sd card fifo we need to read
        SDSPI_ExchangeByte(SDSPI_DUMMY_BYTE);	
    }

    uint8_t response;
    uint8_t retries = 25;
    
    // clear out the bytes from the cmd transfer
    SDSPI_ClearRxBuffer();   
    do {
        response = SDSPI_ExchangeByte(SDSPI_DUMMY_BYTE);
        
    } while (!Is_ValidR1Response(response) && --retries);

    // 0 means it is a valid command, anything else means some error occured (defined by the error flag set in the spec)
    return response;
    
}


/*--------------------------------------------------------------------------
   Public Functions
---------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------*/
/* Get Disk Status - only drive 0 supported                              */
/*-----------------------------------------------------------------------*/
FatFS_DiskStatus_t disk_status(uint8_t drv) {
    if (drv) return STA_NOINIT;

    return _DiskStatus;
}

/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive                                                 */
/*-----------------------------------------------------------------------*/
FatFS_DiskStatus_t disk_initialize(uint8_t drv) {
    uint8_t buf[4], cmd;
    uint32_t retries;


    uint8_t cardType = CARDTYPE_UNDEFINED;
    
    // only drive 0 is supported
    if (drv) return RES_NOTRDY;

    CyDelay(10);

    // dummy clocks to prepare card
    for (uint8_t i = 0; i < 10; i++) {
        SDSPI_ExchangeByte(SDSPI_DUMMY_BYTE);
    }
    
    // tell the card to go to the idle state as the first step in initialization
    if (Send_SDCmd(GO_IDLE_STATE_Cmd0, 0) == R1_RESPONSE_IDLE) {
        
        // Cmd8 actually responds with a R7 response (5 bytes).  The first byte, however, is the same as an R1 response
        //   Send the command with an echo byte of 0xAA and check if it supports 2.7-3.6V
        //   If we receive an idle response, it is a v2 sd card so initialize it as such
        if (Send_SDCmd(SEND_IF_COND_Cmd8, 0x1AA) == R1_RESPONSE_IDLE) {	
            Receive_DataBuf(buf, 4);    // grab the rest of the R7 response
            if ((buf[2] == 0x01) && (buf[3] == 0xAA)) {		// verify that the response indicates the card can operate at 2.7-3.6V and that the echo byte supplied matches
                
                // now send the command indicating that we support SDHC cards and wait for the card to leave the idle state
                for (retries = 1000; (retries != 0); retries--) {
                    if (Send_SDCmd(SD_SEND_OP_COND_ACmd41, HOST_CAPACITY_SUPPORT) == R1_RESPONSE_OK) break;
                    CyDelay(1);
                }
                
                // if the card has left idle, grab the operations conditions register (OCR) with CMD58
                //   CMD58 has a R3 response of 5 bytes with byte one being equiv to an R1 response
                if ((retries != 0) && Send_SDCmd(READ_EXTR_MULTI_Cmd58, 0) == R1_RESPONSE_OK) {
                    
                    Receive_DataBuf(buf, 4);  // grab the rest of the R3 response
                    
                    // OCR reg is in buf, transmitted MSB first
                    // check if the CCS flag is set
                    // CCS=0 means that the card is SDSC. CCS=1 means that the card is SDHC or SDXC.
                    if (buf[0] & CARD_CAPACITY_SUPPORT_FLAG) {
                        cardType = CARDTYPE_SD2 | CARDTYPE_BLOCK;
                    }
                    else {
                         cardType = CARDTYPE_SD2;
                    }
                }
            }
        } 
        
        // CMD8 did not return a valid response so try to initialize as a non V2 card
        else {
            
            // try to initialize (while indicating that we do not support high capacity)
            uint8_t response = Send_SDCmd(SD_SEND_OP_COND_ACmd41, 0); 
            
            // A sd v1 card will respond to the initialization request as either idle or command accepted
            if ((response == R1_RESPONSE_OK) || (response == R1_RESPONSE_IDLE)) {
                cardType = CARDTYPE_SD1; 
                cmd = SD_SEND_OP_COND_ACmd41;       // the ok response may not be immediate, to cover that case, we will repeat 
            } 
            
            // an improper response means that it may be an MMC card which requires sending CMD1 to initialize
            else {
                cardType = CARDTYPE_MMC; 
                cmd = SEND_OP_COND_Cmd1;
            }
            
            // send our initialization command until we get an ok response indicating everything is initialized
            for (retries = 1000; (retries != 0); retries--) {
                if (Send_SDCmd(cmd, 0) == R1_RESPONSE_OK) break;
                CyDelay(1);
            }

            // if we timed out waiting for initialization, we mark the card type as unknown
            if (retries == 0) {
                cardType = CARDTYPE_UNDEFINED;
            }
            
            // otherwise, send the command to set the block length to 512
            else {
                if (Send_SDCmd(SET_BLOCKLEN_Cmd16, 512) != R1_RESPONSE_OK) {
                    cardType = CARDTYPE_UNDEFINED;
                }
            }
        }
    }
    
    _CardType = cardType;
    if (_CardType == CARDTYPE_UNDEFINED) {
        _DiskStatus = STA_NOINIT;
    }
    else {
        _DiskStatus = DISK_STATUS_OK;
    }

    Release_SDCard();

    return _DiskStatus;
}


/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/
FatFS_DiskOpResult_t disk_read(uint8_t drv, uint8_t *buf,	uint32_t sector, uint32_t blockCount) {
    SDCardCmd_t cmd;

    // uninitialized disks tell no tales
    if (Is_DiskUninitialized(drv)) return RES_NOTRDY;
    
     //covert sector number to byte number if we are using a block card
    if (!Is_CardTypeBlock()) sector *= 512;

    // determine if we need to use a single or multiblock read command
    if (blockCount > 1) {
        cmd = READ_MULTIPLE_BLOCK_Cmd18;
    }
    else {
        cmd = READ_SINGLE_BLOCK_Cmd17;
    }
    
    // send the read command to the card
    if (Send_SDCmd(cmd, sector) == R1_RESPONSE_OK) {
        
        // and now grab the data blocks
        do {
            if (!Receive_DataBlock(buf, 512)) break;
            buf += 512;
        } while (--blockCount);
        
        // if we are at the end of a multiblock transmission, send the stop transmission command too
        if (cmd == READ_MULTIPLE_BLOCK_Cmd18) {
            Send_SDCmd(STOP_TRANSMISSION_Cmd12, 0);	
        }
    }
    
    Release_SDCard();

    // if we were unable to read all our blocks successfully, return an error
    if (blockCount != 0) return RES_ERROR;
    
    // otherwise the read was successful
    return RES_OK;
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/
FatFS_DiskOpResult_t disk_write(uint8_t drv, const uint8_t *buf, uint32_t sector, uint32_t numBlocks) {

    // uninitialized disks tell no tales
    if (Is_DiskUninitialized(drv)) return RES_NOTRDY;
    
    //covert sector number to byte number if we are using a block card
    if (!Is_CardTypeBlock()) sector *= 512;

    // if we are writing a single block
    if (numBlocks == 1) {
        
        // send the write block command
        if (Send_SDCmd(WRITE_BLOCK_Cmd24, sector) == R1_RESPONSE_OK) {
            
            // and write the data if we received the command accepted response
            if (Write_DataBlock(buf, SD_DATA_START_TOKEN)) {
                numBlocks = 0; // no blocks remain
            }
        }
    }
    
    // otherwise we are sending multiple blocks to be written
    else {
        
        // SDC cards can preerase the blocks for better performance
        if (Is_CardTypeSDC()) {
            Send_SDCmd(SET_WR_BLK_ERASE_CNT_ACmd23, numBlocks);
        }
        
        // now perform the actual multiblock write
        if (Send_SDCmd(WRITE_MULTIPLE_BLOCK_Cmd25, sector) == R1_RESPONSE_OK) {
            do {
                if (!Write_DataBlock(buf, SD_DATA_MULTI_BLK_WRITE_TOKEN)) break;
                buf += 512;
            } while (--numBlocks);
                        
            // Finalize the multi-block write
            // if the card is not ready after the last block write, at least one block remains unwritten
            if (!Is_CardReady(5000)) {
                numBlocks = 1;
            }
            // otherwise send the stop token
            else {
                SDSPI_ExchangeByte(SD_STOP_TRANS_TOKEN);
            }
        }
    }
    Release_SDCard();

    // no blocks remain unwritten so return success
    if (numBlocks == 0) return RES_OK;
    
    //otherwise, one or more failed
    return RES_ERROR;
}


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/
FatFS_DiskOpResult_t disk_ioctl (uint8_t drv, uint8_t ctrlCode,	void *buf) {
    FatFS_DiskOpResult_t res;
    uint8_t n, csd[16];

    if (Is_DiskUninitialized(drv)) return RES_NOTRDY;

    res = RES_ERROR;
    switch (ctrlCode) {
        
        // make sure card is in ready state
        case CTRL_SYNC :
            if (Select_SDCard()) res = RES_OK;
            break;
        
        // return the number of sectors on the disk, calculated based on the contents of the CSD register
        case GET_SECTOR_COUNT :
            if (Send_SDCmd(SEND_CSD_Cmd9, 0) == 0) {
                if (Receive_DataBlock(csd, 16)) {
                    
                    // check the CSD structure version to determine the type of card, 1 indidicates High Capacity and Extended Capacity
                    //   0 indicates standard capacity
                    if ((csd[0] >> 6) == 1) {
                        
                        // determine the number of sectors based on the csize field in the CSD v2
                        uint32_t csize = csd[9] + ((uint32_t)csd[8] << 8) + 1;
                        *(uint32_t *)buf = csize << 10;
                    }
                    
                    // determine the number of sectors based on the fields in the CSD v1
                    else {
                        n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
                        uint32_t csize = (csd[8] >> 6) + ((uint32_t)csd[7] << 2) + ((uint32_t)(csd[6] & 3) << 10) + 1;
                        *(uint32_t *)buf = csize << (n - 9);
                    }
                    res = RES_OK;
                }
            }
            break;

            

#if (_MAX_SS != _MIN_SS)
#error Implementation does not support variable sector sizes
#endif
        case GET_SECTOR_SIZE:
            *(WORD*)buf = 512;
            res = RES_OK;
            break;
            
            
        // get the number of sectors per allocation unit
        case GET_BLOCK_SIZE :

            // case where it is a v2 sd card
            if (Is_CardTypeSD2()) {
                if (Send_SDCmd(SD_STATUS_ACmd13, 0) == R1_RESPONSE_OK) {
                    SDSPI_ExchangeByte(SDSPI_DUMMY_BYTE);
                    if (Receive_DataBlock(csd, 16)) {
                        
                        // the actual response is 64 bytes so toss the remainder
                        for (n = 64 - 16; n; n--) SDSPI_ExchangeByte(SDSPI_DUMMY_BYTE);
                        *(uint32_t*)buf = 16UL << (csd[10] >> 4);       // csd[10] 7-4   hold the allocation unit size with 0 = invalid
                                                                        //   the AU size is 16k * 2^(AU_Field - 1).  Thus, with 512 byte sectors,
                                                                        //      the number of sectors per AU is 16 * 2^AU_Field
                        res = RES_OK;
                    }
                }
            }
            
            // SD v1 or MMC
            else {			
                if (Send_SDCmd(SEND_CSD_Cmd9, R1_RESPONSE_OK) == 0) {
                    if (Receive_DataBlock(csd, 16)) {
                        
                        // each of these calculations relies on values found in the CSD register described in the spec
                        if (Is_CardTypeSD1()) {
                            *(uint32_t *)buf = (((csd[10] & 63) << 1) + ((uint16_t)(csd[11] & 128) >> 7) + 1) << ((csd[13] >> 6) - 1);
                        } 
                        else {				
                            *(uint32_t *)buf = ((uint16_t)((csd[10] & 124) >> 2) + 1) * (((csd[11] & 3) << 3) + ((csd[11] & 224) >> 5) + 1);
                        }
                    }
                    res = RES_OK;
                }
            }
            break;

                
        default:
            res = RES_PARERR;
    }

    Release_SDCard();

    return res;
}

