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

#include "ctype.h"

#include "cmd.h"
#include "name.h"

#include "debug.h"

#define	NAME_DRIVE	0
#define	NAME_NAME	1
#define	NAME_TYPE	2
#define	NAME_ACCESS	3

void parse_filename(cmd_t *in, nameinfo_t *result) {

	// runtime vars
	uint8_t *p = in->command_buffer;
	uint8_t len = in->command_length;

	// init output
	result->drive = 0;
	result->cmd = 0;	// no command
	result->type = 0;	// PRG
	result->access = 'R';	// read
	result->name = p;	// full name
	result->namelen = len;	// default

	// tmp
	uint8_t state = NAME_DRIVE;
	uint8_t cmd = 0;
	uint8_t drv = 0;
	while (len > 0) {

		switch(state) {
		case NAME_DRIVE:
			// save first char as potential command	
			if (cmd == 0 
				&& (isalpha(*p) || (*p == '$'))) {
				cmd = *p;
				//p++;
			}
			// last digit as potential drive
			if (isdigit(*p)) {
				drv = *p - 0x30;
				//p++;
			}
			if (*p == ':') {
				// found drive separator
				result->drive = drv;
				result->cmd = cmd;
				result->name = (p+1);
				result->namelen = len-1;
				state = NAME_NAME;
				break;
			}
			// fallthrough
		case NAME_NAME:
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

	debug_puts("CMD="); debug_putc(result->cmd); debug_putcrlf();
	debug_puts("DRIVE="); debug_putc(result->drive); debug_putcrlf();
	debug_puts("NAME="); debug_puts((char*)result->name); debug_putcrlf();
	debug_puts("ACCSS="); debug_putc(result->access); debug_putcrlf();
	debug_puts("TYPE="); debug_putc(result->type); debug_putcrlf();

}

