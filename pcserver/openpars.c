/* 
    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2012  Andre Fachat <afachat@gmx.de>

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


#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "errors.h"
#include "wireformat.h"
#include "log.h"

// ************
// process options from the optional OPEN parameter string
// ************

void openpars_process_options(uint8_t *opts, uint8_t *type, uint16_t *reclen) {
        uint8_t *p = opts;
        uint8_t typechar;
        int reclenw;
        int n;
        uint8_t *t;

	if (p == NULL) {
		type = FS_DIR_TYPE_PRG;
		reclen = 0;
		return;
	}

	log_debug("openpars options: %s\n", opts);

        while (*p != 0) {
                switch(*(p++)) {
                case 't':
                        if (*(p++) == '=') {
                                typechar = *(p++);
                                switch(typechar) {
                                case 'u':       *type = FS_DIR_TYPE_USR; break;
                                case 'p':       *type = FS_DIR_TYPE_PRG; break;
                                case 's':       *type = FS_DIR_TYPE_SEQ; break;
                                case 'l':
                                        *type = FS_DIR_TYPE_REL;
                                        n=sscanf((char*)p, "%d", &reclenw);
                                        if (n == 1 && reclenw > 0 && reclenw < (2<<16)) {
                                                *reclen = reclenw;
                                        }
                                        t = (uint8_t*) strchr((char*)p, ',');
                                        if (t == NULL) {
                                                t = p + strlen((char*)p);
                                        }
                                        p = t;
                                        break;
                                default:
                                        log_warn("Unknown open file type option %c\n", typechar);
                                        break;
                                }
                        }
                        break;
                case ',':
                        p++;
                        break;
                default:
                        // syntax error
                        log_warn("error parsing file open options %s\n", opts);
                        return;
                }
        }
}

