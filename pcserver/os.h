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

#include "log.h"
#include "mem.h"

/* 

  This file contains

  - instructions to include headers for the specific operating system
  - shared code for POSIX systems (Linux, OS X)
  - specifics for Linux, Apple OX X and Windows
  - meta functions/prototypes for all operating systems

*/

// =======================================================================
//	INCLUDES FOR LINUX and possibly other POSIX compatible systems
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
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

// =======================================================================
// 	INCLUDES FOR APPLE OS X
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
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

// =======================================================================
//	INCLUDES FOR WIN32
// ======================================================================= 

#ifdef _WIN32
// Enable fileno()
#ifdef __STRICT_ANSI__
#undef __STRICT_ANSI__
#endif
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <winsock2.h>
#include <conio.h>	// _kbhit()
#endif



// =======================================================================
//	COMMON POSIX
// =======================================================================

#ifndef _WIN32

enum boolean { FALSE, TRUE };

typedef int serial_port_t;
#define OS_OPEN_FAILED -1

static inline ssize_t os_read(serial_port_t fd, void *buf, size_t count) {
	return read(fd, buf, count);
}

static inline ssize_t os_write(serial_port_t fd, const void *buf, size_t count) {
	return write(fd, buf, count);
}

static inline int os_mkdir(const char *pathname, mode_t mode) {
	return mkdir(pathname, mode);
}

static inline int os_open_failed(serial_port_t device) {
	return (device < 0);
}

static inline int device_close(int fildes) {
	return close(fildes);
}

static inline int socket_close(int descriptor) {
	return close(descriptor);
}

static inline int os_errno(void) {
	return errno;
}

static inline char *os_strerror(int errnum) {
	return strerror(errnum);
}

static inline int os_fsync(FILE *f) {
	int res;

	res = fflush(f);
	if(res) log_error("fflush failed: (%d) %s\n", os_errno(), os_strerror(os_errno()));
	res = fsync(fileno(f));
	if(res) log_error("fsync failed: (%d) %s\n", os_errno(), os_strerror(os_errno()));
	log_debug("os_fsync completed\n");
	return res;
}


// -----------------------------------------------------------------------
//	LINUX
// -----------------------------------------------------------------------

#ifndef __APPLE__
static inline char *os_realpath (const char *path) {
	return realpath(path, NULL);
}
#endif // LINUX

// -----------------------------------------------------------------------
//	APPLE OS X
// -----------------------------------------------------------------------

#ifdef __APPLE__
static inline char *os_realpath (const char *path) {
	return (realpath(path, mem_alloc_c(PATH_MAX, "realpath")));
}
#endif // APPLE OS X

#endif // POSIX

// =======================================================================
//	WIN32
// ======================================================================= 

#ifdef _WIN32

typedef HANDLE serial_port_t;
#define OS_OPEN_FAILED INVALID_HANDLE
static inline int device_close(serial_port_t device) {
	return CloseHandle(device);
}

// realpath contained in os.c
char *os_realpath(const char *path); 

// Windows mkdir has only 1 parameter, no mode
static inline int os_mkdir(const char *pathname, int mode) 
{
	(void)(mode); // silence unused parameter warning
	return mkdir(pathname);
}

// Get last system error
static inline int os_errno(void) {
	return GetLastError();
}

/* Windows doesn't know sync. A cheat to sync all open files
requires SYSTEM rights. Don't.  If possible, sync() should be
replaced by syncfs(Linux) / FlushFileBuffers(Win) for a single file */
static inline int os_fsync (FILE *f) {
	log_debug("os_fsync\n");
	fflush(f);
	return ((FlushFileBuffers ((HANDLE) _get_osfhandle(_fileno(f)))) ? 0 : -1);
}


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

static inline int socket_close(int descriptor) {
	int res;

	res = closesocket (descriptor);
	WSACleanup();

	return res;
}

static inline int os_stdin_has_data(void) {
	return _kbhit();
}

#endif // WIN32


// =======================================================================
//	COMMON FOR ALL OPERATING SYSTEMS
// =======================================================================

// EXIT_SUCCESS is always defined as 0. The daemon should never respawn.
// EXIT_FAILURE is defined as 1 on Linux. If the daemon should respawn,
// is user defined
#define EXIT_RESPAWN		1 // user defined
#define EXIT_RESPAWN_ALWAYS	2
#define EXIT_RESPAWN_NEVER	3

// return a const char pointer to the home directory of the user 
const char *os_get_home_dir(void);

// check a path, making sure it's something readable, not a directory
int os_path_is_file(const char *name);

// check a file name, making sure it's something creatable (i.e. no wildcards, illegal chars etc)
int os_filename_is_legal(const char *name);

// check a path, making sure it's a directory
int os_path_is_dir(const char *name);

// free disk space in bytes
signed long long os_free_disk_space (const char *path);

// patch dir separator sign to dir_separator_char
char *os_patch_dir_separator (char *path);

static inline char dir_separator_char(void) { return '/'; }
static inline char* dir_separator_string(void) { return "/"; }

// convert errnum into a string error message
char *os_strerror(int errnum);

// get last system error
int os_errno(void);

// check if we have a valid file descriptor/handle
int os_open_failed(serial_port_t device);

ssize_t os_read(serial_port_t fd, void *buf, size_t count);

ssize_t os_write(serial_port_t fd, const void *buf, size_t count);

int os_stdin_has_data(void);

char *drop_crlf(char *s);

#endif // OS_H
