
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

#include "archcompat.h"
#include "charconvert.h"
#include "petscii.h"
#include "wildcard.h"

#define	NUM_OF_CHARSETS		2

#define min(a,b)	(((a)<(b))?(a):(b))

// ------------------------------------------------------------------------------

static const char IN_ROM CSET_ASCII_NAME_P[]	= CHARSET_ASCII_NAME;
static const char IN_ROM CSET_PETSCII_NAME_P[]	= CHARSET_PETSCII_NAME;

static char const* const IN_ROM charsets[] = { 
	CSET_ASCII_NAME_P, 
	CSET_PETSCII_NAME_P
};

static unic_t (IN_ROM *to_unic_funcs[])(const char **ptr) = {
	isolatin1_to_unic,
	petscii_to_unic
};

// ------------------------------------------------------------------------------

// scan the given pattern until a delimiter character is reached
// during scan, check whether a character in match is found
// return the pointer to the char after the delimiter when found, NULL otherwise.
// if not found. Set *matched if character is found in match parameter 
const char *cconv_scan(const char *pattern, charset_t cset, char delim, const char *match, bool *matched) {

	*matched = false;
	unic_t (*to_unic)(const char **ptr) = to_unic_funcs[cset];
	unic_t c;

	while ( (c = to_unic(&pattern)) != 0) {
		if (c == delim) {
			return pattern;
		}
		if (strchr(match, c)) {
			*matched = true;
		}
	}
	return NULL;
}

// ------------------------------------------------------------------------------

// get a const pointer to the string name of the character set
const char *cconv_charsetname(charset_t cnum) {
	if (cnum < 0 || cnum >= NUM_OF_CHARSETS) {
		return NULL;
	}
	return (const char *) rom_read_pointer (&charsets[cnum]);
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


// ------------------------------------------------------------------------------
// conversion functions
int cconv_identity(const char *in, const uint8_t inlen, char *out, const uint8_t outlen) {
	//printf("cconv_identity(%s)\n", in);
	if (in != out) {
		// not an in-place conversion
		memcpy(out, in, min(inlen, outlen));
		return min(inlen, outlen);
	}
	return 0;
}

static int cconv_ascii2petscii(const char *in, const uint8_t inlen, char *out, const uint8_t outlen) {
	//printf("cconv_ascii2petscii(%s)\n", in);
	uint8_t i = 0;
	while (i < inlen && i < outlen) {
		*(out++) = ascii_to_petscii(*(in++));
		i++;
	}
	return i;
}

static int cconv_petscii2ascii(const char *in, const uint8_t inlen, char *out, const uint8_t outlen) {
	//printf("cconv_petscii2ascii(%s)\n", in);
	uint8_t i = 0;
	while (i < inlen && i < outlen) {
		*(out++) = petscii_to_ascii(*(in++));
		i++;
	}
	return i;
}

// ------------------------------------------------------------------------------
// match functions

int cmatch_identity(const char **pattern, const char **tomatch, const uint8_t advanced) {
	return match_pattern(pattern, isolatin1_to_unic, tomatch, isolatin1_to_unic, advanced);
}

int cmatch_asciipetscii(const char **pattern, const char **tomatch, const uint8_t advanced) {
	return match_pattern(pattern, isolatin1_to_unic, tomatch, petscii_to_unic, advanced);
}

int cmatch_petsciiascii(const char **pattern, const char **tomatch, const uint8_t advanced) {
	return match_pattern(pattern, petscii_to_unic, tomatch, isolatin1_to_unic, advanced);
}

// ------------------------------------------------------------------------------


// this table is ordered such that the outer index defines where to convert FROM,
// and the inner index defines where to convert TO 
static const IN_ROM struct {
	charconv_t conv;
	charmatch_t match;
} convtable[NUM_OF_CHARSETS][NUM_OF_CHARSETS] = {
	{ 	// from ASCII
		{ cconv_identity, cmatch_identity }, { cconv_ascii2petscii, cmatch_asciipetscii }
	}, {	// from PETSCII
		{ cconv_petscii2ascii, cmatch_petsciiascii }, { cconv_identity, cmatch_identity }
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
	return (charconv_t) rom_read_pointer(&convtable[from][to].conv);
}

// get a comparator between charsets
charmatch_t cconv_matcher(charset_t from, charset_t to) {
	//printf("Getting converter from %d to %d\n", from, to);
	if (from < 0 || from >= NUM_OF_CHARSETS) {
		return cmatch_identity;
	}
	if (to < 0 || to >= NUM_OF_CHARSETS) {
		return cmatch_identity;
	}
	return convtable[from][to].match;
}



