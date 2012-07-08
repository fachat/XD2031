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

#include <stdio.h>
#include <ctype.h>
#include <string.h>

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

// shared global variable to be used in parse_filename, as long as it's threadsafe
nameinfo_t nameinfo;


/*
 * not sure if I should break that into a parser for file names and a parser for 
 * commands...
 *
 * Note that some functionality relies on that the resulting name pointer is 
 * within the cmd_t command buffer! So the result has to be taken from "in place"
 * of what has been given
 */
void parse_filename(cmd_t *in, nameinfo_t *result, uint8_t is_command) {

	// runtime vars
	uint8_t *p = in->command_buffer;
	uint8_t len = in->command_length;

	// init output
	result->drive = 0xff;
	result->cmd = CMD_NONE;	// no command
	result->type = 0;	// PRG
	result->access = 'R';	// read

	// start either for command or file name
	uint8_t state;
	if (is_command) {
		state = NAME_COMMAND;
		result->name = NULL;
		result->namelen = 0;
	} else {
		state = NAME_FILE;
		result->name = p;	// full name
		result->namelen = len;	// default
	}

	uint8_t drv = 0;
	while (len > 0) {

		switch(state) {
		case NAME_COMMAND:
			// save first char as potential command	
			if (result->cmd == CMD_NONE) {
				if (isalpha(*p)) {
					result->cmd = command_find(p);
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
				result->cmd = CMD_DIR;	// directory
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
			if (is_command) {
				// here the "=" case for COPY/RENAME is missing, 
				// also the "," for multiple files after the "="
				// - further processing is done in command handling
				break;
			}
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

	// insert the drive (as endpoint address) as first byte of the file name
        // prepare request data
        if (result->name == in->command_buffer) {
                // we used a default, and need to insert the endpoint in front
                // of the name
                memmove(result->name+1, result->name, result->namelen);
                result->namelen++;
        } else {
                // parser has left some space before the name
                result->namelen++;
                result->name--;
        }
        result->name[0] = result->drive;

#if DEBUG_NAME
	debug_printf("CMD=%s\n", result->cmd == CMD_NONE ? '-' : command_to_name(result->cmd));
	debug_printf("DRIVE=%c\n", result->drive == 0xff ? '-' : result->drive + 0x30);
	debug_puts("NAME="); debug_puts((char*)result->name); debug_putcrlf();
	debug_puts("ACCESS="); debug_putc(result->access); debug_putcrlf();
	debug_puts("TYPE="); debug_putc(result->type); debug_putcrlf();
#endif
}

