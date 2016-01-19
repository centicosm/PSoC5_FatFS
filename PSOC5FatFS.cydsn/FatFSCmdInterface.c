#include "project.h"
#include <stdio.h>
#include "FatFS/ff.h"
#include "FatFS/FatFS_PrettyMacros.h"
#include "FatFSCmdInterface.h"


/*  Implementation for the interface of the FatFS testing utility */



void Print_ToUSBUart(const char *buf) {
 
    while (!USBUART_CDCIsReady()) {};
    USBUART_PutString(buf);   
}


// '?' will display the available commands
void Display_Help(void) {
    Print_ToUSBUart("\n---Available Commands---\n");
    Print_ToUSBUart("NOTE; Commands with multiple parameters should have no spaces after the comma.\n\n");
    Print_ToUSBUart("? : Display this menu\n");
    Print_ToUSBUart("mount : Mount card\n");
    Print_ToUSBUart("free : Print free space available\n");
    Print_ToUSBUart("list : List disk contents\n");
    Print_ToUSBUart("erase,fileName : Erase fileName\n");
    Print_ToUSBUart("create,fileName : Create empty file with fileName\n");
    Print_ToUSBUart("print,fileName : Display contents of filename\n");
    Print_ToUSBUart("append,fileName,data : Add text 'data' to end of fileName\n\n");
}


// mount the card, must be used before any other operation
void Mount_Disk(FatFS_t *fatFS) {
 	FatFS_Result_t res = f_mount(fatFS, "", 1);
    if (res != FR_OK) {
        Print_ToUSBUart("Error mounting sd card\n");   
    }
    else {
        Print_ToUSBUart("Mounted sd card\n");   
    }   
}


// delete the given fileName from the disk
void Erase_File(const char *fileName) {
   char buf[64];
    
    sprintf(buf, "Erasing file: %s\n", fileName);
    Print_ToUSBUart(buf);
    
    FatFS_Result_t res = f_unlink(fileName);
   	if (res == FR_OK) {
        Print_ToUSBUart("Done\n");   
    }
    else {
        Print_ToUSBUart("Error erasing file\n");
    }   
}


// Create an empty file named fileName
void Create_File(const char *fileName) {
    char buf[64];
    FatFS_File_t fileHandle;
    
    sprintf(buf, "Creating file: %s\n", fileName);
    Print_ToUSBUart(buf);
    
    FatFS_Result_t res = f_open(&fileHandle, fileName, FA_CREATE_ALWAYS | FA_WRITE);
   	if (res == FR_OK) {
        f_close(&fileHandle);
        Print_ToUSBUart("Done\n");   
    }
    else {
        Print_ToUSBUart("Error creating file\n");
    }
}

// print the contents of fileName
void Print_File(const char *fileName) {
    char buf[128];
    
    sprintf(buf, "Printing file: %s\n", fileName);
    Print_ToUSBUart(buf);

    FatFS_File_t fileHandle;
    FatFS_Result_t res = f_open(&fileHandle, fileName, FA_READ);

    if (res == FR_OK) {
        while (f_gets(buf, 128, &fileHandle)) {
            Print_ToUSBUart(buf);            
        }
    	f_close(&fileHandle);					    
        Print_ToUSBUart("\n\nDone\n");   
    }
    else {
        Print_ToUSBUart("Error reading file\n");   
    }
}



// append line to the end of fileName
void Append_File(const char *fileName, const char *line) {
    char buf[128];
    
    sprintf(buf, "Appending to file: %s\n", fileName);
    Print_ToUSBUart(buf);

    FatFS_File_t fileHandle;
    FatFS_Result_t res = f_open(&fileHandle, fileName, FA_WRITE);

    if (res == FR_OK) {
        FatFS_Result_t seekRes = f_lseek(&fileHandle, f_size(&fileHandle));
        if (seekRes == FR_OK) {
            f_puts(line, &fileHandle);
    	    f_close(&fileHandle);
            Print_ToUSBUart("Done\n");
        }
        else {
            Print_ToUSBUart("Error seeking to send of file\n");   
        }
    }
    else {
        Print_ToUSBUart("Error appending to file\n");   
    }
}


// List the contents of the root directory
void List_Dir(void) {
    char buf[64];
    FatFS_Dir_t dirObj;         /* Directory search object */
    FatFS_FileInfo_t fileInfo;    /* File information */
    
    Print_ToUSBUart("Listing Dir:\n");
    FatFS_Result_t res = f_findfirst(&dirObj, &fileInfo, "", "*");
    if (res == FR_OK) {
        while ((res == FR_OK) && (fileInfo.fname[0]) != 0) {
            sprintf(buf, "File: %s\n", fileInfo.fname);
            Print_ToUSBUart(buf);
            res = f_findnext(&dirObj, &fileInfo);
        }
        Print_ToUSBUart("\n--Done--\n");
        f_closedir(&dirObj);
        
    }
    else {
        USBUART_PutString("Error listing directory");   
    }
}


// print the total and free space available on the disk
void Get_FreeSpace(FatFS_t *fatFs) {
    char buf[64];
    uint32_t freeClusters, totalSectors, freeSectors;
    
    FatFS_Result_t res = f_getfree("", &freeClusters, &fatFs);
    if (res == FR_OK) {
        totalSectors = (fatFs->n_fatent - 2) * fatFs->csize;
        sprintf(buf, "Total sectors: %lu\n", totalSectors);
        Print_ToUSBUart(buf); 
        
        freeSectors = freeClusters * fatFs->csize;
        sprintf(buf, "Free sectors: %lu\n", freeSectors);
        Print_ToUSBUart(buf); 

        sprintf(buf, "Total space: %lu\n", totalSectors * 512);
        Print_ToUSBUart(buf); 
        
        sprintf(buf, "Free space: %lu\n", freeSectors * 512);
        Print_ToUSBUart(buf);         
    }
    
    else {
        Print_ToUSBUart("Error getting free space\n");   
    }
}

