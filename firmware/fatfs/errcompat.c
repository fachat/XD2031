/**************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2013 Andre Fachat, Nils Eilers

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
    MA  02110-1301, USA.

***************************************************************************/


/* ----- FatFs FRESULT errors vs CBM errno_t errors --------------------------

   Some FatFs functions depend on the error code of other internal functions,
   so making them an alias to CBM_ERROR-ones does not seem to be a good
   idea if some information is lost by mapping several FatFs errors to a
   single CBM error code (e.g. CBM_ERROR_DRIVE_NOT_READY)

*/

#include "errcompat.h"
#include "archcompat.h"
#include "ff.h"
#include "errors.h"

#if _FATFS != 80960	/* Revision ID */
#error Table might be outdated
#endif

static const errno_t fresult_tbl[] IN_ROM = {
   [FR_OK]                  = CBM_ERROR_OK,              /* (0) Succeeded */
   [FR_DISK_ERR]            = CBM_ERROR_WRITE_ERROR,     /* (1) A hard error occurred in the low level disk I/O layer */
   [FR_INT_ERR]             = CBM_ERROR_FAULT,           /* (2) Assertion failed */
   [FR_NOT_READY]           = CBM_ERROR_DRIVE_NOT_READY, /* (3) The physical drive cannot work */
   [FR_NO_FILE]             = CBM_ERROR_FILE_NOT_FOUND,  /* (4) Could not find the file */
   [FR_NO_PATH]             = CBM_ERROR_DIR_ERROR,       /* (5) Could not find the path */
   [FR_INVALID_NAME]        = CBM_ERROR_SYNTAX_UNKNOWN,  /* (6) The path name format is invalid */
   [FR_DENIED]              = CBM_ERROR_NO_PERMISSION,   /* (7) Access denied due to prohibited access or directory full */
   [FR_EXIST]               = CBM_ERROR_FILE_EXISTS,     /* (8) Access denied due to prohibited access */
   [FR_INVALID_OBJECT]      = CBM_ERROR_FAULT,           /* (9) The file/directory object is invalid */
   [FR_WRITE_PROTECTED]     = CBM_ERROR_WRITE_PROTECT,   /* (10) The physical drive is write protected */
   [FR_INVALID_DRIVE]       = CBM_ERROR_DRIVE_NOT_READY, /* (11) The logical drive number is invalid */
   [FR_NOT_ENABLED]         = CBM_ERROR_DRIVE_NOT_READY, /* (12) The volume has no work area */
   [FR_NO_FILESYSTEM]       = CBM_ERROR_DRIVE_NOT_READY, /* (13) There is no valid FAT volume */
   [FR_MKFS_ABORTED]        = CBM_ERROR_SYNTAX_UNKNOWN,  /* (14) The f_mkfs() aborted due to any parameter error */
   [FR_TIMEOUT]             = CBM_ERROR_DRIVE_NOT_READY, /* (15) Could not get a grant to access the volume within defined period */
   [FR_LOCKED]              = CBM_ERROR_NO_PERMISSION,   /* (16) The operation is rejected according to the file sharing policy */
   [FR_NOT_ENOUGH_CORE]     = CBM_ERROR_DISK_FULL,       /* (17) LFN working buffer could not be allocated */
   [FR_TOO_MANY_OPEN_FILES] = CBM_ERROR_NO_CHANNEL,      /* (18) Number of open files > _FS_SHARE */
   [FR_INVALID_PARAMETER]   = CBM_ERROR_SYNTAX_UNKNOWN   /* (19) Given parameter is invalid */
};

errno_t conv_fresult(FRESULT fres) {
	if (fres > sizeof(fresult_tbl)) return CBM_ERROR_FAULT;
	return rom_read_byte(&fresult_tbl[fres]);
}

errno_t combine (errno_t cres, FRESULT fres) {
	if (cres != CBM_ERROR_OK) return cres;
	return conv_fresult(fres);
}
