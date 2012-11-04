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

#include "mem.h"
#include "errormsg.h"
#include "channel.h"
#include "version.h"		/* for SW_NAME */
#include "device.h"		/* for HW_NAME */
#include "debug.h"
#include "led.h"

#undef	DEBUG_ERROR

/// Version number string, will be added to message 73
const char IN_ROM versionstr[] = HW_NAME "/" SW_NAME " V" VERSION LONGVERSION;

#define	H(v)	((v)|0x80)

static const uint8_t IN_ROM tokens[] = {
	"\201ERROR"	// 1
	"\202FILE"	// 2
	"\203WRITE "	// 3
	"\204SYNTAX "	// 4
	"\205NOT "	// 5
	"\206OPEN"	// 6
	"\207FOUND"	// 7
	"\210DIR "	// 8
	"\211NO "	// 9
	// zero-terminated
};

static const uint8_t IN_ROM messages[] = {
	H(ERROR_OK),			' ','O','K',
	H(ERROR_SCRATCHED), 		2,'S',' ','S','C','R','A','T','C','H','E','D',
	H(ERROR_WRITE_PROTECT),		3,'P','R','O','T','E','C','T',' ',1,
	H(ERROR_WRITE_ERROR),		3,1,		// WRITE ERROR
	H(ERROR_SYNTAX_UNKNOWN),
	H(ERROR_SYNTAX_NONAME),
	H(ERROR_SYNTAX_INVAL),
	H(ERROR_SYNTAX_DIR_SEPARATOR),
	H(ERROR_FILE_NAME_TOO_LONG),	4,1,		// SYNTAX ERROR
	H(ERROR_FILE_NOT_FOUND),	2,' ',5,7,	// FILE NOT FOUND
	H(ERROR_FILE_NOT_OPEN),		2,' ',5,6,	// FILE NOT OPEN
	H(ERROR_FILE_EXISTS),		2,' ','E','X','I','S','T','S',
	H(ERROR_FILE_TYPE_MISMATCH),	2,' ','T','Y','P','E',' ','M','I','S','M','A','T','C','H',
	H(ERROR_DIR_NOT_EMPTY),		8,  5,'E','M','T','Y',
	H(ERROR_NO_PERMISSION),		9,'P','E','R','M','I','S','S','I','O','N',
	H(ERROR_FAULT),			'G','E','N','E','R','A','L',' ','F','A','U','L','T',
	H(ERROR_NO_CHANNEL),		9,'C','H','A','N','N','E','L',
	H(ERROR_DIR_ERROR),		8,1,
	H(ERROR_DISK_FULL),		'D','I','S','K',' ','F','U','L','L',
	H(ERROR_DRIVE_NOT_READY),	'D','R','I','V','E',' ',  5,'R','E','A','D','Y',
	0
};

static uint8_t *appendmsg(uint8_t *msg, const uint8_t *table, const uint8_t code) {

	uint8_t *ptr = (uint8_t*) table; // table index
	uint8_t pattern = H(code);

	// first find the code in the give table
	uint8_t c = rom_read_byte(ptr);
	while (c != 0 && c != pattern) {
		ptr++;
		c = rom_read_byte(ptr);
	}
	if (c == 0) {
		// not found
		return msg;
	}
	// skip over remaining codes
	c = rom_read_byte(ptr);
	while (c & 0x80) {
		ptr++;
		c = rom_read_byte(ptr);
	}
	// now ptr points to the first text entry	
	// c contains the value
	do {	
		if (c < 32) {
			// get text from token table (recursive)
			msg = appendmsg(msg, tokens, c);
		} else {
			*msg = c;
			msg++;
		}
		ptr++;
		c = rom_read_byte(ptr);
	} while (c != 0 && (c & 0x80) == 0);
	
	return msg;
}	
	
/* Append a decimal number to a string */
uint8_t *appendnumber(uint8_t *msg, uint8_t value) {
  	if (value >= 100) {
    		*msg++ = '0' + value/100;
    		value %= 100;
  	}
  	*msg++ = '0' + value/10;
  	*msg++ = '0' + value%10;

  	return msg;
}

void set_error_ts(errormsg_t *err, uint8_t errornum, uint8_t track, uint8_t sector) {
  	uint8_t *msg = err->error_buffer;

  	err->readp = 0;

  	msg = appendnumber(msg,errornum);
  	*msg++ = ',';

	if (errornum == ERROR_DOSVERSION) {
		char *p = (char*) versionstr;
		uint8_t c;
		while ((c = rom_read_byte(p++)) != 0) {
			*msg++ = c;
		}
	} else {
    		msg = appendmsg(msg,messages,errornum);
	}
  	*msg++ = ',';

  	msg = appendnumber(msg,track);
  	*msg++ = ',';

  	msg = appendnumber(msg,sector);
  	*msg++ = 13;

  	// end string marker
  	*msg = 0;

	if (errornum != ERROR_OK && errornum != ERROR_DOSVERSION && errornum != ERROR_SCRATCHED) {
		led_set(ERROR);

		term_printf("Setting status to: %s\n", err->error_buffer);
	} else {
		// same as idle, but clears error
		led_set(OFF);
	}

#ifdef DEBUG_ERROR
debug_printf("Set status to: %s\n", err->error_buffer);
#endif
}

