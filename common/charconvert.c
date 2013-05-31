
/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2013 Andre Fachat

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

/*
 * character conversion routines
 */

#include <inttypes.h>
#include <string.h>
#include <stdio.h>

#include "byte.h"
#include "charconvert.h"
#include "petscii.h"

#define	NUM_OF_CHARSETS		2

static char *charsets[] = { 
	"ASCII", "PETSCII"
};

// get a const pointer to the string name of the character set
const char *cconv_charsetname(charset_t cnum) {
	if (cnum < 0 || cnum >= NUM_OF_CHARSETS) {
		return NULL;
	}
	return charsets[cnum];
}

// get the charset number from the character set name
charset_t cconv_getcharset(const char *charsetname) {
	for (int i = 0; i < NUM_OF_CHARSETS; i++) {
		if (strcmp(charsets[i], charsetname) == 0) {
			return i;
		}
	}
	return -1;
}


// conversion functions
void cconv_identity(const char *in, const BYTE inlen, char *out, const BYTE outlen) {
	//printf("cconv_identity(%s)\n", in);
	if (in != out) {
		// not an in-place conversion
		// assumption: callers behave in terms of buffer lengths
		memcpy(out, in, outlen);
	}
}

static void cconv_ascii2petscii(const char *in, const BYTE inlen, char *out, const BYTE outlen) {
	//printf("cconv_ascii2petscii(%s)\n", in);
	BYTE i = 0;
	while (i < inlen && i < outlen) {
		*(out++) = ascii_to_petscii(*(in++));
		i++;
	}
}

static void cconv_petscii2ascii(const char *in, const BYTE inlen, char *out, const BYTE outlen) {
	//printf("cconv_petscii2ascii(%s)\n", in);
	BYTE i = 0;
	while (i < inlen && i < outlen) {
		*(out++) = petscii_to_ascii(*(in++));
		i++;
	}
}

// this table is ordered such that the outer index defines where to convert FROM,
// and the inner index defines where to convert TO 
static charconv_t convtable[NUM_OF_CHARSETS][NUM_OF_CHARSETS] = {
	{ 	// from ASCII
		cconv_identity,		cconv_ascii2petscii
	}, {	// from PETSCII
		cconv_petscii2ascii,	cconv_identity
	}
};

// get a converter from one charset to another
charconv_t cconv_converter(charset_t from, charset_t to) {
	//printf("Getting converter from %d to %d\n", from, to);
	if (from < 0 || from >= NUM_OF_CHARSETS) {
		return cconv_identity;
	}
	if (to < 0 || to >= NUM_OF_CHARSETS) {
		return cconv_identity;
	}
	return convtable[from][to];
}



