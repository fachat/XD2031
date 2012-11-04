/* 
    XD-2031 - Serial line file server for CBMs
    Copyright (C) 2012  Andre Fachat

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

*/

#include <ctype.h>

#include "cmd.h"
#include "cmd2.h"
#include "file.h"
#include "name.h"
#include "wireformat.h"

#include "debug.h"

#define	DEBUG_CMD


command_t command_find(uint8_t *input) {
	switch(*input) {
	case '$':
		return CMD_DIR;
		break;
	case 'I':
		return CMD_INITIALIZE;
		break;
	case 'R':
		if (*(input+1) == 'M' || *(input+1) == 'D') {
			// RMDIR or RD
			return CMD_RMDIR;
		}
		return CMD_RENAME;
		break;
	case 'S':
		return CMD_SCRATCH;
		break;
	case 'C':
		if (*(input+1) == 'D' || *(input+1) == 'H') {
			// CD or CHDIR
			return CMD_CD;
		}
		// this would be the COPY command
		return CMD_SYNTAX;
		break;
	case 'M':
		// MKDIR or MD
		return CMD_MKDIR;
		break;
	case 'A':
		return CMD_ASSIGN;
		break;
	case 'X':
		// extensions for XD2031
		return CMD_EXT;
		break;
	}
        return CMD_SYNTAX;
}

const char *command_to_name(command_t cmd) {
        switch(cmd) {
        case CMD_NONE:
                return "-";
                break;
        case CMD_DIR:
                return "$";
                break;
        case CMD_SYNTAX:
                return "?";
                break;
        case CMD_INITIALIZE:
                return "INITIALIZE";
                break;
        case CMD_RENAME:
                return "RENAME";
                break;
        case CMD_SCRATCH:
                return "SCRATCH";
                break;
        case CMD_CD:
                return "CD";
                break;
        case CMD_MKDIR:
                return "MKDIR";
                break;
        case CMD_RMDIR:
                return "RMDIR";
                break;
        case CMD_ASSIGN:
                return "ASSIGN";
                break;
	case CMD_EXT:
		return "EXT";
		break;
        }
        return "";
}

// note: this does not return an actual error code, 
// but only <0 if an error occurred; in that case, the error
// message must be set here.
int8_t command_execute(uint8_t channel_no, bus_t *bus, errormsg_t *errormsg,
					void (*callback)(int8_t errnum, uint8_t *rxdata)) {

        cmd_t *command = &(bus->command);
        rtconfig_t *rtconf = &(bus->rtconf);

	debug_printf("COMMAND: %s\n", (char*)&(command->command_buffer));

	parse_filename(command, &nameinfo, 1);

#ifdef DEBUG_CMD
        debug_printf("CMD=%s\n", nameinfo.cmd == CMD_NONE ? "-" : command_to_name(nameinfo.cmd));
        debug_printf("DRIVE=%c\n", nameinfo.drive == 0xff ? '-' : nameinfo.drive + 0x30);
        debug_printf("NAME=%s\n", (char*)nameinfo.name);
        debug_puts("ACCESS="); debug_putc(isprint(nameinfo.access) ? nameinfo.access : '-'); debug_putcrlf();
        debug_puts("TYPE="); debug_putc(isprint(nameinfo.type) ? nameinfo.type : '-'); debug_putcrlf();
#endif
        // post-parse

	// pass-through commands
	// those are just being passed to the provider
	if (nameinfo.cmd == CMD_RENAME 
		|| nameinfo.cmd == CMD_SCRATCH
		|| nameinfo.cmd == CMD_CD
		|| nameinfo.cmd == CMD_MKDIR
		|| nameinfo.cmd == CMD_RMDIR) {

		uint8_t type = 0;
		switch(nameinfo.cmd) {
		case CMD_RENAME:
			type = FS_RENAME;
			break;
		case CMD_SCRATCH:
			type = FS_DELETE;
			break;
		case CMD_CD:
			type = FS_CHDIR;
			break;
		case CMD_MKDIR:
			type = FS_MKDIR;
			break;
		case CMD_RMDIR:
			type =FS_RMDIR;
			break;
		default:
			// should not happen, all if() conditions are accounted for
        	        debug_puts("ILLEGAL COMMAND!");
        	        set_error(errormsg, ERROR_SYNTAX_UNKNOWN);
			return ERROR_FAULT;
		}

		return file_submit_call(channel_no, type, errormsg, rtconf, callback);
	} else
	if (nameinfo.cmd == CMD_ASSIGN) {

		if (nameinfo.drive == NAMEINFO_UNUSED_DRIVE) {
			// no drive
        	        set_error(errormsg, ERROR_DRIVE_NOT_READY);
			return -1;
		}

		// the +1 on the name skips the endpoint number stored in position 0	
		if (provider_assign(nameinfo.drive, (char*) nameinfo.name+1) < 0) {
		
			return file_submit_call(channel_no, FS_ASSIGN, errormsg, rtconf, callback);
		} else {
			// need to unlock the caller by calling the callback function
			callback(ERROR_OK, NULL);
		}
		return 0;
	} else
	if (nameinfo.cmd == CMD_INITIALIZE) {
		debug_puts("INITIALIZE");
		// need to unlock the caller by calling the callback function
		callback(ERROR_OK, NULL);
		return 0;
	} else
	if (nameinfo.cmd == CMD_EXT) {
		debug_puts("CONFIGURATION EXTENSION");
		int8_t rv = rtconfig_set(rtconf, (char*) command->command_buffer);
		callback(rv, NULL);
		return 0;
	}

	// need to have the error message set when returning <0
        set_error(errormsg, ERROR_SYNTAX_UNKNOWN);
	return -1;
}

