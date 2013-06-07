/**************************************************************************

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

**************************************************************************/

#ifndef OS_H
#define OS_H

// =======================================================================
//	LINUX and possibly other POSIX compatible systems
// =======================================================================

#if !defined(_WIN32) && !defined(__APPLE__)

#define	__USE_POSIX
#define _XOPEN_SOURCE 
#define _XOPEN_SOURCE_EXTENDED
#define __USE_XOPEN_EXTENDED
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>

#include "mem.h"

enum boolean { FALSE, TRUE };


static inline int os_mkdir(const char *pathname, mode_t mode) {
	return mkdir(pathname, mode);
}


static inline char *os_realpath (const char *path) {
	return realpath(path, NULL);
}


static inline void os_sync(void) {
	sync();
}

#endif


// =======================================================================
// 	APPLE OS X
// =======================================================================

#ifdef __APPLE__ // OS X 10.6.8, Darwin Kernel Version 10.8.0

#define	__USE_POSIX
#define _XOPEN_SOURCE 
#define _XOPEN_SOURCE_EXTENDED
#define __USE_XOPEN_EXTENDED
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syslimits.h>

#include "mem.h"

enum boolean { FALSE, TRUE };

static inline int os_mkdir(const char *pathname, mode_t mode) {
	return mkdir(pathname, mode);
}


static inline char *os_realpath (const char *path) {
	return (realpath(path, mem_alloc_c(PATH_MAX, "realpath")));
}


static inline void os_sync(void) {
	sync();
}

#endif

// =======================================================================
//	WIN32
// ======================================================================= 

#ifdef _WIN32

#include <stdlib.h>
#include <windows.h>
#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

// realpath contained in os.c
char *os_realpath(const char *path); 

// Windows mkdir has only 1 parameter, no mode
static inline int os_mkdir(const char *pathname, int mode) 
{
	(void)(mode); // silence unused parameter warning
	return mkdir(pathname);
}

/* Windows doesn't know sync. A cheat to sync all open files
requires SYSTEM rights. Don't.  If possible, sync() should be
replaced by syncfs(Linux) / FlushFileBuffers(Win) for a single file */
static inline void os_sync (void) {}


/* dirent.h */

/*

    Declaration of POSIX directory browsing functions and types for Win32.

    Author:  Kevlin Henney (kevlin@acm.org, kevlin@curbralan.com)
    History: Created March 1997. Updated June 2003.
    Rights:  See end of file.
    
*/

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct DIR DIR;

struct dirent
{
    char *d_name;
};

DIR           *opendir(const char *);
int           closedir(DIR *);
struct dirent *readdir(DIR *);
void          rewinddir(DIR *);

/*

    Copyright Kevlin Henney, 1997, 2003. All rights reserved.

    Permission to use, copy, modify, and distribute this software and its
    documentation for any purpose is hereby granted without fee, provided
    that this copyright and permissions notice appear in all copies and
    derivatives.
    
    This software is supplied "as is" without express or implied warranty.

    But that said, if there are any problems please get in touch.

*/

#ifdef __cplusplus
}
#endif

#endif // WIN32


// =======================================================================
//	COMMON
// =======================================================================

// return a const char pointer to the home directory of the user 
const char *os_get_home_dir(void);

// check a path, making sure it's something readable, not a directory
int os_path_is_file(const char *name);

// check a path, making sure it's a directory
int os_path_is_dir(const char *name);

// free disk space in bytes
signed long long os_free_disk_space (char *path);

// patch dir separator sign to dir_separator_char
char *os_patch_dir_separator (char *path);

static inline char dir_separator_char(void) { return '/'; }
static inline char* dir_separator_string(void) { return "/"; }


#endif // OS_H
