/****************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2013 Andre Fachat, Nils Eilers

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
#include <inttypes.h>

#include "name.h"
#include "cmd.h"

#ifdef PCTEST
#define debug_flush() fflush(stdout)
#define term_rom_puts(x) printf("%s", x)
#define debug_putcrlf() printf("\n")
#define PROGMEM
#define nullstring "<NULL>"
#else
#include "debug.h"
#endif

#define	NAME_DRIVE	0
#define	NAME_NAME	1
#define	NAME_OPTS	2
#define	NAME_COMMAND	4
#define	NAME_CMDDRIVE	5
#define	NAME_FILE	6
#define	NAME_DRIVE2	7
#define	NAME_NAME2	8
#define	NAME_RELPAR	9

#if defined(DEBUG_NAME) || defined(PCTEST)
static const char name_state[][9] PROGMEM = {
	"DRIVE", "NAME", "OPTS", "-?- ", "COMMAND", "CMDDRIVE", "FILE",
	"DRIVE2", "NAME2", "RELPAR"
};
#endif

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
void parse_filename(cmd_t *in, nameinfo_t *result, uint8_t parsehint) {

	int8_t len = in->command_length;	//  includes the zero-byte

	// copy over command to the end of the buffer, so we can 
	// construct it from the parts at the beginning after parsing it
	// (because we may need to insert bytes at some places, which would
	// be difficult)
	// Note that assembling takes place in assemble_filename_packet below.
	uint8_t diff = CONFIG_COMMAND_BUFFER_SIZE - len;
	memmove(in->command_buffer + diff, in->command_buffer, len);

	// adjust so we exclude final null byte
	len--;

	// runtime vars (uint e.g. to avoid sign extension on REL file record len)
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
	result->recordlen = 0;
	result->recordno = 0;
	if (parsehint & PARSEHINT_COMMAND) {
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
#if defined(DEBUG_NAME) || defined(PCTEST)
		printf("len=%d, curr=%c, state=", len, *p);
		if(state == 3 || state > 9) {
			term_rom_puts(name_state[3]);
			printf("%02X", state);
		} else term_rom_puts(name_state[state]);
		debug_putcrlf();
#endif
		switch(state) {
		case NAME_COMMAND:
			// save first char as potential command	
			if (result->cmd == CMD_NONE) {
				if (isalpha(*p)) {
					result->cmd = command_find(p);
					if (result->cmd == CMD_POSITION) {
						// the position command is fully binary
						result->name = p+1;
						result->namelen = len - 1;
						return;
					}
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
			if (ch == '$' && (parsehint & PARSEHINT_LOAD)) {
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
			if (parsehint & PARSEHINT_COMMAND) {
				if (ch == '=') {
					*p = 0;	// end first file name
					result->namelen = (p-result->name);
					
					result->drive2 = NAMEINFO_UNUSED_DRIVE;
					result->name2 = (p+1);
					result->namelen2 = len-1;
					// Some commands take the string "as is":
					if ((result->cmd == CMD_ASSIGN) || (result->cmd == CMD_TIME)) {
						state = NAME_NAME2; // quit parser
					} else {
						state = NAME_DRIVE2;
					}
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
				state = NAME_NAME2;
				result->name2 = p+1;
				result->namelen2 = len-1;
				break;
			} else {
				drv = NAMEINFO_UNDEF_DRIVE;
			}
			break;
		case NAME_NAME2:
			len = 0; // we're done
			break;
		case NAME_OPTS:
			// options can be used in any order, but each type only once
			if ((ch == 'P' || ch == 'S' || ch == 'U') && result->type == 0) {
				result->type = ch;
			} else
			if ((ch == 'R' || ch == 'W' || ch == 'A' || ch == 'X') && result->access == 0) {
				result->access = ch;
			} else
			if (ch == 'N') {
				result->options |= NAMEOPT_NONBLOCKING;
			} else
			if (ch == 'L') {
				result->type = ch;
				state = NAME_RELPAR;
			}
			// note the ',' are just stepped over
			break;
		case NAME_RELPAR:
			// separator after the "L" in the filename open
			if (ch == ',') {
				// CBM is single byte format, but we "integrate" the closing null-byte
				// to allow two byte record lengths
				len--;
				p++;
				result->recordlen = *p;
				if (*p != 0) {
					len--;
					p++;
					result->recordlen |= (*p << 8);
				}
				break;
			}
		default:
			break;
		}
		len--;
		p++;
	}

	if (result->access == 0) {
		if (result->type == 'L') {
			// if access is not set on a REL file, it's read & write
			result->access = 'X';
		} else {
			result->access = 'R';
		}
	}

#if defined(DEBUG_NAME) || defined(PCTEST)
	printf("CMD=%s\n", result->cmd == CMD_NONE ? "-" : command_to_name(result->cmd));
	printf("DRIVE=%c\n", result->drive == NAMEINFO_UNUSED_DRIVE ? '-' :
				(result->drive == NAMEINFO_UNDEF_DRIVE ? '*' :
				result->drive + 0x30));
	printf("NAME='%s' (%d)\n", result->name ? (char*)result->name : nullstring, result->namelen);
	printf("ACCESS=%c\n", result->access);
	printf("TYPE=%c\n", result->type);
	printf("NAME2='%s' (%d)\n", result->name2 ? (char*)result->name2 : nullstring, result->namelen2);
	printf("DRIVE2=%c\n", result->drive2 == NAMEINFO_UNUSED_DRIVE ? '-' :
				(result->drive2 == NAMEINFO_UNDEF_DRIVE ? '*' :
				result->drive2 + 0x30));
	printf("RECLEN=%d\n", result->recordlen);
	debug_flush();
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
 *
 * TODO: check for buffer overflow
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

	// those areas may overlap
	memmove((char*)p, (char*)nameinfo->name, nameinfo->namelen + 1);
	// let p point to the byte after the null byte
	p += nameinfo->namelen + 1;

	// it's either two file names (like MOVE), or one file name with parameters (for OPEN_*)
	if (nameinfo->namelen2 != 0) {
		// two file names
		*p = nameinfo->drive2;
		p++;

		memmove((char*)p, (char*)nameinfo->name2, nameinfo->namelen2 + 1);
		p += nameinfo->namelen2 + 1;
	} else 
	if (nameinfo->type != 0) {
		// parameters are comma-separated lists of "<name>'='<values>", e.g. "T=S"
		*(p++) = 'T';
		*(p++) = '=';
		*(p++) = nameinfo->type;
		if (nameinfo->recordlen > 0) {
			sprintf((char*)p, "%d", nameinfo->recordlen);
			p = p + strlen((char*)p);
		}
		*(p++) = 0;
	}

	return p-trg;
}



#ifdef PCTEST

#define MAX_LINE 255
int main(int argc, char** argv) {
    char line[MAX_LINE + 1]; int had_a_comment = 1;
    cmd_t in;
    nameinfo_t result;
    uint8_t parsehint = PARSEHINT_COMMAND;

    while(fgets(line, MAX_LINE, stdin) != NULL) {
        line[strlen(line) - 1] = 0; // drop '\n'
        if(line[0] == '#') {
            if(!had_a_comment) puts("\n");
            puts(line);
            had_a_comment=1;
            continue;
        } else if(line[0] == '!') {
            printf("%s\n", line);
            if(!strcmp(line, "!PARSEHINT_COMMAND")) parsehint = PARSEHINT_COMMAND;
            else if(!strcmp(line, "!PARSEHINT_LOAD")) parsehint = PARSEHINT_LOAD;
            else printf("*** SYNTAX ERROR: '%s'\n", line);
            continue;
        } else {
            if(!had_a_comment) puts("\n");
            had_a_comment=0;
            printf("Test: '%s'\n", line);
        }
        // --------------------------------------
        strcpy((char*)in.command_buffer, line);
        in.command_length = strlen(line) + 1; // length including zero-byte
        printf("parsehint: ");
        if(parsehint == PARSEHINT_COMMAND) printf("PARSEHINT_COMMAND\n");
        else if(parsehint == PARSEHINT_LOAD) printf("PARSEHINT_LOAD\n");
        else printf("%d ?\n", parsehint);
        parse_filename(&in, &result, parsehint);
    }

    return 0;
}
#endif
