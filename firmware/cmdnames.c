/**************************************************************************

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

***************************************************************************/

#include <string.h>
#include <ctype.h>
#include "cmdnames.h"

#ifdef __AVR__
#include <avr/pgmspace.h>
#else
#define PROGMEM
#define memcpy_P memcpy
static inline uint8_t pgm_read_byte(uint8_t *a) { return *a; }
#endif


#define STRLEN 7 /* max length of command name without zero-terminator */
#define TABLEN (sizeof(cmd_tab) / sizeof(struct cmd_struct))

struct cmd_struct {
	const char name[STRLEN + 1];
	uint8_t    cmd;
};

// This table resides in flash memory

static const struct cmd_struct PROGMEM cmd_tab[] = {
	{"-"      , CMD_NONE       }, // command_to_name starts here
	{"?"      , CMD_SYNTAX     },
	{"@"      , CMD_OVERWRITE  },
	{"$"      , CMD_DIR        },
	{"ASSIGN" , CMD_ASSIGN     }, // command_find starts here
	{"BLOCK"  , CMD_BLOCK      },
	{"COPY"   , CMD_SYNTAX     }, // not yet supported but not to confuse with 'CD'
	{"CD"     , CMD_CD         },
	{"CHDIR"  , CMD_CD         },
	{"INIT"   , CMD_INITIALIZE },
	{"M-"     , CMD_SYNTAX     }, // not yet supported but not to confuse with 'MD'
	{"MKDIR"  , CMD_MKDIR      },
	{"MD"     , CMD_MKDIR      },
	{"P"      , CMD_POSITION   },
	{"RENAME" , CMD_RENAME     },
	{"RMDIR"  , CMD_RMDIR      },
	{"RD"     , CMD_RMDIR      },
	{"SCRATCH", CMD_SCRATCH    },
	{"TIME"   , CMD_TIME       },
	{"USER"   , CMD_UX         },
	{"X"      , CMD_EXT        },
};

/**************************************************************************
	This parser accepts abbreviations of the commands the same way as
	the Commodore drives do. So 'S', 'SC', 'SCR', 'SCRA' etc. are all
	recognized as the command "SCRATCH'.
	Single letter commands like 'R' for 'RENAME' have to precede other
	commands starting with the same letter. So 'R' is correctly parsed
	as 'RENAME' and not as 'RMDIR'.
***************************************************************************/

command_t command_find(uint8_t *input, uint8_t *len) {
	uint8_t i, j, b;

	if (input[0] == '$') {  // special non-alpha command
		*len = 1;
		return CMD_DIR;
	}
	for (i=4; i < TABLEN; i++) {
		for (j=0; j <= STRLEN; j++) {
			b = pgm_read_byte((uint8_t *)&cmd_tab[i].name[j]);
			if (b == 0 || !isalpha(input[j])) { // comparison ends
				*len = j;
				if (j == 0) return CMD_SYNTAX;    // no valid input
				return pgm_read_byte((uint8_t *)&cmd_tab[i].cmd);
			}
			if (b != input[j]) break;     // strings are not equal
		}
	}
	*len = 0;
	return CMD_SYNTAX;                      // command not recognized
}

// cmd_string gets a copy of the command string residing in flash memory

static char cmd_string[STRLEN + 1];

const char *command_to_name(command_t cmd) {
	uint8_t i;

	strcpy(cmd_string, "-");          // Initialize with CMD_NONE
	for (i=0; i < TABLEN; i++) {
		if (cmd == pgm_read_byte( (uint8_t *) &cmd_tab[i].cmd)) {
			memcpy_P(cmd_string, cmd_tab[i].name, STRLEN + 1);
			break;
		}
	}
	return cmd_string;
}



#ifdef PCTEST

#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
	char line[256];
	uint8_t len;
	uint8_t cmd;

	while (fgets(line, 256, stdin)) {
		if (strlen(line)) line[strlen(line) - 1] = 0; // drop '\n'
		else printf("\n");
		if (line[0] == '#') printf("\n%s\n", line);
		else {
			printf ("Input:  '%s'\n", line);
			cmd = command_find( (uint8_t *) line, &len);
			printf("Parsed: cmd = %3d   len = %2d   ", cmd, len);
			printf("<%s>\n\n", (char *) command_to_name(cmd));
		}
	}
	return 0;
}
#endif
