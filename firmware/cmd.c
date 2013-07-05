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
#include "bufcmd.h"
#include "file.h"
#include "name.h"
#include "wireformat.h"

#include "debug.h"

#undef	DEBUG_CMD


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
		if (*(input+1) == 'D' || *(input+1) == 'K') {
			// MKDIR or MD
			return CMD_MKDIR;
		}
		// here we would have M-R/M-W/M-E
		return CMD_SYNTAX;
		break;
	case 'A':
		return CMD_ASSIGN;
		break;
	case 'U':
		return CMD_UX;
		break;
	case 'B':
		return CMD_BLOCK;
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
                return "INIT";
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
        case CMD_UX:
                return "USER";
                break;
        case CMD_BLOCK:
                return "BLOCK";
                break;
	case CMD_EXT:
		return "EXT";
		break;
     	case CMD_OVERWRITE:
		return "@";
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

	parse_filename(command, &nameinfo, PARSEHINT_COMMAND);

#ifdef DEBUG_CMD
        debug_printf("CMD=%s\n", nameinfo.cmd == CMD_NONE ? "-" : command_to_name(nameinfo.cmd));
        debug_printf("DRIVE=%c\n", nameinfo.drive == NAMEINFO_UNUSED_DRIVE ? '-' :
                                (nameinfo.drive == NAMEINFO_UNDEF_DRIVE ? '*' :
                                nameinfo.drive + 0x30));
        debug_printf("NAME='%s' (%d)\n", (nameinfo.name == NULL) ? "" : (char*)nameinfo.name, 
				nameinfo.namelen);
        debug_printf("DRIVE2=%c\n", nameinfo.drive2 == NAMEINFO_UNUSED_DRIVE ? '-' :
                                (nameinfo.drive2 == NAMEINFO_UNDEF_DRIVE ? '*' :
                                nameinfo.drive2 + 0x30));
        debug_printf("NAME2='%s' (%d)\n", (nameinfo.name2 == NULL) ? "" : (char*)nameinfo.name2,
				nameinfo.namelen2);
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

		// nameinfo cmd enum definition such that wireformat matches it
		return file_submit_call(channel_no, nameinfo.cmd, command->command_buffer, 
			errormsg, rtconf, callback, 1);
	} else
	if (nameinfo.cmd == CMD_ASSIGN) {

		if (nameinfo.drive == NAMEINFO_UNUSED_DRIVE) {
			// no drive
        	        set_error(errormsg, CBM_ERROR_DRIVE_NOT_READY);
			return -1;
		}

		if (provider_assign( nameinfo.drive, (char*) nameinfo.name, 
				     (char*) nameinfo.name2 ) < 0) {
		
			return file_submit_call(channel_no, FS_ASSIGN, command->command_buffer,
				errormsg, rtconf, callback, 1);
		} else {
			// need to unlock the caller by calling the callback function
			callback(CBM_ERROR_OK, NULL);
		}
		return 0;
	} else
	if (nameinfo.cmd == CMD_INITIALIZE) {
		debug_puts("INITIALIZE\n");
		// need to unlock the caller by calling the callback function
		callback(CBM_ERROR_OK, NULL);
		return 0;
	} else
	if (nameinfo.cmd == CMD_UX) {
		debug_puts("USER COMMAND\n");
		int8_t rv = cmd_user(bus, (char*) command->command_buffer, errormsg);
		if (rv >= 0) {
			callback(rv, NULL);
			return 0;
		}
		return rv;	// waiting for callback
	} else
	if (nameinfo.cmd == CMD_BLOCK) {
		debug_puts("BLOCK COMMAND\n");
		int8_t rv = cmd_block(bus, (char*) command->command_buffer, errormsg);
		if (rv >= 0) {
			callback(rv, NULL);
			return 0;
		}
		return rv;	// waiting for callback
	} else
	if (nameinfo.cmd == CMD_EXT) {
		debug_puts("CONFIGURATION EXTENSION\n");
		int8_t rv = rtconfig_set(rtconf, (char*) command->command_buffer);
		callback(rv, NULL);
		return 0;
	}

	// need to have the error message set when returning <0
        set_error(errormsg, CBM_ERROR_SYNTAX_UNKNOWN);
	return -1;
}

