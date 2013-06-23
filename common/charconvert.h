
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

// charset number is defined by supported charset table, -1 is unsupported
typedef signed char charset_t;

// get the charset number from the character set name
// return -1 for an unsupported character set
charset_t cconv_getcharset(const char *charsetname);

// to avoid costly lookups and wasted memory, those are pre-defined here
#define	CHARSET_ASCII		0
#define	CHARSET_PETSCII		1

// convert from input buffer (with length inlen) to output buffer (of length outlen)
// must work with in = out buffer (i.e. in place conversion), zero-bytes in the input
// that must be converted to zero in the output (so multiple strings are converted 
// in one call), 
typedef void (*charconv_t)(const char *in, const uint8_t inlen, char *out, const uint8_t outlen);

// fallback
//charconv_t cconv_identity;
void cconv_identity(const char *in, const uint8_t inlen, char *out, const uint8_t outlen);

// get a converter from one charset to another
charconv_t cconv_converter(charset_t from, charset_t to);

// get a const pointer to the string name of the character set
const char *cconv_charsetname(charset_t cnum);

#endif

