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
#ifndef ERRORMSG_H
#define ERRORMSG_H

#include <stdint.h>

#include "config.h"


//typedef void errormsg_t;

typedef struct {
	// callback that is called when the buffer is to be reset
	//void 		(*set_ok_message)();	// (struct errormsg_t *err);
	// error buffer
	uint8_t 	error_buffer[CONFIG_ERROR_BUFFER_SIZE];
	uint8_t		readp;	// read index in error_buffer
} errormsg_t ;

void set_error_ts(errormsg_t *error, uint8_t errornum, uint8_t track, uint8_t sector);

static inline void set_error(errormsg_t *error, uint8_t errornum) {
	set_error_ts(error, errornum, 0, 0);
}

typedef enum {
	ERROR_OK 			= 0,
	ERROR_SCRATCHED			= 1,
	ERROR_STATUS 			= 3,
	ERROR_LONGVERSION		= 9,
	// in CBM DOS error numbers 20-29 are translated from FDC errors (mostly unused here)
//	ERROR_WRITE_VERIFY		= 25,
//	ERROR_WRITE_PROTECT		= 26,
	// error numbers 30-34 are all just "SYNTAX ERROR" on the 1541
	ERROR_SYNTAX_UNKNOWN		= 30,
	ERROR_SYNTAX_NONAME		= 34,
	ERROR_FILE_NOT_FOUND		= 39,
	// relative file errors
//	ERROR_RECORD_NOT_PRESENT	= 50,
//	ERROR_OVERFLOW_IN_RECORD	= 51,
	// DOS file level problems
//	ERROR_WRITE_FILE_OPEN		= 60,
	ERROR_FILE_NOT_OPEN		= 61,	
	ERROR_FILE_EXISTS		= 63,
	ERROR_FILE_TYPE_MISMATCH	= 64,
//	ERROR_NO_BLOCK			= 65,
	// DOS/disk problems and status
	ERROR_NO_CHANNEL		= 70,
//	ERROR_DIR_ERROR			= 71,
//	ERROR_DISK_FULL			= 72,
	ERROR_DOSVERSION		= 73,
	ERROR_DRIVE_NOT_READY		= 74
} errno_t;


#endif
