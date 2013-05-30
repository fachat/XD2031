
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

// I know this is awkward, but I currently don't have another idea
// to avoid including byte.h everywhere 
#include "byte.h"

// charset number is defined by supported charset table, -1 is unsupported
typedef signed char charset_t;

// get the charset number from the character set name
charset_t cconv_getcharset(const char *charsetname);

// convert from input buffer (with length inlen) to output buffer (of length outlen)
// must work with in = out buffer (i.e. in place conversion), zero-bytes in the input
// that must be converted to zero in the output (so multiple strings are converted 
// in one call), 
typedef void (*charconv_t)(const char *in, const BYTE inlen, char *out, const BYTE outlen);

// fallback
//charconv_t cconv_identity;
void cconv_identity(const char *in, const BYTE inlen, char *out, const BYTE outlen);

// get a converter from one charset to another
charconv_t cconv_converter(charset_t from, charset_t to);

#endif

