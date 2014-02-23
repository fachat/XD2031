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
#include "rtconfig.h"
#include "rtconfig2.h"
#include "cmdnames.h"

#ifdef HAS_RTC
#include "rtc.h"
#endif

#include "debug.h"

#undef	DEBUG_CMD


// note: this does not return an actual error code, 
// but only <0 if an error occurred; in that case, the error
// message must be set here.
int8_t command_execute(uint8_t channel_no, bus_t *bus, errormsg_t *errormsg,
					void (*callback)(int8_t errnum, uint8_t *rxdata)) {

        cmd_t *command = &(bus->command);
        rtconfig_t *rtconf = &(bus->rtconf);
	int8_t rv = 0;

	debug_printf("COMMAND: %s\n", (char*)&(command->command_buffer));

	parse_filename(command, &nameinfo, PARSEHINT_COMMAND);

#ifdef DEBUG_CMD
        debug_printf("CMD=%s\n", nameinfo.cmd == command_to_name(nameinfo.cmd));
        debug_printf("DRIVE=%c\n", nameinfo.drive == NAMEINFO_UNUSED_DRIVE ? '-' :
                                (nameinfo.drive == NAMEINFO_UNDEF_DRIVE ? '*' :
                                nameinfo.drive + 0x30));
        debug_printf("NAME='%s' (%d)\n", (nameinfo.name == NULL) ? "" : (char*)nameinfo.name, 
				nameinfo.namelen);
        debug_printf("DRIVE2=%c\n", nameinfo.file[0].drive == NAMEINFO_UNUSED_DRIVE ? '-' :
                                (nameinfo.file[0].drive == NAMEINFO_UNDEF_DRIVE ? '*' :
                                nameinfo.file[0].drive + 0x30));
        debug_printf("NAME2='%s' (%d)\n", (nameinfo.file[0].name == NULL) ? "" : (char*)nameinfo.file[0].name,
				nameinfo.file[0].namelen);
        debug_puts("ACCESS="); debug_putc(isprint(nameinfo.access) ? nameinfo.access : '-'); debug_putcrlf();
        debug_puts("TYPE="); debug_putc(isprint(nameinfo.type) ? nameinfo.type : '-'); debug_putcrlf();
#endif
        // post-parse

	switch (nameinfo.cmd) {
		case CMD_INITIALIZE: // If a drive number is given, set the last used drive
			if (command->command_buffer[1])
            rtconf->last_used_drive = command->command_buffer[1] - 0x30;
		case CMD_RENAME:
		case CMD_SCRATCH:
		case CMD_CD:
		case CMD_MKDIR:
		case CMD_RMDIR:
      case CMD_VALIDATE:
      case CMD_COPY:
      case CMD_DUPLICATE:
      case CMD_NEW:
			// pass-through commands
			// those are just being passed to the provider
			// nameinfo cmd enum definition such that wireformat matches it
			return file_submit_call(channel_no, nameinfo.cmd, command->command_buffer, errormsg, rtconf, callback, 1);
		case CMD_ASSIGN:
			if (nameinfo.drive == NAMEINFO_UNUSED_DRIVE) {
				// no drive
				set_error(errormsg, CBM_ERROR_DRIVE_NOT_READY);
				return -1;
			}

			if (provider_assign( nameinfo.drive, (char*) nameinfo.name, (char*) nameinfo.file[0].name ) < 0) {
				return file_submit_call(channel_no, FS_ASSIGN, command->command_buffer, errormsg, rtconf, callback, 1);
			}
			break;
		case CMD_UX:
			rv = cmd_user(bus, (char*) command->command_buffer, errormsg);
			break;
		case CMD_BLOCK:
			rv = cmd_block(bus, (char*) command->command_buffer, errormsg);
			break;
		case CMD_EXT:
			rv = rtconfig_set(rtconf, (char*) command->command_buffer);
			break;
		case CMD_POSITION:
			rv = bufcmd_position(bus, (char*) nameinfo.name, nameinfo.namelen, errormsg);
			break;
#ifdef HAS_RTC
		case CMD_TIME:
			rv = rtc_time( (char*) command->command_buffer, errormsg);
			if (rv < 0) rv = 0; // OK, RTC was updated
			break;
#endif
		default:
			rv = -1;
	}
	if (rv >= 0) {
		callback(rv, NULL);
		return 0;
	}
	// need to have the error message set when returning <0
        set_error(errormsg, CBM_ERROR_SYNTAX_UNKNOWN);
	return -1;
}
