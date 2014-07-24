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
#include <stdbool.h>

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
 * classic Commodore pattern matching:
 * 	"*" - only works as the last pattern char and matches everything
 * 	      further chars in the pattern are ignored
 * 	"?" - single character is ignored
 *
 * outpattern points into the second compare string (the pattern)
 */

static int8_t classic_dirmatch(const char *x, const char *y, const char **outpattern) {
	int i = 0;	// current position
	char a,b;

	printf("classic dir-enabled match between '%s' (%02x) and '%s' (%02x)\n", 
				(char*)x, x[0], (char*)y, y[0]);

	while(1) {
		a = x[i];
		b = y[i];
		*outpattern = y+i;
		if (a ==  0  && b ==  0 ) return 1; // match
		if (a == '*' || b == '*') {
			// move on to path separator (if any)
			while (**outpattern && **outpattern != '/') {
				(*outpattern)++;
			}
			return 1; // match
		}
		if (a ==  0  && b == '/') return 1;	// match
		i++;
		if (a == '?' || b == '?') continue; // wild letter
		if (a != b) break;
	}
	return 0;
}


/**
 * advanced pattern matching (CBM 1581):
 *	"*" - matches any number of characters (including empty string)
 *	      further chars in the pattern must also match
 *	"?" - single character is ignored, but required
 */

static int8_t advanced_match(const char *name, const char *pattern) {
	bool match = true;
	const char *after_name_ptr = NULL;
	const char *after_pattern_ptr = NULL;
	char n, p;

	while (1) {
		n = *name;
		p = *pattern;
		if (!n) {
			if (!p) break;
			if (p == '*') {
				pattern++;
				continue;
			}
			if (after_name_ptr) {
				if (!(*after_name_ptr)) {
					match = false;
					break;
				}
				name = after_name_ptr++;
				pattern = after_pattern_ptr;
				continue;
			}
			match = false;
			break;
		} else {
			if ((n != p) && (p != '?')) {
				if (p == '*') {
					after_pattern_ptr = ++pattern;
					after_name_ptr = name;
					p = *pattern;
					if (!p) break;
					continue;
				}
				if (after_pattern_ptr) {
					if (after_pattern_ptr != pattern) {
						pattern = after_pattern_ptr;
						p = *pattern;
						if (n == p) pattern++;
					}
					name++;
					continue;
				} else {
					match = false;
					break;
				}
			}
		}
		name++;
		pattern++;
	}
	return match;
}


/**
 * compares the given name to the given pattern
 * and returns true if it matches.
 * Both names are null-terminated
 */

int8_t compare_pattern(const char *name, const char *pattern, bool advanced_wildcards) {
	if (advanced_wildcards)
		return advanced_match(name, pattern);
	else
		return classic_match(name, pattern);
}


/**
 * compares the given name to the given pattern
 * and returns true if it matches.
 * Both names are null-terminated, but if the name is finished, 
 * and the pattern ends with a path separator, the name still matches.
 * Also returns the rest of the pattern in the outpattern pointer
 */

int8_t compare_dirpattern(const char *name, const char *pattern, const char **outpattern) {
	// 1581 matching style not yet implemented
	// so do always a "classic" matching:
	return classic_dirmatch(name, pattern, outpattern);
}
