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

#include "charconvert.h"
#include "provider.h"
#include "dir.h"
#include "wildcard.h"
#include "fscmd.h"
#include "wireformat.h"
#include "log.h"

#ifndef min
#define min(a,b)        (((a)<(b))?(a):(b))
#endif


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
			if (compare_pattern(de->d_name, pattern)) {
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
struct dirent* dir_next(DIR *dp, const char *dirpattern) {

	struct dirent *de = NULL;

	log_debug("dir_next(dp=%p, pattern=%s\n", dp, dirpattern);

	de = readdir(dp);

	if (dirpattern != NULL && *dirpattern != 0) {
		while (de != NULL) {
			log_debug("dir_next:match(%s)\n", de->d_name);

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
int dir_fill_entry(char *dest, char *curpath, struct dirent *de, int maxsize) {
	struct stat sbuf;
	struct tm *tp;

	char *realname = malloc_path(curpath, de->d_name);

        /* TODO: check return value */
	int writecheck = -EACCES;
	int rv = stat(realname, &sbuf);
	if (rv < 0) {
		log_error("Failed stat'ing entry %s\n", de->d_name);
		log_errno("Problem stat'ing dir entry");
	} else {
		writecheck = access(realname, W_OK);
		if ((writecheck < 0) && (errno != EACCES)) {
			writecheck = -errno;
			log_error("Could not get write access to %s\n", de->d_name);
			log_errno("Reason");
		}
	}
	free(realname);

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

	dest[FS_DIR_ATTR]  = FS_DIR_TYPE_PRG;
	if (writecheck < 0) {
		dest[FS_DIR_ATTR] |= FS_DIR_ATTR_LOCKED;
	}
	// test
	//if (sbuf.st_size & 1) {
	//	dest[FS_DIR_ATTR] |= FS_DIR_ATTR_SPLAT;
	//}

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
 * fill in the buffer with a directory entry from a file_t struct
 */
int dir_fill_entry_from_file(char *dest, file_t *file, int maxsize, charconv_t converter) {
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

        dest[FS_DIR_MODE] = file->mode;

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
	converter(dest+FS_DIR_NAME, l, dest+FS_DIR_NAME, l);

	return FS_DIR_NAME + l + 1;
}


/**
 * fill in the buffer with the final disk info entry
 */
int dir_fill_disk(char *dest, char *curpath) {
	signed long long total;

	total = os_free_disk_space(curpath);
	if (total > 0) {
		if (total > 0xffffffff) {
			// max in FS_DIR stuff
			total = 0xffffffff;
		}
	        dest[FS_DIR_LEN] = total & 255;
	        dest[FS_DIR_LEN+1] = (total >> 8) & 255;
	        dest[FS_DIR_LEN+2] = (total >> 16) & 255;
	        dest[FS_DIR_LEN+3] = (total >> 24) & 255;
	} else {
		log_errno("Could not get free disk space for '%s'\n", curpath);
	        dest[FS_DIR_LEN] = 1;
	        dest[FS_DIR_LEN+1] = 0;
	        dest[FS_DIR_LEN+2] = 0;
	        dest[FS_DIR_LEN+3] = 0;
	}
       	dest[FS_DIR_MODE]  = FS_DIR_MOD_FRE;
       	dest[FS_DIR_NAME] = 0;
	return FS_DIR_NAME + 1;
}

/**
 * malloc a new path and copy the given base path and name, concatenating
 * them with the path separator. Ignore base for absolute paths in name.
 */
char *malloc_path(const char *base, const char *name) {

	log_debug("malloc_path: base=%s, name=%s\n", base, name);

	if(name[0] == '/' || name[0]=='\\') base=NULL;	// omit base for absolute paths
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

        log_debug("Calculated new dir path: %s\n", dirpath);

        return dirpath;
}

