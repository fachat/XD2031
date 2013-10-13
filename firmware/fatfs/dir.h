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

#ifndef DIR_H
#define DIR_H

uint8_t compare_pattern(const char *name, const char *pattern);

/* splitpath
 * returns the base filename
 * dir points to the directory path
 */
char *splitpath(char *path, char **dir);

/* concats path and filename
 * returns ERROR_FILE_NAME_TOO_LONG if the buffer cannot take the resulting path
 * otherwise returns ERROR_OK
 */
errno_t concat_path_filename(char *path, uint16_t pathmax, const char *dir, const char *name);


errno_t dummy_action(const char *dir, const char *name); // just a dummy action for debug purposes
errno_t traverse(
        char 		*path,                  // path string (may contain wildcards and path separators)
        uint16_t        max_matches,            // abort if this number of matches is reached
        uint16_t        *matches,               // count number of total matches
        uint8_t         required_flags,         // AM_DIR | AM_RDO | AM_HID | AM_SYS | AM_ARC
        uint8_t         forbidden_flags,        // AM_DIR | AM_RDO | AM_HID | AM_SYS | AM_ARC
        errno_t  (*action)(const char *path)	// function called by each match
);
  

#endif /* DIR_H */
