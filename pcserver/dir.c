/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2012 Andre Fachat

    Derived from:
    OS/A65 Version 1.3.12
    Multitasking Operating System for 6502 Computers
    Copyright (C) 1989-1997 Andre Fachat

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

****************************************************************************/

#include "os.h"

#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "provider.h"
#include "charconvert.h"
#include "wireformat.h"

#ifndef min
#define min(a,b)        (((a)<(b))?(a):(b))
#endif

/**
 * fill in the buffer with a directory entry from a direntry_t struct
 */
int dir_fill_entry_from_direntry(char *dest, charset_t outcset, int driveno, direntry_t *de, int maxsize) {
	struct tm *tp;

	ssize_t size = de->size;
	if (de->mode == FS_DIR_MOD_NAM) {
		size = driveno;
	}
	// TODO: overflow check if ssize_t has more than 32 bits
        dest[FS_DIR_LEN] = size & 255;
        dest[FS_DIR_LEN+1] = (size >> 8) & 255;
        dest[FS_DIR_LEN+2] = (size >> 16) & 255;
        dest[FS_DIR_LEN+3] = (size >> 24) & 255;

        tp = localtime(&(de->moddate));
        dest[FS_DIR_YEAR]  = tp->tm_year;
        dest[FS_DIR_MONTH] = tp->tm_mon;
        dest[FS_DIR_DAY]   = tp->tm_mday;
        dest[FS_DIR_HOUR]  = tp->tm_hour;
        dest[FS_DIR_MIN]   = tp->tm_min;
        dest[FS_DIR_SEC]   = tp->tm_sec;

	log_debug("dir_fill_entry: type=%02x, attr=%02x\n", de->type, de->attr);

        dest[FS_DIR_MODE] = de->mode;
        dest[FS_DIR_ATTR] = de->attr | de->type;

	// file name
       	int l = 0;
	if (de->name == NULL) {
		// blocks free
		dest[FS_DIR_NAME] = 0;
	} else {

		log_debug("Converting DIR entry from %s to %s\n", 
			cconv_charsetname(de->cset), cconv_charsetname(outcset));
		charconv_t converter = cconv_converter(de->cset, outcset);


        	l = strlen((const char*)de->name);
		int n = converter((char*)de->name, l, dest+FS_DIR_NAME, maxsize-1-FS_DIR_NAME);
		// make sure we're still null-terminated
		dest[min(FS_DIR_NAME+n, maxsize-1)] = 0;
	}
	// character set conversion
      	l = strlen(dest+FS_DIR_NAME);

	return FS_DIR_NAME + l + 1;
}



