/* 
   XD2031 - Serial line file server for CBMs
   Copyright (C) 2012, 2018 Andre Fachat <afachat@gmx.de>
 
   DOS error messages

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

#include <inttypes.h>

#include "archcompat.h"
#include "errors.h"
#include "version.h"
#include "errnames.h"

struct err_struct {
  uint8_t    code;
  const char *str;
};

#ifndef HW_NAME
#define HW_NAME	"<unknown>"
#endif

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

const struct err_struct IN_ROM err_tab[] = {
	{CBM_ERROR_OK                   , STR_OK                   },
	{CBM_ERROR_SCRATCHED            , STR_SCRATCHED            },
	{CBM_ERROR_READ                 , STR_READ                 },
	{CBM_ERROR_WRITE_PROTECT        , STR_WRITE_PROTECT        },
	{CBM_ERROR_WRITE_ERROR          , STR_WRITE_ERROR          },
	{CBM_ERROR_SYNTAX_UNKNOWN       , STR_SYNTAX_ERROR         },
	{CBM_ERROR_SYNTAX_NOCMD       	, STR_SYNTAX_ERROR         },
	{CBM_ERROR_SYNTAX_PATTERN       , STR_SYNTAX_ERROR         },
	{CBM_ERROR_SYNTAX_NONAME        , STR_SYNTAX_ERROR         },
	{CBM_ERROR_SYNTAX_INVAL         , STR_SYNTAX_ERROR         },
	{CBM_ERROR_SYNTAX_DIR_SEPARATOR , STR_SYNTAX_ERROR         },
	{CBM_ERROR_SYNTAX_WILDCARDS 	, STR_SYNTAX_ERROR         },
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
	uint8_t l = sizeof(err_tab) / sizeof(struct err_struct);

	for (i=0; i < l; i++) {
		if (code == rom_read_byte( (uint8_t *) &err_tab[i].code))
			return (const char *) rom_read_pointer (&err_tab[i].str);
	}
	return STR_EMPTY;            // default if code not in table
}	


