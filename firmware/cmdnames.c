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


#include "cmdnames.h"

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
	case 'T':
		return CMD_TIME;
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

