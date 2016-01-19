#include <project.h>
#include <stdbool.h>
#include "FatFS/ff.h"
#include "FatFS/FatFS_PrettyMacros.h"
#include "FatFSCmdInterface.h"

FatFS_t _FatFs;		/* FatFs work area needed for needed for each volume */

char _FnameBuf[32];
char _CmdBuf[32];
char _DataBuf[64];

uint16_t _USBBufDataCnt = 0;
uint8_t _USBRxBuffer[128];



// commands will always be of the format (cmd, filename, data) where filename and data are optional depending on the command
bool Parse_USBBuffer(void) {
    bool parsingCommand = true;
    bool parsingFilename = false;
    bool parsingData = false;
    
    uint8_t curBufIndex = 0, cmdDataSize = 0, fnameDataSize = 0, dataDataSize = 0;
    char *curBuf = _CmdBuf;
    
    for (uint8_t i = 0; i < _USBBufDataCnt; i++) {
        
        uint8_t b = _USBRxBuffer[i];
        
        if ((b != '\n') && (b != '\r')) {       // exclude any carriage returns or line feeds
        
            // fields are delimited by commas so first check if we are starting a new field
            if (b == ',') {
                
                // end the previous field with a null character
                curBuf[curBufIndex] = 0;
                
                // sanity check the fields for empty or too much data
                if ((parsingCommand) && cmdDataSize == 0) return false;  // this means we got an empty cmd field
                if ((parsingFilename) && fnameDataSize == 0) return false;  // this means we got an empty filename field
                if (parsingData) return false;                              // this menas we got 4 fields

                // start adding data to a new field buffer
                if (parsingFilename) {
                    curBuf = _DataBuf;
                    parsingData = true;
                    parsingFilename = false;
                }
                else if (parsingCommand) {
                    curBuf = _FnameBuf;
                    parsingFilename = true;
                    parsingCommand = false;
                }
                curBufIndex = 0;    // start adding data to the start of our new buffer
            }
            
            // if not, we are adding data to our current field;
            else {
                curBuf[curBufIndex++] = b;
                if (parsingData) {
                    dataDataSize++;
                }
                else if (parsingFilename) {
                    fnameDataSize++;
                }
                else {
                    cmdDataSize++;
                }
            }
        }
    }
    curBuf[curBufIndex] = 0;
    
    // now check for valid commands
    if (cmdDataSize == 0) {
        Print_ToUSBUart("Empty command");
        return false;
    }
    
    // no arg commands
    if (!strcmp(_CmdBuf, "?")) return true;
    if (!strcmp(_CmdBuf, "list")) return true;
    if (!strcmp(_CmdBuf, "free")) return true;
    if (!strcmp(_CmdBuf, "mount")) return true;
    
    // check for cmd, fname commands
    if (!strcmp(_CmdBuf, "print") && fnameDataSize) return true;
    if (!strcmp(_CmdBuf, "erase") && fnameDataSize) return true;
    if (!strcmp(_CmdBuf, "create") && fnameDataSize) return true;
    
    // check for cmd, fname, data commands
    if (!strcmp(_CmdBuf, "append") && fnameDataSize && dataDataSize) return true;
    
    // unknown command or invalid parameters
    Print_ToUSBUart("Unknown command: ");
    Print_ToUSBUart((const char *)_USBRxBuffer);
    Print_ToUSBUart("\n");
    return false;
}


int main()
{
    CyGlobalIntEnable; 
   
    /* Start SPI bus. */
    SDSPI_Start();

    
    // set up the USB connection
    USBUART_Start(0, USBUART_3V_OPERATION);
    while(USBUART_GetConfiguration() == 0){};
    USBUART_CDC_Init();
    
     
    
    while(true) {
        
        _USBBufDataCnt = USBUART_GetCount();
        
        // when we get usb data, grab it and parse it
        if (_USBBufDataCnt != 0) {
            if (_USBBufDataCnt > 128) {
                // too much data, do something (we throw away all the data)
                while(_USBBufDataCnt > 128) {
                    USBUART_GetData(_USBRxBuffer, 128);
                    _USBBufDataCnt -= 128;
                }
                USBUART_GetData(_USBRxBuffer, _USBBufDataCnt);
            }
            else {
                USBUART_GetData(_USBRxBuffer, _USBBufDataCnt);
                bool parseRes = Parse_USBBuffer();
                
                // if we got a vailid command, figure out what it was and run it
                if (parseRes) {
                    if (!strcmp(_CmdBuf, "list")) {
                        List_Dir();
                    }
                    else if (!strcmp(_CmdBuf, "?")) {
                        Display_Help();
                    }
                    else if (!strcmp(_CmdBuf, "mount")) {
                        Mount_Disk(&_FatFs);
                    }
                    else if (!strcmp(_CmdBuf, "free")) {
                        Get_FreeSpace(&_FatFs);        
                    }
                    else if (!strcmp(_CmdBuf, "print")) {
                        Print_File(_FnameBuf);
                    }
                    else if (!strcmp(_CmdBuf, "erase")) {
                        Erase_File(_FnameBuf);   
                    }
                    else if (!strcmp(_CmdBuf, "create")) {
                        Create_File(_FnameBuf);   
                    }
                    else if (!strcmp(_CmdBuf, "append")) {
                        Append_File(_FnameBuf, _DataBuf);
                    }
                }
            }
        }
    }
}


