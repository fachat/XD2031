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

#ifndef DIR_H
#define	DIR_H

/**
 * advanced wildcards
 * If set to true, anything following a '*' must also match
 */
extern bool advanced_wildcards;

/**
 * traverse a directory and find the first match for the pattern,
 * using the Commodore file search pattern matching algorithm.
 * Returns a malloc'd pathname, which has to be freed
 */
char *find_first_match(const char *dir, const char *pattern, int (*check)(const char *name));

/**
 * fopen the first matching directory entry, using the given
 * options string
 */
FILE *open_first_match(const char *dir, const char *pattern, const char *options);

/**
 * calls the callback on every matching file, returning the number of matches
 * The callback gets the match count as first parameter (starting with one),
 * and if it returns != 0 then the loop is exited.
 */
int dir_call_matches(const char *dir, const char *pattern, int (*callback)(const int num_of_match, const char *name));

/**
 * fill the buffer with a header entry, using the driveno as line number
 * and dirpattern as file name
 *
 * returns the length of the written buffer
 */
int dir_fill_header(char *dest, int driveno, const char *dirpattern);

/**
 * finds the next directory entry matching the given directory pattern
 */
struct dirent* dir_next(DIR *dp, const char *dirpattern);

/**
 * fill in the buffer with a directory entry
 *
 * returns the length of the written buffer
 */
int dir_fill_entry_from_file(char *dest, file_t *file, int maxsize);


#endif

