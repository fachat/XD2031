/**************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2013 Andre Fachat

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

***************************************************************************/

#include <string.h>
#include "cmdnames.h"

command_t command_find(uint8_t *input, uint8_t *len) {
	*len = 1; // true for most commands
	char *n = (char*) input + 1;
	char c = *n;
	switch(*input) {
	case '$':
		return CMD_DIR;
		break;
	case 'I':
		return CMD_INITIALIZE;
		break;
	case 'R':
		if (c == 'D') {			// RD
			*len = 2;
			return CMD_RMDIR;
		}
		if (!strncmp(n, "MDIR", 4)) {	// RMDIR
			*len = 5;
			return CMD_RMDIR;
		}
		return CMD_RENAME;
		break;
	case 'S':
		return CMD_SCRATCH;
		break;
	case 'T':
		if (c == 'I') {			// TI
			*len = 2;
		}				// T
		return CMD_TIME;
		break;
	case 'C':
		if (c == 'D') {			// CD
			*len = 2;
			return CMD_CD;
		}
		if (!strncmp(n, "HDIR", 4)) {	// CHDIR
			*len = 5;
			return CMD_CD;
		}
		// this would be the COPY command
		return CMD_SYNTAX;
		break;
	case 'M':
		if (c == 'D') {			// MD
			*len = 2;
			return CMD_MKDIR;
		}
		if (!strncmp(n, "KDIR", 4)) {	// MKDIR
			*len = 5;
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
		if(c) *len = 2;			// XD, XU
		else return CMD_SYNTAX;
		if (!strncmp(n, "RESET", 5)) *len = 6;	// XRESET
		return CMD_EXT;
		break;
	case 'P':
		return CMD_POSITION;
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
	case CMD_TIME:
		return "TIME";
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
	case CMD_POSITION:
		return "P";
		break;
   }
        return "";
}

