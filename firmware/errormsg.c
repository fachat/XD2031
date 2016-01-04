/* 
   XD2031 - Serial line file server for CBMs
   Copyright (C) 2012 Andre Fachat <afachat@gmx.de>
 
   Generating the DOS error message
   Inspired by sd2iec by Ingo Korb, but rewritten in the end 

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License only.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#include <string.h>
#include <ctype.h>

#include "archcompat.h"
#include "errormsg.h"
#include "channel.h"
#include "version.h"    /* for SW_NAME */
#include "hwdefines.h"  /* for HW_NAME */
#include "debug.h"
#include "term.h"
#include "led.h"

#undef	DEBUG_ERROR

/// Version number string, will be added to message 73
const char IN_ROM versionstr[] = SW_NAME " V" VERSION LONGVERSION "/" HW_NAME;

const char IN_ROM STR_OK[]                   = " OK";
const char IN_ROM STR_SCRATCHED[]            = " FILES SCRATCHED";
const char IN_ROM STR_READ[]                 = "READ ERROR";
const char IN_ROM STR_WRITE_PROTECT[]        = "WRITE PROTECT ERROR";
const char IN_ROM STR_WRITE_ERROR[]          = "WRITE ERROR";
const char IN_ROM STR_SYNTAX_ERROR[]         = "SYNTAX ERROR";
const char IN_ROM STR_FILE_NOT_FOUND[]       = " FILE NOT FOUND";
const char IN_ROM STR_FILE_NOT_OPEN[]        = " FILE NOT OPEN";
const char IN_ROM STR_FILE_EXISTS[]          = " FILE EXISTS";
const char IN_ROM STR_FILE_TYPE_MISMATCH[]   = " FILE TYPE MISMATCH";
const char IN_ROM STR_DIR_NOT_SUPPORTED[]    = "DIR NOT SUPPORTED";
const char IN_ROM STR_DIR_NOT_EMPTY[]        = "DIR NOT EMPTY";
const char IN_ROM STR_DIR_NOT_FOUND[]        = "DIR NOT FOUND";
const char IN_ROM STR_NO_PERMISSION[]        = "NO PERMISSION";
const char IN_ROM STR_FAULT[]                = "GENERAL FAULT";
const char IN_ROM STR_NO_CHANNEL[]           = "NO CHANNEL";
const char IN_ROM STR_DIR_ERROR[]            = "DIR ERROR";
const char IN_ROM STR_DISK_FULL[]            = " DISK FULL";
const char IN_ROM STR_DRIVE_NOT_READY[]      = "DRIVE NOT READY";
const char IN_ROM STR_NO_BLOCK[]             = "NO BLOCK";
const char IN_ROM STR_ILLEGAL_T_OR_S[]       = "ILLEGAL TRACK OR SECTOR";
const char IN_ROM STR_OVERFLOW_IN_RECORD[]   = "OVERFLOW IN RECORD";
const char IN_ROM STR_RECORD_NOT_PRESENT[]   = " RECORD NOT PRESENT";
const char IN_ROM STR_TOO_LARGE[]            = "FILE TOO LARGE";
const char IN_ROM STR_EMPTY[]                = "";

struct err_struct {
  uint8_t    code;
  const char *str;
};

const struct err_struct IN_ROM err_tab[] = {
	{CBM_ERROR_OK                   , STR_OK                   },
	{CBM_ERROR_SCRATCHED            , STR_SCRATCHED            },
	{CBM_ERROR_READ                 , STR_READ                 },
	{CBM_ERROR_WRITE_PROTECT        , STR_WRITE_PROTECT        },
	{CBM_ERROR_WRITE_ERROR          , STR_WRITE_ERROR          },
	{CBM_ERROR_SYNTAX_UNKNOWN       , STR_SYNTAX_ERROR         },
	{CBM_ERROR_SYNTAX_PATTERN       , STR_SYNTAX_ERROR         },
	{CBM_ERROR_SYNTAX_NONAME        , STR_SYNTAX_ERROR         },
	{CBM_ERROR_SYNTAX_INVAL         , STR_SYNTAX_ERROR         },
	{CBM_ERROR_SYNTAX_DIR_SEPARATOR , STR_SYNTAX_ERROR         },
	{CBM_ERROR_FILE_NAME_TOO_LONG   , STR_SYNTAX_ERROR         },
	{CBM_ERROR_FILE_NOT_FOUND       , STR_FILE_NOT_FOUND       },
	{CBM_ERROR_FILE_NOT_OPEN        , STR_FILE_NOT_OPEN        },
	{CBM_ERROR_FILE_EXISTS          , STR_FILE_EXISTS          },
	{CBM_ERROR_FILE_TYPE_MISMATCH   , STR_FILE_TYPE_MISMATCH   },
	{CBM_ERROR_DIR_NOT_SUPPORTED    , STR_DIR_NOT_SUPPORTED    },
	{CBM_ERROR_DIR_NOT_EMPTY        , STR_DIR_NOT_EMPTY        },
	{CBM_ERROR_DIR_NOT_FOUND        , STR_DIR_NOT_FOUND        },
	{CBM_ERROR_NO_PERMISSION        , STR_NO_PERMISSION        },
	{CBM_ERROR_FAULT                , STR_FAULT                },
	{CBM_ERROR_NO_CHANNEL           , STR_NO_CHANNEL           },
	{CBM_ERROR_DIR_ERROR            , STR_DIR_ERROR            },
	{CBM_ERROR_DISK_FULL            , STR_DISK_FULL            },
	{CBM_ERROR_DRIVE_NOT_READY      , STR_DRIVE_NOT_READY      },
	{CBM_ERROR_NO_BLOCK             , STR_NO_BLOCK             },
	{CBM_ERROR_ILLEGAL_T_OR_S       , STR_ILLEGAL_T_OR_S       },
	{CBM_ERROR_OVERFLOW_IN_RECORD   , STR_OVERFLOW_IN_RECORD   },
	{CBM_ERROR_RECORD_NOT_PRESENT   , STR_RECORD_NOT_PRESENT   },
	{CBM_ERROR_TOO_LARGE            , STR_TOO_LARGE            },
	{CBM_ERROR_DOSVERSION           , versionstr               }
};

const char *errmsg(const uint8_t code) {  // lookup error text
	uint8_t i;

	for (i=0; i < sizeof(err_tab) / sizeof(struct err_struct); i++) {
		if (code == rom_read_byte( (uint8_t *) &err_tab[i].code))
			return (const char *) rom_read_pointer (&err_tab[i].str);
	}
	return STR_EMPTY;            // default if code not in table
}	

void set_error_tsd(errormsg_t *err, uint8_t errornum, uint8_t track, uint8_t sector, int8_t drive) {
	char *msg = (char *)err->error_buffer;
	err->errorno = errornum;
  	err->readp = 0;

	rom_sprintf(msg, IN_ROM_STR("%2.2d,"), errornum);	// error number
	rom_strcat(msg, errmsg(errornum));			// error message from flash memory
	if (drive < 0) {
		rom_sprintf(msg + strlen(msg), IN_ROM_STR(",%2.2d,%2.2d\r"), track%100, sector%100); // track & sector
	} else {
		rom_sprintf(msg + strlen(msg), IN_ROM_STR(",%2.2d,%2.2d,%1.1d\r"), track, sector, drive); // track & sector & drive
	}

	if (errornum != CBM_ERROR_OK         &&
	    errornum != CBM_ERROR_DOSVERSION &&
	    errornum != CBM_ERROR_SCRATCHED) {
		led_set(ERROR);
		term_printf("Setting status to: %s\n", err->error_buffer);
	} else {
		led_set(OFF); // same as idle, but clears error
	}

#ifdef DEBUG_ERROR
debug_printf("Set status to: %s\n", err->error_buffer);
#endif
}

void set_status(errormsg_t *err, char* s) {
	strncpy( (char*) err->error_buffer, s, CONFIG_ERROR_BUFFER_SIZE - 1);
	err->error_buffer[CONFIG_ERROR_BUFFER_SIZE - 1] = 0;
	err->readp = 0;
	err->errorno = 0;
}
