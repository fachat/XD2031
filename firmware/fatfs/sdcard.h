/*-----------------------------------------------------------------------
/  Low level SD card routines used by FatFs
/-----------------------------------------------------------------------*/

#ifndef SDCARD_H_DEFINED
#define SDCARD_H_DEFINED

#include "integer.h"
#include "diskio.h"

volatile uint8_t media_status;

/* Prototypes for disk control functions */

#if 0

DSTATUS SD_disk_initialize (BYTE pdrv);
DSTATUS SD_disk_status (BYTE pdrv);
DRESULT disk_read (BYTE pdrv, BYTE*buff, DWORD sector, UINT count);
#if _READONLY == 0
DRESULT disk_write (BYTE pdrv, const BYTE* buff, DWORD sector, UINT count);
#endif
DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void* buff);

#endif
#endif
