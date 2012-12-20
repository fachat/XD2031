/****************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2012 Andre Fachat

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

****************************************************************************/

/**
 * This file contains the file name parser
 */

#undef	DEBUG_NAME 

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "cmd.h"
#include "name.h"

#include "debug.h"

#define	NAME_DRIVE	0
#define	NAME_NAME	1
#define	NAME_OPTS	2
#define	NAME_COMMAND	4
#define	NAME_CMDDRIVE	5
#define	NAME_FILE	6
#define	NAME_DRIVE2	7
#define	NAME_NAME2	8

// shared global variable to be used in parse_filename, as long as it's threadsafe
nameinfo_t nameinfo;


/*
 * not sure if I should break that into a parser for file names and a parser for 
 * commands...
 *
 * Note that some functionality relies on that the resulting name pointer is 
 * within the cmd_t command buffer! So the result has to be taken from "in place"
 * of what has been given
 *
 * To distinguish numeric drive numbers from unassigned (undefined) drives like "ftp:",
 * the provider name must not end with a digit.
 */
void parse_filename(cmd_t *in, nameinfo_t *result, uint8_t is_command) {

	uint8_t len = in->command_length;	//  includes the zero-byte

	// copy over command to the end of the buffer, so we can 
	// construct it from the parts at the beginning after parsing it
	// (because we may need to insert bytes at some places, which would
	// be difficult)
	uint8_t diff = CONFIG_COMMAND_BUFFER_SIZE - len;
	memmove(in->command_buffer + diff, in->command_buffer, len);

	// adjust so we exclude final null byte
	len--;

	// runtime vars
	uint8_t *p = in->command_buffer + diff;
	uint8_t ch;

	// init output
	result->drive = NAMEINFO_UNUSED_DRIVE;
	result->drive2 = NAMEINFO_UNUSED_DRIVE;
	result->cmd = CMD_NONE;	// no command
	result->type = 0;	// PRG
	result->access = 0;	// read
	result->options = 0;	// read

	// start either for command or file name
	uint8_t state;
	uint8_t *name = p;		// initial name ptr
	result->name2 = NULL;
	result->namelen2 = 0;
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
		ch = *p;
#ifdef DEBUG_NAME
		debug_printf("len=%d, curr=%c, state=%d\n", len, *p, state);
#endif
		switch(state) {
		case NAME_COMMAND:
			// save first char as potential command	
			if (result->cmd == CMD_NONE) {
				if (isalpha(*p)) {
					result->cmd = command_find(p);
					result->namelen = 0;	// just to be sure, until we parsed it
					state = NAME_CMDDRIVE;
				} 
			}
			break;
		case NAME_CMDDRIVE:
			if (isspace(ch)) {
				// move over from NAME_COMMAND
				state = NAME_CMDDRIVE;
				name = p+1;	// name is byte after space
			} else
			if (isdigit(ch)) {
				// last digit as drive
				result->drive = ch - 0x30;
			} else
			// command parameters following?
			if (ch == ':') {
				if (result->drive != NAMEINFO_UNDEF_DRIVE) {
					result->name = (p+1);
					result->namelen = len-1;
				} else {
					result->name = name;
					result->namelen = p - name + len;
				}
				state = NAME_NAME;
			} else {
				if (state == NAME_CMDDRIVE) {
					// non-numeric
					result->drive = NAMEINFO_UNDEF_DRIVE;
				}
			}
			break;
		case NAME_FILE:
			if (ch == '$') {
				result->cmd = CMD_DIR;	// directory
				result->namelen = 0;	// just to be sure, until we parsed it
				state = NAME_CMDDRIVE;	// jump into command parser
				name = p+1;
				break;
			} else
			if (ch == '@') {
				result->cmd = CMD_OVERWRITE;	// overwrite a file
				result->name = (p+1);
				result->namelen = len-1;
				state = NAME_DRIVE;
				break;
			}
		case NAME_DRIVE:
			// last digit as potential drive
			if (isdigit(ch)) {
				drv = ch - 0x30;
				//p++;
			} else
			if (ch == ':') {
				// found drive separator
				result->drive = drv;
				if (drv != NAMEINFO_UNDEF_DRIVE) {
					// if we had a real drive, hide it from name,
					// otherwise the "provider:" part is included in the name
					result->name = (p+1);
					result->namelen = len-1;
				}
				state = NAME_NAME;
				break;
			} else {
				drv = NAMEINFO_UNDEF_DRIVE;
			}
			// fallthrough
		case NAME_NAME:
			if (is_command) {
				if (ch == '=') {
					*p = 0;	// end first file name
					result->namelen = (p-result->name);
					
					result->drive2 = NAMEINFO_UNUSED_DRIVE;
					result->name2 = (p+1);
					result->namelen2 = len-1;
					state = NAME_DRIVE2;
				}
				break;
			}
			if (ch == ',') {
				*p = 0; // end file name
				// found file type/access separator
				result->namelen = p - result->name;
				state = NAME_OPTS;
				break;
			}
			break;
		case NAME_DRIVE2:
			// last digit as potential drive
			if (isdigit(ch)) {
				drv = ch - 0x30;
				//p++;
			} else
			if (ch == ':') {
				// found drive separator
				result->drive2 = drv;
				if (drv != NAMEINFO_UNDEF_DRIVE) {
					// if we had a real drive, hide it from name,
					// otherwise the "provider:" part is included in the name
					result->name2 = (p+1);
					result->namelen2 = len-1;
				}
				state = NAME_NAME2;
				break;
			} else {
				drv = NAMEINFO_UNDEF_DRIVE;
			}
			// fallthrough
		case NAME_NAME2:
			// nothing to do, only commands here
			break;
		case NAME_OPTS:
			// options can be used in any order, but each type only once
			if ((ch == 'P' || ch == 'S' || ch == 'R') && result->type == 0) {
				result->type = ch;
			} else
			if ((ch == 'R' || ch == 'W' || ch == 'A' || ch == 'X') && result->access == 0) {
				result->access = ch;
			} else
			if (ch == 'N') {
				result->options |= NAMEOPT_NONBLOCKING;
			}
			break;
		default:
			break;
		}
		len--;
		p++;
	}

	if (result->access == 0) {
		result->access = 'R';
	}

#ifdef DEBUG_NAME
	debug_printf("CMD=%s\n", result->cmd == CMD_NONE ? "-" : command_to_name(result->cmd));
	debug_printf("DRIVE=%c\n", result->drive == NAMEINFO_UNUSED_DRIVE ? '-' : 
				(result->drive == NAMEINFO_UNDEF_DRIVE ? '*' :
				result->drive + 0x30));
	debug_printf("NAME='%s' (%d)\n", result->name, result->namelen);
	debug_puts("ACCESS="); debug_putc(result->access); debug_putcrlf();
	debug_puts("TYPE="); debug_putc(result->type); debug_putcrlf();
	debug_printf("NAME2='%s' (%d)\n", result->name2 == NULL ? "" : (char*)result->name2, result->namelen2);
	debug_printf("DRIVE2=%c\n", result->drive2 == NAMEINFO_UNUSED_DRIVE ? '-' : 
				(result->drive2 == NAMEINFO_UNDEF_DRIVE ? '*' :
				result->drive2 + 0x30));
#endif
}

/**
 * assembles the filename packet from nameinfo into the target buffer.
 * For this it is essential, that nameinfo content does not overlap
 * (at least in the order of assembly) with the target buffer.
 * That is why the command_buffer content is moved to the end of the
 * command_buffer in parse_filename - so it can be re-assembled in the
 * beginning of the command_buffer.
 * 
 * it returns the number of bytes in the buffer
 */
uint8_t assemble_filename_packet(uint8_t *trg, nameinfo_t *nameinfo) {

	uint8_t *p = trg;

	*p = nameinfo->drive;
	p++;

	if (nameinfo->namelen == 0) {
		*p = 0;
		p++;
		return p - trg;
	}

	strcpy((char*)p, (char*)nameinfo->name);
	p += nameinfo->namelen + 1;

	if (nameinfo->namelen2 == 0) {
		return p-trg;
	}

	*p = nameinfo->drive2;
	p++;

	strcpy((char*)p, (char*)nameinfo->name2);
	p += nameinfo->namelen2 + 1;

	return p-trg;
}

