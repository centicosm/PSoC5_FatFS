#ifndef FATFS_CMD_INTERFACE_H
#define FATFS_CMD_INTERFACE_H

#include "FatFS/ff.h"
#include "FatFS/FatFS_PrettyMacros.h"

    
void Print_ToUSBUart(const char *buf);
    
void Display_Help(void);
void Mount_Disk(FatFS_t *fatFS);
void Erase_File(const char *fileName);
void Create_File(const char *fileName);
void Print_File(const char *fileName);
void Append_File(const char *fileName, const char *line);
void List_Dir(void);
void Get_FreeSpace(FatFS_t *fatFs);



#endif