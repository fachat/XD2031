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

#include <stdio.h>
#include <string.h>

#include "packet.h"
#include "petscii.h"
#include "version.h"

#include "debug.h"
/*

#include "provider.h"
#include "wireformat.h"
#include "serial.h"
#include "uarthw.h"

#include "led.h"
*/


/*
 * helper for conversion of ASCII to PETSCII
 */

static uint8_t *append(uint8_t *outp, const char *to_append) {
        while(*to_append != 0) {
                *outp = ascii_to_petscii(*to_append);
                outp++;
                to_append++;
        }
        *outp = 0;
        return outp;
}


static uint8_t out[64];

int8_t directory_converter(packet_t *p, uint8_t drive) {
	uint8_t *inp = NULL;
	uint8_t *outp = &(out[0]);

	if (p == NULL) {
		debug_puts("P IS NULL!");
		return -1;
	}

	inp = packet_get_buffer(p);
	uint8_t type = inp[FS_DIR_MODE];
	uint8_t attribs = inp[FS_DIR_ATTR];

	//packet_update_wp(p, 2);

	if (type == FS_DIR_MOD_NAM) {
		*outp = 1; outp++;	// load address low
		*outp = 4; outp++;	// load address high
	}

	*outp = 1; outp++;		// link address low; will be overwritten on LOAD
	*outp = 1; outp++;		// link address high

	uint16_t lineno = 0;
	if (type == FS_DIR_MOD_NAM) {
		lineno = drive;
	} else {
		// line number, derived from file length
		if (inp[FS_DIR_LEN+3] != 0 
			|| inp[FS_DIR_LEN+2] > 0xf9
			|| (inp[FS_DIR_LEN+2] == 0xf9 && inp[FS_DIR_LEN+1] == 0xff && inp[FS_DIR_LEN] != 0)) {
			// more than limit of 63999 blocks
			lineno = 63999;
		} else {
			lineno = inp[FS_DIR_LEN+1] | (inp[FS_DIR_LEN+2] << 8);
			if (inp[FS_DIR_LEN] != 0) {
				lineno++;
			}
		}
	}
	*outp = lineno & 255; outp++;
	*outp = (lineno>>8) & 255; outp++;

	//snprintf(outp, 5, "%hd", (unsigned short)lineno);
	//outp++;
	if (lineno < 10) { *outp = ' '; outp++; }
	if (type == FS_DIR_MOD_NAM) {
		*outp = 0x12;	// reverse for disk name
		outp++;
	} else {
		if (type != FS_DIR_MOD_FRE) {
			if (lineno < 100) { *outp = ' '; outp++; }
			if (lineno < 1000) { *outp = ' '; outp++; }
			if (lineno < 10000) { *outp = ' '; outp++; }
		}
	}

	if (type != FS_DIR_MOD_FRE) {
		*outp = '"'; outp++;
		uint8_t i = FS_DIR_NAME;
		// note the check i<16 - this is buffer overflow protection
		// file names longer than 16 chars are not displayed
		while ((inp[i] != 0) && (i < (FS_DIR_NAME + 16))) {
			*outp = ascii_to_petscii(inp[i]);
			outp++;
			i++;
		}
		// note: not counted in i
		*outp = '"'; outp++;
	
		// fill up with spaces, at least one space behind filename
		while (i < FS_DIR_NAME + 16 + 1) {
			*outp = ' '; outp++;
			i++;
		}
	}

	// add file type
	if (type == FS_DIR_MOD_NAM) {
		// file name entry
		outp = append(outp, SW_NAME_LOWER);
		//strcpy(outp, SW_NAME_LOWER);
		//outp += strlen(SW_NAME_LOWER)+1;	// includes ending 0-byte
	} else
	if (type == FS_DIR_MOD_DIR) {
		outp = append(outp, "dir");
		//strcpy(outp, "dir");
		//outp += 4;	// includes ending 0-byte
	} else
	if (type == FS_DIR_MOD_FIL) {
		if (attribs & FS_DIR_ATTR_SPLAT) {
			*(outp-1) = '*';
		}
		const char *ftypes[] = { "del", "seq", "prg", "usr", "rel" };
		uint8_t ftype = attribs & FS_DIR_ATTR_TYPEMASK;
		if (ftype >= 0 && ftype < 5) {
			outp = append(outp, ftypes[ftype]);
		} else {
			outp = append(outp, "---");
		}
		if (attribs & FS_DIR_ATTR_LOCKED) {
			*outp = '<';
			outp++;
			*outp = 0;
		}
	} else
	if (type == FS_DIR_MOD_FRE) {
		outp = append(outp, "blocks free");
		//strcpy(outp, "bytes free");
		//outp += 11;	// includes ending 0-byte

		outp++; *outp = 0; 	// BASIC end marker (zero link address)
		outp++; *outp = 0; 	// BASIC end marker (zero link address)
	}

	// outp points to last (null) byte to be transmitted, thus +1
	uint8_t len = outp - out + 1;
	if (len > packet_get_capacity(p)) {
		debug_puts("CONVERSION NOT POSSIBLE!"); debug_puthex(len); debug_putcrlf();
		return -1;	// conversion not possible
	}

#ifdef DEBUG_SERIAL
	debug_puts("CONVERTED TO: LEN="); debug_puthex(len);
	for (uint8_t j = 0; j < len; j++) {
		debug_putc(' '); debug_puthex(out[j]);
	} 
	debug_putcrlf();
#endif
	// this should probably be combined
	memcpy(packet_get_buffer(p), &out, len);
	packet_update_wp(p, len);

	return 0;
}

/**
 * convert PETSCII names to ASCII names
 */
int8_t to_provider(packet_t *p) {
	uint8_t *buf = packet_get_buffer(p);
	uint8_t len = packet_get_contentlen(p);
//debug_printf("CONVERT: len=%d, b=%s\n", len, buf);
	while (len > 0) {
//debug_puts("CONVERT: "); 
//debug_putc(*buf); //debug_puthex(*buf);debug_puts("->");
		*buf = petscii_to_ascii(*buf);
//debug_putc(*buf);debug_puthex(*buf);debug_putcrlf();
		buf++;
		len--;
	}
	return 0;
}


