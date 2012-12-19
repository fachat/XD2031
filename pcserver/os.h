/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2012 Andre Fachat
    Copyright (C) 2012 Nils Eilers

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

#ifndef OS_H
#define OS_H

char *patch_dir_separator (char *path);

static inline char dir_separator_char(void) { return '/'; }
static inline char* dir_separator_string(void) { return "/"; }

#include "mem.h"

// Linux (3.4.11)
#define	__USE_POSIX
#define _XOPEN_SOURCE
#define __USE_XOPEN_EXTENDED
#include <stdlib.h>

#ifdef __APPLE__
#include <sys/syslimits.h>

static inline char *os_realpath (const char *path) 
{
	// OS X 10.6.8, Darwin Kernel Version 10.8.0
	return (realpath(path, mem_alloc_c(PATH_MAX, "realpath")));
}
#else
static inline char *os_realpath (const char *path) 
{
	return realpath(path, NULL);
}
#endif

/*
 * return a const char pointer to the home directory of the user 
 */
const char *get_home_dir(void);

#endif
