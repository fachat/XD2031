/****************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2012 Andre Fachat

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation;
    version 2 of the License ONLY.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

****************************************************************************/

/**
 * This file contains the file name parser
 */

#define	DEBUG_NAME 0

#include "ctype.h"

#include "cmd.h"
#include "name.h"

#include "debug.h"

#define	NAME_DRIVE	0
#define	NAME_NAME	1
#define	NAME_TYPE	2
#define	NAME_ACCESS	3
#define	NAME_COMMAND	4
#define	NAME_CMDDRIVE	5
#define	NAME_FILE	6

/*
 * not sure if I should break that into a parser for file names and a parser for 
 * commands...
 */
void parse_filename(cmd_t *in, nameinfo_t *result, uint8_t is_command) {

	// runtime vars
	uint8_t *p = in->command_buffer;
	uint8_t len = in->command_length;

	// init output
	result->drive = 0xff;
	result->cmd = 0;	// no command
	result->type = 0;	// PRG
	result->access = 'R';	// read
	result->name = p;	// full name
	result->namelen = len;	// default

	// start either for command or file name
	uint8_t state = is_command ? NAME_COMMAND : NAME_FILE;
	uint8_t drv = 0;
	while (len > 0) {

		switch(state) {
		case NAME_COMMAND:
			// save first char as potential command	
			if (result->cmd == 0) {
				if (isalpha(*p)) {
					result->cmd = *p;
					result->namelen = 0;	// just to be sure, until we parsed it
				} 
			}
			// fallthrough
		case NAME_CMDDRIVE:
			// last digit as drive
			if (isdigit(*p)) {
				result->drive = *p - 0x30;
			}
			// command parameters following?
			if (*p == ':') {
				result->name = (p+1);
				result->namelen = len-1;
				state = NAME_NAME;
			}
			break;
		case NAME_FILE:
			if (*p == '$') {
				result->cmd = *p;	// directory
				result->namelen = 0;	// just to be sure, until we parsed it
				state = NAME_CMDDRIVE;	// jump into command parser
			}
		case NAME_DRIVE:
			// last digit as potential drive
			if (isdigit(*p)) {
				drv = *p - 0x30;
				//p++;
			}
			if (*p == ':') {
				// found drive separator
				result->drive = drv;
				result->name = (p+1);
				result->namelen = len-1;
				state = NAME_NAME;
				break;
			}
			// fallthrough
		case NAME_NAME:
			// here the "=" case for COPY/RENAME is missing, 
			// also the "," for multiple files after the "="
			if (*p == ',') {
				// found file type/access separator
				result->namelen = p - result->name;
				state = NAME_TYPE;
				break;
			}
			break;
		case NAME_TYPE:
			if (result->type == 0) {
				result->type = *p;
			}
			if (*p == ',') {
				state = NAME_ACCESS;
			}
			break;
		case NAME_ACCESS:
			if (result->access == 0) {
				result->access = *p;
			}
			break;
		default:
			break;
		}
		len--;
		p++;
	}

//	if (result->cmd == 0 && cmd == '$') {
//	 	// no command detected, but name starts with "$"
//	 	// so this is a directory name without file name filter
//		result->drive = drv;
//		result->cmd = cmd;
//		result->namelen = 0;
//	}

#if DEBUG_NAME
	debug_printf("CMD=%c\n", result->cmd == 0 ? '-' : result->cmd);
	debug_printf("DRIVE=%c\n", result->drive == 0xff ? '-' : result->drive + 0x30);
	debug_puts("NAME="); debug_puts((char*)result->name); debug_putcrlf();
	debug_puts("ACCESS="); debug_putc(result->access); debug_putcrlf();
	debug_puts("TYPE="); debug_putc(result->type); debug_putcrlf();
#endif
}

