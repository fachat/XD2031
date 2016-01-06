/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2012 Andre Fachat

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

#ifndef WILDCARD_H
#define	WILDCARD_H

#define	CBM_PATH_SEPARATOR_CHAR 	'/'
#define	CBM_PATH_SEPARATOR_STR	 	"/"

/**
 * compares the given name to the given pattern
 * and returns true if it matches.
 * Both names are null-terminated
 */
int8_t compare_pattern(const char *name, const char *pattern,
		       bool advanced_wildcards);

/**
 * compares the given name to the given pattern
 * and returns true if it matches.
 * Both names are null-terminated, but if the name is finished, 
 * and the pattern ends with a path separator, the name still matches.
 * Also returns the rest of the pattern in the outpattern pointer
 */
int8_t compare_dirpattern(const char *name, const char *pattern,
			  const char **outpattern);

#endif
