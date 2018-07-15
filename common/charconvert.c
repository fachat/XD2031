
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

#include "charconvert.h"
#include "petscii.h"

#define	NUM_OF_CHARSETS		2

#define min(a,b)	(((a)<(b))?(a):(b))

static char *charsets[] = { 
	CHARSET_ASCII_NAME, CHARSET_PETSCII_NAME
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

#if 0
// ------------------------------------------------------------------------------
// comparator functions

int ccomp_samecset(const char *in1, const uint8_t in1len, char *in2, const uint8_t in2len) {
	if (in1len != in2len) {
		return in1len - in2len;
	}
	return strncmp(in1, in2, in1len);
}

static int ccomp_asciipetscii(const char *in1, const uint8_t in1len, char *in2, const uint8_t in2len) {
	uint8_t i = 0;
	if (in1len != in2len) {
		return in1len - in2len;
	}
	while (i < in1len) {
		uint8_t c1 = ascii_to_petscii(*(in1++));
		uint8_t c2 = *(in2++);
		i++;
		if (c1 != c2) {
			return i;
		}
	}
	return 0;
}

static int ccomp_petsciiascii(const char *in1, const uint8_t in1len, char *in2, const uint8_t in2len) {
	uint8_t i = 0;
	if (in1len != in2len) {
		return in1len - in2len;
	}
	while (i < in1len) {
		uint8_t c1 = *(in1++);
		uint8_t c2 = ascii_to_petscii(*(in2++));
		i++;
		if (c1 != c2) {
			return i;
		}
	}
	return 0;
}
#endif

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
static struct {
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
	return convtable[from][to].conv;
}

#if 0
// get a comparator between charsets
charcomp_t cconv_comparator(charset_t from, charset_t to) {
	//printf("Getting converter from %d to %d\n", from, to);
	if (from < 0 || from >= NUM_OF_CHARSETS) {
		return cconv_identity;
	}
	if (to < 0 || to >= NUM_OF_CHARSETS) {
		return cconv_identity;
	}
	return convtable[from][to].comp;
}
#endif

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



