/*-----------------------------------------------------------------------
/  Low level SD card routines used by FatFs
/-----------------------------------------------------------------------*/

#ifndef SDCARD_H_DEFINED
#define SDCARD_H_DEFINED

#include "integer.h"
#include "diskio.h"

volatile uint8_t media_status;

/* Prototypes for disk control functions */

DSTATUS SD_disk_initialize (BYTE);
DSTATUS SD_disk_status (BYTE);
DRESULT SD_disk_read (BYTE, BYTE*, DWORD, BYTE);
#if _READONLY == 0
  DRESULT SD_disk_write (BYTE, const BYTE*, DWORD, BYTE);
#endif
DRESULT SD_disk_ioctl (BYTE, BYTE, void*);

#endif
