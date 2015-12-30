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
#include "provider.h"

#include "debug.h"

#undef	DEBUG_DIR


//#define	MAX_LINE_NUMBER		63999
#define	MAX_LINE_NUMBER		65535

/*
 * helper for conversion of ASCII to PETSCII
 */

static uint8_t *append(charconv_t converter, uint8_t *outp, const char *to_append) {
	int l = strlen(to_append);
	converter(to_append, l+1, (char*) outp, l+1);
//        while(*to_append != 0) {
//                *outp = *to_append; //ascii_to_petscii(*to_append);
//                outp++;
//                to_append++;
//        }
//        *outp = 0;
        return outp + l;
}


static uint8_t out[64];

int8_t directory_converter(endpoint_t *ep, packet_t *p, uint8_t drive) {

	// convert from provider to PETSCII
	charconv_t converter = cconv_converter(ep->provider->charset(ep->provdata), CHARSET_PETSCII);
	charconv_t asciiconv = cconv_converter(CHARSET_ASCII, CHARSET_PETSCII);

	uint8_t *inp = NULL;
	uint8_t *outp = &(out[0]);

	if (p == NULL) {
		debug_puts("P IS NULL!");
		return -1;
	}

	inp = packet_get_buffer(p);
	uint8_t type = inp[FS_DIR_MODE];
	uint8_t attribs = inp[FS_DIR_ATTR];


	if (type == FS_DIR_MOD_NAM) {
		*outp = 1; outp++;	// load address low
		*outp = 4; outp++;	// load address high
	}

	*outp = 1; outp++;		// link address low; will be overwritten on LOAD
	*outp = 1; outp++;		// link address high

	uint16_t lineno = 0;

	if (type == FS_DIR_MOD_NAM || type == FS_DIR_MOD_NAS) {
		lineno = drive;
	} else {
		uint16_t in[4];
		uint16_t tmp[4];
	
		in[0] = inp[FS_DIR_LEN] + (inp[FS_DIR_LEN + 1] << 8);
		in[1] = inp[FS_DIR_LEN + 1] + (inp[FS_DIR_LEN + 2] << 8);
		in[2] = inp[FS_DIR_LEN + 2] + (inp[FS_DIR_LEN + 3] << 8);
		in[3] = inp[FS_DIR_LEN + 3];

		if (in[3] > 0) {
			lineno = MAX_LINE_NUMBER;
		} else {

			// first add 253, so that the "leftover" bytes in the remainder
			// are counted as an own block
			in[0] += 253;
			if (((in[0] >> 8) & 0xff) != inp[FS_DIR_LEN + 1]) {
				// there was a carry
				in[1] += 1;
				if (((in[1] >> 8) & 0xff) != inp[FS_DIR_LEN + 2]) {
					// there was a carry
					in[2] += 1;
					if (((in[2] >> 8) & 0xff) != inp[FS_DIR_LEN + 3]) {
						// there was a carry
						in[3] += 1;
					}
				}
			}

			// first term "1"
			tmp[0] = in[0] & 0xff;
			tmp[1] = in[1] & 0xff;
			tmp[2] = in[2] & 0xff;
			tmp[3] = in[3] & 0xff;

			// if estimate, don't adjust
			// this is to allow a D64 provider to just set the directory entry
			// block number into the second and third byte and get the same
			// value here
			if ((attribs & FS_DIR_ATTR_ESTIMATE) == 0) {

				// to get from 256 byte blocks to 254 byte blocks, we multiply
				// by 256/254, which is the same as (1+1/127). So we now add 1/127th
				// which is 1/127 = 1/128 + 1/(128^2) + 1/(128^3) + 1/(128^4) + ...

				// second term "1/128" = "1/(1<<7); note in[] contains the "high byte" as well
				tmp[0] += (in[0] >> 7) & 0xff;
				tmp[1] += (in[1] >> 7) & 0xff;
				tmp[2] += (in[2] >> 7) & 0xff;
				tmp[3] += (in[3] >> 7) & 0xff;
	
				// third term "1/(128^2)" = "1/16384" = "1/(1<<14)"	
				tmp[0] += ((in[1] >> 6) & 0x03) + ((in[2] << 2) & 0xfc);
				tmp[1] += ((in[2] >> 6) & 0x03) + ((in[3] << 2) & 0xfc);
				tmp[2] += ((in[3] >> 6) & 0x03);

				// fourth term "1/(128^3)" = "1/(1<<21)"	
				tmp[0] += ((in[2] >> 5) & 0x07) + ((in[3] << 3) & 0xf8);
				tmp[1] += ((in[3] >> 5) & 0x07);

				// fifth term "1/(128^4)" = "1/(1<<28)"	
				tmp[0] += ((in[3] >> 4) & 0x0f);

				// add one "rest" for the missing terms
				tmp[0] += 1;

				// adjust carry bits
				tmp[1] += (tmp[0] >> 8) & 0xff;
				tmp[2] += (tmp[1] >> 8) & 0xff;
				tmp[3] += (tmp[2] >> 8) & 0xff;
			}
			// now compute the line number
			if (tmp[3] > 0) {
				lineno = MAX_LINE_NUMBER;
			} else {
				lineno = (tmp[1] & 0xff) | ((tmp[2] & 0xff) << 8);
			}

			// restrict line number
#			if MAX_LINE_NUMBER < 65535
				if (lineno > MAX_LINE_NUMBER) lineno = MAX_LINE_NUMBER;
#			endif

		}
	}
	*outp = lineno & 255; outp++;
	*outp = (lineno>>8) & 255; outp++;

	if (type == FS_DIR_MOD_NAM || type == FS_DIR_MOD_NAS) {
		*outp = 0x12;	// reverse for disk name
		outp++;
	} else {
		if (type != FS_DIR_MOD_FRE && type != FS_DIR_MOD_FRS) {
			if (lineno < 10) { *outp = ' '; outp++; }
			if (lineno < 100) { *outp = ' '; outp++; }
			if (lineno < 1000) { *outp = ' '; outp++; }
			//if (lineno < 10000) { *outp = ' '; outp++; }
		}
	}

	if (type != FS_DIR_MOD_FRE && type != FS_DIR_MOD_FRS) {
		*outp = '"'; outp++;
		uint8_t i = FS_DIR_NAME;
		// note the check i<16 - this is buffer overflow protection
		// file names longer than 16 chars are not displayed
		int n = strlen((char*)inp+i);	// includes null-byte (terminator)
		int l = n;
		if (l > 16) {
			l = 16;
		}
		converter((char*)(inp+i), l+1, (char*)outp, l+1);
		outp += l;
		i += l;
//		while ((inp[i] != 0) && (i < (FS_DIR_NAME + 16))) {
//			*outp = ascii_to_petscii(inp[i]);
//			outp++;
//			i++;
//		}
		// note: not counted in i
		*outp = '"'; outp++;

		if ((type == FS_DIR_MOD_NAM || type == FS_DIR_MOD_NAS) && n > l) {
			*outp = ' '; outp++;
			l = n - 16;
			if (l > 5) {
				l = 5;
			}
			converter((char*)(inp+i), l+1, (char*)outp, l+1);
			outp += l;
			i += l;
		} else
		if (type == FS_DIR_MOD_NAM || type == FS_DIR_MOD_NAS) {
			// file name entry
			outp = append(asciiconv, outp, SW_NAME_LOWER);
		} else {

			// fill up with spaces, at least one space behind filename
			while (i < FS_DIR_NAME + 16 + 1) {
				*outp = ' '; outp++;
				i++;
			}
		}
	}


	// add file type
	if (type == FS_DIR_MOD_DIR) {
		outp = append(asciiconv, outp, "dir  ");
	} else
	if (type == FS_DIR_MOD_FIL) {
		if (attribs & FS_DIR_ATTR_SPLAT) {
			*(outp-1) = '*';
		}
		const char *ftypes[] = { "del", "seq", "prg", "usr", "rel" };
		uint8_t ftype = attribs & FS_DIR_ATTR_TYPEMASK;
		if (ftype < 5) {
			outp = append(asciiconv, outp, ftypes[ftype]);
		} else {
			outp = append(asciiconv, outp, "---");
		}
		*outp++ = (attribs & FS_DIR_ATTR_LOCKED) ? '<' : ' ';
		*outp = ' '; outp++;
		
		// spaces after file type compensating for block size
		if (lineno > 10) { *outp = ' '; outp++; }
		if (lineno > 100) { *outp = ' '; outp++; }
		if (lineno > 1000) { *outp = ' '; outp++; }
	} else
	if (type == FS_DIR_MOD_FRE || type == FS_DIR_MOD_FRS) {
		outp = append(asciiconv, outp, "blocks free."); 
		memset(outp, ' ', 13); outp += 13;

		if (type != FS_DIR_MOD_FRS) {
			*outp = 0; outp++; 	// BASIC end marker (zero link address)
			*outp = 0; outp++; 	// BASIC end marker (zero link address)
		}
	}

	*outp = 0;

	// outp points to last (null) byte to be transmitted, thus +1
	uint8_t len = outp - out + 1;
	if (len > packet_get_capacity(p)) {
		debug_puts("CONVERSION NOT POSSIBLE!"); debug_puthex(len); debug_putcrlf();
		return -1;	// conversion not possible
	}

#ifdef DEBUG_DIR
	debug_printf("CONVERTED, LEN = %d = 0x%02X\n", len, len);
	debug_hexdump(out, len, 1);
#endif

	// this should probably be combined
	memcpy(packet_get_buffer(p), &out, len);
	packet_update_wp(p, len);

	return 0;
}


