
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

#ifndef CHARCONVERT_H
#define	CHARCONVERT_H

#include <inttypes.h>
#include <stdbool.h>

#include "petscii.h"

typedef uint16_t unic_t;

/** 
 * to unicode
 */
static inline unic_t petscii_to_unic(const char **ptr) {
        unic_t c = petscii_to_ascii(**ptr);
        (*ptr)++;       // petscii is single char
        return c;
}

/** 
 * to unicode
 */
static inline unic_t isolatin1_to_unic(const char **ptr) {
        unic_t c = **ptr;
        (*ptr)++;       // iso-8859-1 is single char
        return c;
}


// charset number is defined by supported charset table, -1 is unsupported
typedef signed char charset_t;

// get the charset number from the character set name
// return -1 for an unsupported character set
charset_t cconv_getcharset(const char *charsetname);

// to avoid costly lookups and wasted memory, those are pre-defined here
#define	CHARSET_ASCII		0
#define	CHARSET_PETSCII		1

#define CHARSET_ASCII_NAME	"ASCII"
#define	CHARSET_PETSCII_NAME	"PETSCII"

// convert from input buffer (with length inlen) to output buffer (of length outlen)
// must work with in = out buffer (i.e. in place conversion), zero-bytes in the input
// that must be converted to zero in the output (so multiple strings are converted 
// in one call), 
typedef int (*charconv_t) (const char *in, const uint8_t inlen, char *out,
			    const uint8_t outlen);

// compare in1 and in2 and return 0 on equal.
typedef int (*charcomp_t) (const char *in1, const uint8_t in1len, char *in2,
			    const uint8_t in2len);

// compare in1 and in2 and return 0 on equal.
typedef int (*charmatch_t) (const char **pattern, const char **tomatch,
			    uint8_t advanced);

// get a converter from one charset to another
charconv_t cconv_converter(charset_t from, charset_t to);

// get a comparator between two charsets
charcomp_t cconv_comparator(charset_t from, charset_t to);

// get a matcher between two charsets
charmatch_t cconv_matcher(charset_t pattern, charset_t tomatch);

// get a const pointer to the string name of the character set
const char *cconv_charsetname(charset_t cnum);

// scan the given pattern until a delimiter character is reached
// during scan, check whether a character in match is found
// return the length until the delimiter (patter[l]==delim), zero
// if not found. Return as negative value when a match is found.
const char *cconv_scan(const char *pattern, charset_t cset, char delim, const char *match, bool *matched);

#endif
