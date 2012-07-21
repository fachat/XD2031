/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2012 Andre Fachat

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

#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#include "dir.h"
#include "name.h"
#include "fscmd.h"
#include "wireformat.h"
#include "log.h"

#define min(a,b)        (((a)<(b))?(a):(b))

/**
 *  fopen the first matching directory entry, using the given
 *  options string
 */
FILE *open_first_match(const char *dir, const char *pattern, const char *options) {
	DIR *dp;
	FILE *fp;
	struct dirent *de;

	// shortcut - if we don't have wildcards, just open it
	if (index(pattern, '*') == NULL && index(pattern, '?') == NULL) {
		char *namebuf = malloc_path(dir, pattern);

		log_info("opening file with name %s\n",namebuf);

		fp = fopen(namebuf, options);

		if (fp == NULL) {
			log_errno("Error opening file with first match");
		}
		free(namebuf);
		return fp;
	}


	dp = opendir(dir);
	if (dp) {
		de = readdir(dp);

		while (de != NULL) {
			if (compare_pattern(de->d_name, pattern)) {
				// match

				char *namebuf = malloc_path(dir, de->d_name);

				log_info("opening file with name %s\n",namebuf);

				fp = fopen(namebuf, options);
				if (fp == NULL) {
					log_errno("Error opening file with match");
				}

				free(namebuf);
				closedir(dp);
				return fp;
			}
			de = readdir(dp);
		}
		
	}
	return NULL;
}

/**
 *  calls the callback on every matching file, returning the number of matches
 *  The callback gets the match count as first parameter (starting with one),
 *  and if it returns != 0 then the loop is exited.
 */
int dir_call_matches(const char *dir, const char *pattern, int (*callback)(const int num_of_match, const char *name)) {
	int matches = 0;
	DIR *dp;
	int rv;
	struct dirent *de;
	int onlyone = 0;

	// shortcut - if we don't have wildcards, just open it
	if (index(pattern, '*') == NULL && index(pattern, '?') == NULL) {
		onlyone = 1;
	}


	dp = opendir(dir);
	if (dp) {
		de = readdir(dp);

		while (de != NULL) {
			if (compare_pattern(de->d_name, pattern)) {
				// match
				matches ++;

				// get full path
				char *namebuf = malloc_path(dir, de->d_name);
				rv = callback(matches, namebuf);
				// free memory
				free(namebuf);
				if (rv || onlyone) {
					// either callback tells us to stop
					// or there are no wildcards, so this has to be it
					closedir(dp);
					// if rv < 0 then some kind of error happened, return it
					// instead of the number of matches
					return (rv < 0) ? rv : matches;
				}
			}
			de = readdir(dp);
		}
		
	}
	closedir(dp);
	return matches;
}



/**
 * fill the buffer with a header entry, using the driveno as line number
 * and dirpattern as file name
 *
 * returns the length of the buffer
 */
int dir_fill_header(char *dest, int driveno, char *dirpattern) {
        dest[FS_DIR_LEN+0] = driveno & 255;
        dest[FS_DIR_LEN+1] = 0;
        dest[FS_DIR_LEN+2] = 0;
        dest[FS_DIR_LEN+3] = 0;
        // don't set date for now
        dest[FS_DIR_MODE]  = FS_DIR_MOD_NAM;
	if (*dirpattern == 0) {
		// no pattern given
	        // simple default
	        strncpy(dest+FS_DIR_NAME, ".               ", 16);
	} else {
		strncpy(dest+FS_DIR_NAME, dirpattern, 16);
		int l = strlen(dest+FS_DIR_NAME);
		// fill up with spaces
		while (l < 16) (dest+FS_DIR_NAME)[l++] = ' ';
	}
        dest[FS_DIR_NAME + 16] = 0;
	return FS_DIR_NAME + 17;
}


/**
 * finds the next directory entry matching the given directory pattern
 */
struct dirent* dir_next(DIR *dp, char *dirpattern) {

	struct dirent *de = NULL;

	de = readdir(dp);

	if (dirpattern != NULL && *dirpattern != 0) {
		while (de != NULL) {
			if (compare_pattern(de->d_name, dirpattern)) {
				// match
				return de;
			}
			de = readdir(dp);
		}		
	}

	return de;
}


/**
 * fill in the buffer with a directory entry
 */
int dir_fill_entry(char *dest, struct dirent *de, int maxsize) {
	struct stat sbuf;
	struct tm *tp;

        /* TODO: check return value */
	stat(de->d_name, &sbuf);

        dest[FS_DIR_LEN] = sbuf.st_size & 255;
        dest[FS_DIR_LEN+1] = (sbuf.st_size >> 8) & 255;
        dest[FS_DIR_LEN+2] = (sbuf.st_size >> 16) & 255;
        dest[FS_DIR_LEN+3] = (sbuf.st_size >> 24) & 255;

        tp = localtime(&sbuf.st_mtime);
        dest[FS_DIR_YEAR]  = tp->tm_year;
        dest[FS_DIR_MONTH] = tp->tm_mon;
        dest[FS_DIR_DAY]   = tp->tm_mday;
        dest[FS_DIR_HOUR]  = tp->tm_hour;
        dest[FS_DIR_MIN]   = tp->tm_min;
        dest[FS_DIR_SEC]   = tp->tm_sec;

        dest[FS_DIR_MODE]  = S_ISDIR(sbuf.st_mode) ? FS_DIR_MOD_DIR : FS_DIR_MOD_FIL;
        // de->d_name is 0-terminated (see readdir man page)
        int l = strlen(de->d_name);
        strncpy(dest+FS_DIR_NAME, de->d_name,
                  min(l+1, maxsize-1-FS_DIR_NAME));
	// make sure we're still null-terminated
	dest[maxsize-1] = 0;

	return FS_DIR_NAME + strlen(dest+FS_DIR_NAME) + 1;
}


/**
 * fill in the buffer with the final disk info entry
 */
int dir_fill_disk(char *dest) {
        dest[FS_DIR_LEN] = 0;
        dest[FS_DIR_LEN+1] = 0;
        dest[FS_DIR_LEN+2] = 1;
        dest[FS_DIR_LEN+3] = 0;
        dest[FS_DIR_MODE]  = FS_DIR_MOD_FRE;
        dest[FS_DIR_NAME] = 0;
	return FS_DIR_NAME + 1;
}

/**
 * malloc a new path and copy the given base path and name, concatenating
 * them with the path separator
 */
char *malloc_path(const char *base, const char *name) {
        int l = (base == NULL) ? 0 : strlen(base);
        l += (name == NULL) ? 0 : strlen(name);
        l += 3; // dir separator, terminating zero, optional "."

        char *dirpath = malloc(l);
        dirpath[0] = 0;
        if (base != NULL) {
                strcat(dirpath, base);
                strcat(dirpath, "/");   // TODO dir separator char
        }
        if (name != NULL) {
                strcat(dirpath, name);
        }

        log_info("Calculate new dir path: %s\n", dirpath);

        return dirpath;
}

