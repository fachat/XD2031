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
#include "mem.h"
#include "name.h"
#include "openpars.h"

// ************
// process options from the optional OPEN parameter string
// ************

void openpars_process_options(const uint8_t *opts, openpars_t *pars) { 
        const uint8_t *p = opts;
        uint8_t typechar;
        int reclenw;
        int n;
        const uint8_t *t;

	pars->filetype = FS_DIR_TYPE_UNKNOWN;
	pars->recordlen = 0;

	if (p == NULL) {
		// no type given, so any may match
		return;
	}


        while (*p != 0) {
                switch(*(p++)) {
                case 't':
                case 'T':
                        if (*(p++) == '=') {
                                typechar = *(p++);
                                switch(typechar) {
                                case 'u':
                                case 'U':       
					pars->filetype = FS_DIR_TYPE_USR; 
					break;
                                case 'P':
                                case 'p':       
					pars->filetype = FS_DIR_TYPE_PRG; 
					break;
                                case 'S':
                                case 's':       
					pars->filetype = FS_DIR_TYPE_SEQ; 
					break;
                                case 'L':
                                case 'l':
                                        pars->filetype = FS_DIR_TYPE_REL;
                                        n=sscanf((char*)p, "%d", &reclenw);
                                        if (n == 1 && reclenw > 0 && reclenw < (2<<16)) {
                                                pars->recordlen = reclenw;
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
	log_debug("openpars options: %s -> type=%d, reclen=%d\n", opts, pars->filetype, pars->recordlen);
}

/**
 * fill in default values 
 */
void openpars_init_options(openpars_t *pars) {

	pars->filetype = FS_DIR_TYPE_UNKNOWN;
	pars->recordlen = 0;
}



