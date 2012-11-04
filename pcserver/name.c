/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2012 Andre Fachat
    Copyright (C) 2012 Nils Eilers

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

#include <string.h>

#include "name.h"

/**
 * compares the given name to the given pattern
 * and returns true if it matches.
 * Both names are null-terminated
 *
 * This implementation is roughly based on the Commodore semantics
 * of "*" and "?":
 * Commodore:
 * 	"*" - only works as the last pattern char and matches everything
 * 	      further chars in the pattern are ignored
 * 	"?" - single character is ignored
 */
int compare_pattern(const char *name, const char *pattern) {

	int p = 0;		// current position

	do {
		if (pattern[p] == '*') {
			// For Commodore, we are basically done here - anything else does not count
			return 1;
		} else
		if (pattern[p] != '?') {
			if (pattern[p] != name[p]) {
				// not equal
				return 0;
			}
		}
	} while (name[p] && pattern[p++]);

	if(name[p] == 0 && pattern[p] == 0) {
		// both, name and pattern are finished, and
		// not exited so far, so both match
		return 1;
	}

	// no match
	return 0;
}
