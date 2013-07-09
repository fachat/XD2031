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

#include <stdint.h>
#include <string.h>

#include "ff.h"
#include "fat_provider.h"
#include "debug.h"
#include "config.h"
#include "device.h"

/**
 * cbm_compare_pattern compares the given name to the given pattern
 * and returns true if it matches.
 * Both names are null-terminated
 *
 * This implementation is roughly based on the Commodore semantics
 * of "*" and "?":
 * Commodore:
 * 	"*" - only works as the last pattern char and matches everything
 * 	      further chars in the pattern are ignored
 * 	"?" - single character is ignored
 * Adapted from pcserver/name.c.
 */
uint8_t compare_pattern(const char *name, const char *pattern) {

	uint8_t p = 0;		// current position

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

uint8_t is_path_separator(char c) {
	if(c == '/' || c =='\\') return 1;
	return 0;
}

/* splitpath 
 * returns the base filename
 * dir points to the directory path
 */
char *splitpath(char *path, char **dir) {
	char *p;

	p = path + strlen(path) - 1;

	// Remove trailing path separators
	while (p > path && is_path_separator(*p)) *p-- = 0;

	// Strip basename
	while (p >= path && !is_path_separator(*p)) p--;
	if(p > path) {
		*p = 0;
		*dir = path;
	} else {
		if(is_path_separator(*p)) *dir = "/";
		else *dir = ".";
	}
		
	return (p + 1);
}

/* concats path and filename
 * returns CBM_ERROR_FILE_NAME_TOO_LONG if the buffer cannot take the resulting path
 * otherwise returns CBM_ERROR_OK
 */
int8_t concat_path_filename(char *path, uint16_t pathmax, const char *dir, const char *name) {
	if((strlen(dir) + 1 + strlen(name)) > pathmax) return CBM_ERROR_FILE_NAME_TOO_LONG;
	strcpy(path, dir);
	strcat(path, "/");
	strcat(path, name);
	return CBM_ERROR_OK;
}

// just a dummy action for debug purposes
int8_t dummy_action(const char *path) {
	debug_printf("--> '%s'\n", path);
	return CBM_ERROR_OK;
}

int8_t traverse(
	char		*path,			// path string (may contain wildcards and path separators)
	uint16_t	max_matches,		// abort if this number of matches is reached
	uint16_t	*matches,		// count number of total matches
	uint8_t		required_flags,		// AM_DIR | AM_RDO | AM_HID | AM_SYS | AM_ARC
	uint8_t		forbidden_flags,	// AM_DIR | AM_RDO | AM_HID | AM_SYS | AM_ARC
	int8_t 	(*action)(const char *path)	// function called by each match
) {
	uint8_t res;
	char *b, *d;
	char *filename;
        char action_path[_MAX_LFN+1];
	DIR dir; 
	FILINFO Finfo;		// holds file information returned by f_readdir() / f_stat()
				// the long file name *lfname must be stored externally:
#	ifdef _USE_LFN
		char Lfname[_MAX_LFN+1];
		Finfo.lfname = Lfname;
		Finfo.lfsize = sizeof Lfname;
#	endif

	debug_printf("traverse called with '%s'\n", path);
	b = splitpath(path, &d);
	debug_printf("DIR: %s NAME: %s\n", d, b);

	res = f_opendir(&dir, d);
	if(res) {
		debug_printf("traverse f_opendir=%d", res); debug_putcrlf();
		return res;
	}

	for(;;)
	{
		res = f_readdir(&dir, &Finfo);
		if(res || !Finfo.fname[0]) break;

		filename = Finfo.fname;
#		ifdef _USE_LFN
			if(Lfname[0]) filename = Lfname;
#		endif
		debug_printf("candidate: '%s'\n", filename);

		if((Finfo.fattrib & required_flags) != required_flags) {
			debug_puts("required flags missing, ignored\n");
			continue;
		}

		if(Finfo.fattrib & forbidden_flags) {
			debug_puts("flagged with forbidden flags, ignored\n");
			continue;
		}

		if(compare_pattern(filename, b)) {
			res = concat_path_filename(action_path, sizeof(action_path), d, filename);
			if(res) return res;
			res = action(action_path);
			(*matches)++;
			if(res || (*matches == max_matches)) return res;
		}
	}
	return CBM_ERROR_OK;
}
