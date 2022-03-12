/* 
   XD2031 - Serial line file server for CBMs
   Copyright (C) 2012 Andre Fachat <afachat@gmx.de>
 
   Generating the DOS error message
   Inspired by sd2iec by Ingo Korb, but rewritten in the end 

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

*/

#include <string.h>
#include <ctype.h>

#include "archcompat.h"
#include "errnames.h"
#include "errormsg.h"
#include "channel.h"
#include "version.h"    /* for SW_NAME */
#include "debug.h"
#include "term.h"
#include "led.h"

#undef	DEBUG_ERROR


void set_error_tsd(errormsg_t *err, uint8_t errornum, uint8_t track, uint8_t sector, int8_t drive) {
	char *msg = (char *)err->error_buffer;
	err->errorno = errornum;
  	err->readp = 0;

	rom_sprintf(msg, IN_ROM_STR("%2.2d,"), errornum);	// error number
	rom_strcat(msg, errmsg(errornum));			// error message from flash memory
	if (drive < 0) {
		rom_sprintf(msg + strlen(msg), IN_ROM_STR(",%2.2d,%2.2d\r"), track%100, sector%100); // track & sector
	} else {
		rom_sprintf(msg + strlen(msg), IN_ROM_STR(",%2.2d,%2.2d,%1.1d\r"), track, sector, drive); // track & sector & drive
	}

	if (errornum != CBM_ERROR_OK         &&
	    errornum != CBM_ERROR_DOSVERSION &&
	    errornum != CBM_ERROR_SCRATCHED) {
		led_set(ERROR);
		term_printf("Setting status to: %s\n", err->error_buffer);
	} else {
		led_set(OFF); // same as idle, but clears error
	}

#ifdef DEBUG_ERROR
debug_printf("Set status to: %s\n", err->error_buffer);
#endif
}

void set_status(errormsg_t *err, char* s) {
	strncpy( (char*) err->error_buffer, s, CONFIG_ERROR_BUFFER_SIZE - 1);
	err->error_buffer[CONFIG_ERROR_BUFFER_SIZE - 1] = 0;
	err->readp = 0;
	err->errorno = 0;
}
