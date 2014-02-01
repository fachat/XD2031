/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2013 Andre Fachat, Edilbert Kirk, Nils Eilers

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

#include <string.h>
#include <inttypes.h>
#include <stdio.h>

#include "wildcard.h"


/**
 * classic Commodore pattern matching:
 * 	"*" - only works as the last pattern char and matches everything
 * 	      further chars in the pattern are ignored
 * 	"?" - single character is ignored, but required
 */

static int8_t classic_match(const char *name, const char *pattern) {
	int i = 0;	// current position
	char n, p;

	//printf("classic match between '%s' and '%s'\n", (char*)x, (char*)y);

	while(1) {
		n = name[i];
		p = pattern[i++];
		if (n ==  0  && p ==  0 ) return 1; // match
		if (p == '*')             return 1; // match
		if (p == '?' && n)        continue; // wild letter
		if (n != p) break;
	}
	return 0;
}


/**
 * compares the given name to the given pattern
 * and returns true if it matches.
 * Both names are null-terminated
 */

int8_t compare_pattern(const char *name, const char *pattern) {
	// 1581 matching style not yet implemented
	// so do always a "classic" matching:
	return classic_match(name, pattern);
}
