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
#include <inttypes.h>

#include "cmdnames.h"
#include "archcompat.h"

#define STRLEN 10 /* max length of command name without zero-terminator */
#define TABLEN (sizeof(cmd_tab) / sizeof(struct cmd_struct))

struct cmd_struct {
	const char name[STRLEN + 1];
	uint8_t    cmd;
};

// This table resides in flash memory

static const struct cmd_struct IN_ROM cmd_tab[] = {
	{"-"         , CMD_NONE       }, // command_to_name starts here
	{"?"         , CMD_SYNTAX     },
	{"@"         , CMD_OVERWRITE  },
	{"$"         , CMD_DIR        },
	{"ASSIGN"    , CMD_ASSIGN     }, // command_find starts here
	{"BLOCK"     , CMD_BLOCK      },
	{"COPY"      , CMD_COPY       },
	{"CD"        , CMD_CD         },
	{"CHDIR"     , CMD_CD         },
   	{"DUPLICATE" , CMD_DUPLICATE  },
	{"INITIALIZE", CMD_INITIALIZE },
	{"M-"        , CMD_SYNTAX     }, // not yet supported but not to confuse with 'MD'
	{"MKDIR"     , CMD_MKDIR      },
	{"MD"        , CMD_MKDIR      },
   	{"NEW"       , CMD_NEW        },
	{"P"         , CMD_POSITION   },
	{"RENAME"    , CMD_RENAME     },
	{"RMDIR"     , CMD_RMDIR      },
	{"RD"        , CMD_RMDIR      },
	{"SCRATCH"   , CMD_SCRATCH    },
	{"TIME"      , CMD_TIME       },
	{"U"         , CMD_UX         },
   	{"VALIDATE"  , CMD_VALIDATE   },
	{"X"         , CMD_EXT        },
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
			b = rom_read_byte((uint8_t *)&cmd_tab[i].name[j]);
			if (b == 0 || !isalpha(input[j])) { // comparison ends
				*len = j;
				if (j == 0) return CMD_SYNTAX;    // no valid input
				return rom_read_byte((uint8_t *)&cmd_tab[i].cmd);
			}
			if (b != input[j]) break;     // strings are not equal
		}
	}
	*len = 0;
	return CMD_SYNTAX;                      // command not recognized
}

// cmd_string gets a copy of the command string residing in flash memory

static char cmd_string[STRLEN + 1];

// TODO: evaluate second table for non-command messages (like FS_READ etc)
const char *command_to_name(command_t cmd) {
	uint8_t i;

	strcpy(cmd_string, "-");          // Initialize with CMD_NONE
	for (i=0; i < TABLEN; i++) {
		if (cmd == rom_read_byte( (uint8_t *) &cmd_tab[i].cmd)) {
#ifdef SERVER
			return cmd_tab[i].name;
#else
			rom_memcpy(cmd_string, cmd_tab[i].name, STRLEN + 1);
			break;
#endif
		}
	}
	return cmd_string;
}
