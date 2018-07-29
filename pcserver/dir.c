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

#include "os.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>

#include "provider.h"
#include "dir.h"
#include "wildcard.h"
#include "wireformat.h"
#include "log.h"

#ifndef min
#define min(a,b)        (((a)<(b))?(a):(b))
#endif

int advanced_wildcards = false;


/**
 * traverse a directory and find the first match for the pattern,
 * using the Commodore file search pattern matching algorithm.
 * Returns a malloc'd pathname, which has to be freed
 */
char *find_first_match(const char *dir, const char *pattern, int (*check)(const char *name)) {
	DIR *dp;
	struct dirent *de;

	// shortcut - if we don't have wildcards, just open it
	if (strchr(pattern, '*') == NULL && strchr(pattern, '?') == NULL) {
		char *namebuf = malloc_path(dir, pattern);

		if (check(namebuf)) {
			return namebuf;
		}

		free(namebuf);
		return NULL;
	}


	dp = opendir(dir);
	if (dp) {
		de = readdir(dp);

		while (de != NULL) {
			if (compare_pattern(de->d_name, pattern, advanced_wildcards)) {
				// match

				char *namebuf = malloc_path(dir, de->d_name);

				if (check(namebuf)) {
					closedir(dp);
					return namebuf;
				}
			}
			de = readdir(dp);
		}

		closedir(dp);
	}
	return NULL;
}

/**
 *  fopen the first matching directory entry of type "file", using the given
 *  options string
 */
FILE *open_first_match(const char *dir, const char *pattern, const char *options) {
	FILE *fp = NULL;

	char *name = find_first_match(dir, pattern, os_path_is_file);
	if (name != NULL) {
		fp = fopen(name, options);
		if (fp == NULL) {
			log_errno("Error opening file with first match");
		}

		free(name);
		return fp;
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
	if (strchr(pattern, '*') == NULL && strchr(pattern, '?') == NULL) {
		onlyone = 1;
	}


	dp = opendir(dir);
	if (dp) {
		de = readdir(dp);

		while (de != NULL) {
			if (compare_pattern(de->d_name, pattern, advanced_wildcards)) {
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
int dir_fill_header(char *dest, int driveno, const char *dirpattern) {
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
struct dirent* dir_next(DIR *dp, const char *dirpattern) {

	struct dirent *de = NULL;

	log_debug("dir_next(dp=%p, pattern=%s\n", dp, dirpattern);

	de = readdir(dp);

	if (dirpattern != NULL && *dirpattern != 0) {
		while (de != NULL) {
			log_debug("dir_next:match(%s)\n", de->d_name);
			if (compare_pattern(de->d_name, dirpattern, advanced_wildcards)) {
				// match
				return de;
			}
			de = readdir(dp);
		}
	}

	return de;
}


/**
 * fill in the buffer with a directory entry from a direntry_t struct
 */
int dir_fill_entry_from_direntry(char *dest, direntry_t *de, int maxsize) {
	struct tm *tp;

	ssize_t size = de->size;
	// TODO: overflow check if ssize_t has more than 32 bits
        dest[FS_DIR_LEN] = size & 255;
        dest[FS_DIR_LEN+1] = (size >> 8) & 255;
        dest[FS_DIR_LEN+2] = (size >> 16) & 255;
        dest[FS_DIR_LEN+3] = (size >> 24) & 255;

        tp = localtime(&(de->moddate));
        dest[FS_DIR_YEAR]  = tp->tm_year;
        dest[FS_DIR_MONTH] = tp->tm_mon;
        dest[FS_DIR_DAY]   = tp->tm_mday;
        dest[FS_DIR_HOUR]  = tp->tm_hour;
        dest[FS_DIR_MIN]   = tp->tm_min;
        dest[FS_DIR_SEC]   = tp->tm_sec;

//	if (file->writable == 0) {
//		dest[FS_DIR_ATTR] |= FS_DIR_ATTR_LOCKED;
//	}
	// test
	//if (sbuf.st_size & 1) {
	//	dest[FS_DIR_ATTR] |= FS_DIR_ATTR_SPLAT;
	//}

	log_debug("dir_fill_entry: type=%02x, attr=%02x\n", de->type, de->attr);

        dest[FS_DIR_MODE] = de->mode;
        dest[FS_DIR_ATTR] = de->attr | de->type;

	// file name
       	int l = 0;
	if (de->name == NULL) {
		// blocks free
		dest[FS_DIR_NAME] = 0;
	} else {
        	l = strlen((const char*)de->name);
        	strncpy(dest+FS_DIR_NAME, (const char*)de->name,
                	  min(l+1, maxsize-1-FS_DIR_NAME));
		// make sure we're still null-terminated
		dest[maxsize-1] = 0;
	}
	// character set conversion
      	l = strlen(dest+FS_DIR_NAME);

	return FS_DIR_NAME + l + 1;
}

/**
 * fill in the buffer with a directory entry from a file_t struct
 */
int dir_fill_entry_from_file(char *dest, file_t *file, int maxsize) {
	struct tm *tp;

	ssize_t size = file->filesize;
	// TODO: overflow check if ssize_t has more than 32 bits
        dest[FS_DIR_LEN] = size & 255;
        dest[FS_DIR_LEN+1] = (size >> 8) & 255;
        dest[FS_DIR_LEN+2] = (size >> 16) & 255;
        dest[FS_DIR_LEN+3] = (size >> 24) & 255;

        tp = localtime(&(file->lastmod));
        dest[FS_DIR_YEAR]  = tp->tm_year;
        dest[FS_DIR_MONTH] = tp->tm_mon;
        dest[FS_DIR_DAY]   = tp->tm_mday;
        dest[FS_DIR_HOUR]  = tp->tm_hour;
        dest[FS_DIR_MIN]   = tp->tm_min;
        dest[FS_DIR_SEC]   = tp->tm_sec;

	dest[FS_DIR_ATTR]  = file->type;
	if (file->writable == 0) {
		dest[FS_DIR_ATTR] |= FS_DIR_ATTR_LOCKED;
	}
	// test
	//if (sbuf.st_size & 1) {
	//	dest[FS_DIR_ATTR] |= FS_DIR_ATTR_SPLAT;
	//}

	log_debug("dir_fill_entry: type=%02x, attr=%02x\n", file->type, file->attr);

        dest[FS_DIR_MODE] = file->mode;
        dest[FS_DIR_ATTR] = file->attr | file->type;

	// file name
       	int l = 0;
	if (file->filename == NULL) {
		// blocks free
		dest[FS_DIR_NAME] = 0;
	} else {
        	l = strlen(file->filename);
        	strncpy(dest+FS_DIR_NAME, file->filename,
                	  min(l+1, maxsize-1-FS_DIR_NAME));
		// make sure we're still null-terminated
		dest[maxsize-1] = 0;
	}
	// character set conversion
      	l = strlen(dest+FS_DIR_NAME);

	return FS_DIR_NAME + l + 1;
}



