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
#include "errors.h"


typedef struct {
	uint8_t		errorno;
	// callback that is called when the buffer is to be reset
	//void 		(*set_ok_message)();	// (struct errormsg_t *err);
	// error buffer
	uint8_t 	error_buffer[CONFIG_ERROR_BUFFER_SIZE];
	uint8_t		readp;	// read index in error_buffer
} errormsg_t ;

void set_error_tsd(errormsg_t *error, uint8_t errornum, uint8_t track, uint8_t sector, int8_t drive);

void set_status(errormsg_t *err, char* s);

static inline void set_error(errormsg_t *error, uint8_t errornum) {
	// note: ignores possible changes in T&S
	if (error->errorno != errornum || error->readp != 0) {
		set_error_tsd(error, errornum, 0, 0, 0);
	}
}

#endif
