/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2013 Andre Fachat
    Copyright (C) 2013 Nils Eilers

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
#include "log.h"

#include <unistd.h>
#include <errno.h>
#include <sys/time.h>		// select()

/* patch dir separator characters to '/'
 * fs_provider (Linux / OS X), http and ftp require the slash.
 * Windows user would prefer the backslash, but Windows can cope with 
 * the forward slash too, the same is true for FatFs.
 * Notice that WIN32 realpath changes slashes to '\'!
 */

char *os_patch_dir_separator(char *path) {
	char *newpath = path;

	if(!path) {
		log_debug("os_patch_dir_separator(NULL)\n");
		return NULL;
	}

	while(*path) {
		if(*path == '\\') *path = dir_separator_char();
		path++;
	}

	return newpath;
}

// Remove trailing line end characters
char *drop_crlf(char *s) {
	char *p = s + strlen(s) - 1;
	while(p > s) {
		if((*p == 10) || (*p == 13)) *p--=0; 	// remove CR or LF
		else break;
	}
	return s;
}

// ========================================================================
//	POSIX
// ========================================================================

#ifndef _WIN32

#include <sys/stat.h>
#include <sys/statvfs.h>
#include <pwd.h>
#include <dirent.h>

const char* os_get_home_dir (void) {
        char* dir = getenv("HOME");
	log_info("Home path (env): %s\n", dir);
	// If for some reason the environment variable should be undefined:
        if(!dir) {
                struct passwd* pwd = getpwuid(getuid());
                dir = pwd->pw_dir;
		log_info("Home path (getpwuid): %s\n", dir);
	}
	if (!dir) {
		fprintf(stderr, "Unable to determine home directory.\n");
                exit(EXIT_RESPAWN_NEVER);
        }
        return dir;
}

/**
 * check a path, making sure it's something readable, not a directory
 */
int os_path_is_file(const char *name) {
	struct stat sbuf;
	int isfile = 1;

	log_debug("checking file with name %s\n",name);

	if (lstat(name, &sbuf) < 0) {
		log_errno("Error stat'ing file");
		// note we still return 1, as open may succeed - e.g. for 
		// save where the file does not exist in the first place
	} else {
		if (S_ISDIR(sbuf.st_mode)) {
			isfile = 0;
			log_error("Error trying to open a directory as file\n");
		}
	}
	return isfile;
}

/**
 * check a path, making sure it's a directory
 */
int os_path_is_dir(const char *name) {
	struct stat sbuf;
	int isdir = 1;

	log_info("checking dir with name %s\n",name);

	if (lstat(name, &sbuf) < 0) {
		log_errno("Error stat'ing file\n");
		isdir = 0;
	} else {
		if (!S_ISDIR(sbuf.st_mode)) {
			isdir = 0;
			log_error("Error trying to open a directory as file\n");
		}
	}
	return isdir;
}

// free disk space in bytes, < 0 on errors
signed long long os_free_disk_space (char *path) {
	struct statvfs buf;
	int er;
	unsigned long long total;

	er = statvfs(path, &buf);
	if (er == 0) {
		unsigned long blksize = buf.f_frsize;
		fsblkcnt_t free_blocks = buf.f_bavail; // unprivileged users
		total = (unsigned long long) blksize * (unsigned long long) free_blocks;
	} else {
		total = -errno;
	}
	return total;
}


int os_stdin_has_data(void) {
	fd_set rfds;
	struct timeval tv;
	int res;

	// Watch stdin (fd 0) to see when it has input
	FD_ZERO(&rfds);
	FD_SET(0, &rfds);

	// Don't block
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	res = select(1, &rfds, NULL, NULL, &tv);
	// Don't rely on the value of tv now!

	if (res == -1) {
		log_error("select(): (%d) %s\n", os_errno(), os_strerror(os_errno()));
		return 0;
	} else if(res) {
		// data available in stdin
		return 1;
	}

	// no data available
	return 0;
}

#else


// ========================================================================
//	WIN32
// ========================================================================

#include <windows.h>
#include <stdio.h>
#include <limits.h>
#include <sys/stat.h>
#include <ctype.h>

#include "mem.h"

const char* os_get_home_dir (void) {
        char* dir = getenv("HOME");
	log_info("Home path (env): %s\n", dir);
	if (!dir) {
		fprintf(stderr, "Unable to determine home directory.\n");
                exit(EXIT_RESPAWN_NEVER);
        }
        return dir;
}


int os_path_is_file (const char *name) {
	int isfile = 1;

	log_info("checking file with name %s (path_is_file)\n",name);
	uint32_t file_type = GetFileAttributes(name);
	if (file_type == INVALID_FILE_ATTRIBUTES) {
		// note we still return 1, as open may succeed - e.g. for 
		// save where the file does not exist in the first place
		isfile = 1;
		log_error("Unable to GetFileAttributes for %s\n", name);
	} else if ((file_type & FILE_ATTRIBUTE_DEVICE) || (file_type & FILE_ATTRIBUTE_DIRECTORY)) {
		log_debug("FILE_ATTRIBUTE || FILE_ATTRIBUTE_DIRECTORY\n");
		isfile = 0;
	}
	if (!isfile) {
		log_error("Error trying to open a directory as file\n");
	}
	return isfile;
}

int os_path_is_dir (const char *name) {
	int isdir;

	log_info("checking dir with name %s (path_is_dir)\n", name);
	uint32_t file_type = GetFileAttributes(name);
	if (file_type == INVALID_FILE_ATTRIBUTES) {
		isdir = 0;
		log_error("Unable to GetFileAttributes for %s\n", name);
	} else {
		isdir = file_type & FILE_ATTRIBUTE_DIRECTORY;
	}
	if (!isdir) {
		log_error("Error trying to open a directory as file\n");
	}
	return isdir;
}

// free disk space in bytes, < 0 on errors
// If per-user quotas are being used, the reported value may be less than 
// the total number of free bytes on a disk.
signed long long os_free_disk_space (char *path) {
	BOOL res;
	signed long long total, free_bytes_to_caller;

	res = GetDiskFreeSpaceEx(path, (PULARGE_INTEGER)&free_bytes_to_caller, NULL, NULL);
	if (res) {
		total = free_bytes_to_caller;
	} else {
		log_errno("Unable to get free disk space for '%s'", path);
		total = -GetLastError();
	}
	return total;
}

/* 

realpath() Win32 implementation, supports non standard glibc extension
This file has no copyright assigned and is placed in the Public Domain.
Written by Nach M. S. September 8, 2005

Taken from http://sourceforge.net/p/mingw/patches/256/

*/

char *os_realpath(const char *path)
{
  char *return_path = 0;
  char *resolved_path = NULL;
  char *in_path;

  if (path) //Else EINVAL
  {
    if (resolved_path)
    {
      return_path = resolved_path;
    }
    else
    {
      //Non standard extension that glibc uses
      return_path = malloc(PATH_MAX);
    }

    // Special handling for root directory with drive
    if (strlen(path) == 3) {
      if((isalpha(path[0])) && (path[1] == ':') &&
         ((path[2] == '/') || (path[2] == '\\'))) {
        log_debug("os_realpath: root directory with drive: %s\n", path);
        char *p = mem_alloc_str(path);
        p[0] = toupper(p[0]);
        return p;
      }
    }

    // Special handling for root directory without drive
    if ((strlen(path) == 1) && ((path[0] == '/') || (path[0] == '\\'))) {
      char *current_directory = malloc(PATH_MAX);
      if(!current_directory) {
        log_error("malloc failed: %s, line %u\n", __FILE__, __LINE__);
        return NULL;
      }
      DWORD res = GetCurrentDirectory(PATH_MAX, current_directory);
      if(!res) {
        log_error("GetCurrentDirectory failed (%d)\n", GetLastError());
        return NULL;
      }
      if(res > PATH_MAX) {
        free(current_directory);
        current_directory = malloc(res);
        if(!current_directory) {
          log_error("malloc failed: %s, line %u\n", __FILE__, __LINE__);
          return NULL;
        }
	DWORD res2 = GetCurrentDirectory(res, current_directory);
        if(!res2) {
          log_error("GetCurrentDirectory failed (%d)\n", GetLastError());
          return NULL;
        }
      }
      current_directory[2] = '/'; 
      current_directory[3] = 0; 
      log_debug("os_realpath: root directory without drive: %s\n", current_directory);
      return current_directory;
    }

    // Drop trailing slashes
    in_path = mem_alloc_str_(path, __FILE__, __LINE__);
    char *p = in_path + strlen(in_path) -1;
    while((*p == '/') || (*p == '\\')) *p-- = 0;

    if (return_path) //Else EINVAL
    {
      //This is a Win32 API function similar to what realpath() is supposed to do
      size_t size = GetFullPathNameA(in_path, PATH_MAX, return_path, 0);

      //GetFullPathNameA() returns a size larger than buffer if buffer is too small
      if (size > PATH_MAX)
      {
        if (return_path != resolved_path) //Malloc'd buffer - Unstandard extension retry
        {
          size_t new_size;

          free(return_path);
          return_path = malloc(size);

          if (return_path)
          {
            new_size = GetFullPathNameA(in_path, size, return_path, 0); //Try again

            if (new_size > size) //If it's still too large, we have a problem, don't try again
            {
              free(return_path);
              return_path = 0;
              errno = ENAMETOOLONG;
            }
            else
            {
              size = new_size;
            }
          }
          else
          {
            //I wasn't sure what to return here, but the standard does say to return EINVAL
            //if resolved_path is null, and in this case we couldn't malloc large enough buffer
            errno = EINVAL;
          }  
        }
        else //resolved_path buffer isn't big enough
        {
          return_path = 0;
          errno = ENAMETOOLONG;
        }
      }

      //GetFullPathNameA() returns 0 if some path resolve problem occured
      if (!size) 
      {
        if (return_path != resolved_path) //Malloc'd buffer
        {
          free(return_path);
        }

        return_path = 0;

        //Convert MS errors into standard errors
        switch (GetLastError())
        {
          case ERROR_FILE_NOT_FOUND:
            errno = ENOENT;
            break;

          case ERROR_PATH_NOT_FOUND: case ERROR_INVALID_DRIVE:
            errno = ENOTDIR;
            break;

          case ERROR_ACCESS_DENIED:
            errno = EACCES;
            break;

          default: //Unknown Error
            errno = EIO;
            break;
        }
      }

      //If we get to here with a valid return_path, we're still doing good
      if (return_path)
      {
        struct stat stat_buffer;

        //Make sure path exists, stat() returns 0 on success
        if (stat(return_path, &stat_buffer)) 
        {
          if (return_path != resolved_path)
          {
            free(return_path);
          }

          return_path = 0;
          //stat() will set the correct errno for us
        }
        //else we succeeded!
      }
    }
    else
    {
      errno = EINVAL;
    }
  }
  else
  {
    errno = EINVAL;
  }
  os_patch_dir_separator(return_path);
  mem_free(in_path);
  return return_path;
}


/*

Implementation of POSIX directory browsing functions and types for Win32.

Author:  Kevlin Henney (kevlin@acm.org, kevlin@curbralan.com)
History: Created March 1997. Updated June 2003 and July 2012.
Rights:  See end of file.

Taken from http://www.two-sdg.demon.co.uk/curbralan/code/dirent/dirent.html

*/

#ifdef __cplusplus
extern "C"
{
#endif

typedef ptrdiff_t handle_type; /* C99's intptr_t not sufficiently portable */

struct DIR
{
    handle_type         handle; /* -1 for failed rewind */
    struct _finddata_t  info;
    struct dirent       result; /* d_name null iff first time */
    char                *name;  /* null-terminated char string */
};

DIR *opendir(const char *name)
{
    DIR *dir = 0;

    if(name && name[0])
    {
        size_t base_length = strlen(name);
        const char *all = /* search pattern must end with suitable wildcard */
            strchr("/\\", name[base_length - 1]) ? "*" : "/*";

        if((dir = (DIR *) malloc(sizeof *dir)) != 0 &&
           (dir->name = (char *) malloc(base_length + strlen(all) + 1)) != 0)
        {
            strcat(strcpy(dir->name, name), all);

            if((dir->handle =
                (handle_type) _findfirst(dir->name, &dir->info)) != -1)
            {
                dir->result.d_name = 0;
            }
            else /* rollback */
            {
                free(dir->name);
                free(dir);
                dir = 0;
            }
        }
        else /* rollback */
        {
            free(dir);
            dir   = 0;
            errno = ENOMEM;
        }
    }
    else
    {
        errno = EINVAL;
    }

    return dir;
}

int closedir(DIR *dir)
{
    int result = -1;

    if(dir)
    {
        if(dir->handle != -1)
        {
            result = _findclose(dir->handle);
        }

        free(dir->name);
        free(dir);
    }

    if(result == -1) /* map all errors to EBADF */
    {
        errno = EBADF;
    }

    return result;
}

struct dirent *readdir(DIR *dir)
{
    struct dirent *result = 0;

    if(dir && dir->handle != -1)
    {
        if(!dir->result.d_name || _findnext(dir->handle, &dir->info) != -1)
        {
            result         = &dir->result;
            result->d_name = dir->info.name;
        }
    }
    else
    {
        errno = EBADF;
    }

    return result;
}

void rewinddir(DIR *dir)
{
    if(dir && dir->handle != -1)
    {
        _findclose(dir->handle);
        dir->handle = (handle_type) _findfirst(dir->name, &dir->info);
        dir->result.d_name = 0;
    }
    else
    {
        errno = EBADF;
    }
}

#ifdef __cplusplus
}
#endif

/*

    Copyright Kevlin Henney, 1997, 2003, 2012. All rights reserved.

    Permission to use, copy, modify, and distribute this software and its
    documentation for any purpose is hereby granted without fee, provided
    that this copyright and permissions notice appear in all copies and
    derivatives.
    
    This software is supplied "as is" without express or implied warranty.

    But that said, if there are any problems please get in touch.

*/

// convert errnum into a string error message
char *os_strerror(int errnum) {
	static char msg[80];

	DWORD len = FormatMessage(
		//FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		errnum,
		MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
		//MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),     // Unicode? ANSI?
		(LPTSTR) &msg,
		sizeof(msg), // minimum number of TCHARs to allocate when FORMAT_MESSAGE_ALLOCATE_BUFFER
		NULL);
	if(len) {
		drop_crlf(msg);
		return (char *) msg;
	}
	return "Unknown error";
}

// Check if we have a valid file descriptor/handle
int os_open_failed(serial_port_t device) {
	return (device == INVALID_HANDLE_VALUE);
}

// Read data from the specified file or input/output device
ssize_t os_read(serial_port_t fd, void *buf, size_t count) {
	DWORD read_bytes = 0;
	int success;

	success = ReadFile(fd, buf, count, &read_bytes, NULL);

	if(!success) return -1;

#ifdef DEBUG_OS_READ
	if(success && (read_bytes >= 1)) {
		log_debug("os_read bytes read: %u\n", read_bytes);
	}
#endif

	return read_bytes;
}

// Write data to the specified file or input/output device
ssize_t os_write(serial_port_t fd, const void *buf, size_t count) {
	DWORD written_bytes = 0;
	int success;

	success = WriteFile(fd, buf, count, &written_bytes, NULL);

	if(success) return written_bytes;

	return -1;
}

#endif
