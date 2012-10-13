/*-----------------------------------------------------------------------
/  Low level SD card routines used by FatFs
/-----------------------------------------------------------------------*/

#ifndef SDCARD_H_DEFINED
#define SDCARD_H_DEFINED

#define CS_LOW()	PORT_SD_CS &= ~_BV(PIN_SD_CS)
#define CS_HIGH()	PORT_SD_CS |= _BV(PIN_SD_CS)

/* Prototypes for disk control functions */

DSTATUS SD_disk_initialize (BYTE);
DSTATUS SD_disk_status (BYTE);
DRESULT SD_disk_read (BYTE, BYTE*, DWORD, BYTE);
#if _READONLY == 0
  DRESULT SD_disk_write (BYTE, const BYTE*, DWORD, BYTE);
#endif
DRESULT SD_disk_ioctl (BYTE, BYTE, void*);

#endif
