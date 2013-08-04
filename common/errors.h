/* 
 * XD2031 - Serial line file server for CBMs
 * Copyright (C) 2012 Andre Fachat <afachat@gmx.de>
 * 
 * Taken over / derived from:
 * sd2iec - SD/MMC to Commodore serial bus interface/controller
   Copyright (C) 2007-2012  Ingo Korb <ingo@akana.de>
   Inspired by MMC2IEC by Lars Pontoppidan et al.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License only.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


   errormsg.h: Definitions for the error message generator

*/

/**
 * Note that this error message code is deficient for a device that 
 * has both IEEE and IEC busses, as it only supports a single error 
 * state.
 */
#ifndef ERRORS_H
#define ERRORS_H

/*
 * note: should better be <100 for two-digit display
 * note: should better be <128 to not mess up sign handling in some places
 * 	(e.g. out_callback in rtconfig.c)
 */

typedef enum {
	CBM_ERROR_OK 			= 0,
	CBM_ERROR_SCRATCHED		= 1,
	CBM_ERROR_OPEN_REL		= 2,	// opened a relative file (internal)
	// in CBM DOS error numbers 20-29 are translated from FDC errors (mostly unused here)
//	CBM_ERROR_WRITE_VERIFY		= 25,
	CBM_ERROR_WRITE_PROTECT		= 26,
	CBM_ERROR_WRITE_ERROR		= 28,
	// error numbers 30-34 are all just "SYNTAX ERROR" on the 1541
	CBM_ERROR_SYNTAX_UNKNOWN	= 30,
	CBM_ERROR_SYNTAX_NONAME		= 34,
	CBM_ERROR_FILE_NAME_TOO_LONG	= 38,	// new for ENAMETOOLONG
	CBM_ERROR_FILE_NOT_FOUND	= 39,
	// new problems
	CBM_ERROR_SYNTAX_INVAL		= 40,	// EINVAL
	CBM_ERROR_SYNTAX_DIR_SEPARATOR	= 41,	// name contains directory separator
	// REL file errors
	CBM_ERROR_RECORD_NOT_PRESENT	= 50,	// also when REL file rec lengths not match
	CBM_ERROR_OVERFLOW_IN_RECORD	= 51,
	CBM_ERROR_TOO_LARGE		= 52,

	CBM_ERROR_DIR_NOT_EMPTY		= 57,	// new for ENOTEMPTY
	CBM_ERROR_NO_PERMISSION		= 58,	// new for EACCESS
	CBM_ERROR_FAULT			= 59,	// new for EFAULT and others (fallback)

	// DOS file level problems
//	CBM_ERROR_WRITE_FILE_OPEN	= 60,
	CBM_ERROR_FILE_NOT_OPEN		= 61,	
	CBM_ERROR_FILE_NOT_FOUND62	= 62,
	CBM_ERROR_FILE_EXISTS		= 63,	// also used for EEXIST
	CBM_ERROR_FILE_TYPE_MISMATCH	= 64,
	CBM_ERROR_NO_BLOCK		= 65,	// error for FS_BLOCK_ALLOCATE
	CBM_ERROR_ILLEGAL_T_OR_S	= 66,	// error for FS_BLOCK_ALLOCATE
	// DOS/disk problems and status
	CBM_ERROR_NO_CHANNEL		= 70,
	CBM_ERROR_DIR_ERROR		= 71,
	CBM_ERROR_DISK_FULL		= 72,
	CBM_ERROR_DOSVERSION		= 73,
	CBM_ERROR_DRIVE_NOT_READY	= 74
} errno_t;


#endif
