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

/*
 * This file is a filesystem provider implementation, to be
 * used with the FSTCP program on an OS/A65 computer. 
 *
 * In this file the actual command work is done for the 
 * local filesystem.
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <strings.h>

#include "wireformat.h"
#include "dir.h"
#include "fscmd.h"
#include "provider.h"

#include "log.h"

#undef DEBUG_READ

#define	MAX_BUFFER_SIZE	64

//#define	min(a,b)	(((a)<(b))?(a):(b))

typedef struct {
	int		state;		/* note: currently not really used */
	FILE		*fp;
	DIR 		*dp;
	char		dirpattern[MAX_BUFFER_SIZE];
	struct dirent	*de;
	unsigned int	is_first :1;	// is first directory entry?
} File;

File files[MAXFILES];


void fsp_init() {
	int i;
        for(i=0;i<MAXFILES;i++) {
          files[i].state = F_FREE;
        }
}

// ----------------------------------------------------------------------------------
// commands as sent from the device

// close a file descriptor
static void close_fds(int tfd) {
	int er = 0;
	if (files[tfd].fp != NULL) {
		er = fclose(files[tfd].fp);
		if (er < 0) {
			log_errno("Error closing fd");
		}
		files[tfd].fp = NULL;
	}
	if (files[tfd].dp != NULL) {
		er = closedir(files[tfd].dp);
		if (er < 0) {
			log_errno("Error closing dp");
		}
		files[tfd].dp = NULL;
	}
}

// open a file for reading, writing, or appending
static int open_file(int tfd, const char *buf, const char *mode) {
		/* no directory separators - security rules! */
		char *nm = (char*)buf;
		if(*nm=='/') nm++;
		if(strchr(nm, '/')) {
			// should give a security error
			// TODO: replace with correct error number
			return -1;
		}

		// close the currently open files
		// so we don't loose references to open files
		close_fds(tfd);

		FILE *fp = open_first_match(nm, mode);
printf("OPEN_RD/AP/WR(%s: %s)=%p\n",mode, buf,(void*)fp);
		if(fp) {
		  files[tfd].fp = fp;
		  files[tfd].dp = NULL;
		  return 0;
		}
		// TODO: open error (maybe depending on errno?)
		return -1;
}

// open a directory read
static int open_dr(int tfd, const char *buf) {
		// close the currently open files
		// so we don't loose references to open files
		close_fds(tfd);

		// save pattern for later comparisons
		strcpy(files[tfd].dirpattern, buf);
		DIR *dp = opendir("." /*buf+FSP_DATA*/);
printf("OPEN_DR(%s)=%p\n",buf,(void*)dp);
		if(dp) {
		  files[tfd].fp = NULL;
		  files[tfd].dp = dp;
		  files[tfd].is_first = 1;
		  return 0;
		}
		// TODO: open error (maybe depending on errno?)
		return -1;
}

// read directory
static int read_dir(int tfd, char *retbuf, int *eof) {
		int rv = 0;
		  if (files[tfd].is_first) {
		    files[tfd].is_first = 0;
		    int l = dir_fill_header(retbuf, 0, files[tfd].dirpattern);
		    rv = l;
		    files[tfd].de = dir_next(files[tfd].dp, files[tfd].dirpattern);
		    return rv;
		  }
		  if(!files[tfd].de) {
		    close_fds(tfd);
		    *eof = 1;
		    int l = dir_fill_disk(retbuf);
		    rv = l;
		    return rv;
		  }
		  int l = dir_fill_entry(retbuf, files[tfd].de, MAX_BUFFER_SIZE-FSP_DATA);
		  rv = l;
		  // prepare for next read (so we know if we're done)
		  files[tfd].de = dir_next(files[tfd].dp, files[tfd].dirpattern);
		  return rv;
}

// read file data
static int read_file(int tfd, char *retbuf, int len, int *eof) {

		int rv = 0;

		  FILE *fp = files[tfd].fp;

		  int n = fread(retbuf, 1, len, fp);
		  rv = n;
		  if(n<MAX_BUFFER_SIZE) {
		    *eof = 1;
		    close_fds(tfd);
		  } else {
		    // as feof() does not let us know if the file is EOF without
		    // having attempted to read it first, we need this kludge
		    int eofc = fgetc(fp);
		    if (eofc < 0) {
		      // EOF
		      *eof = 1;
		      close_fds(tfd);
		    } else {
		      // restore fp, so we can read it properly on the next request
		      ungetc(eofc, fp);
		      // do not send EOF
		    }
		  }
		  return rv;
}

// write file data
static int write_file(int tfd, char *buf, int len, int is_eof) {
		FILE *fp = files[tfd].fp;
		if(fp) {
		  if (len > FSP_DATA) {
		    // TODO: evaluate return value
		    int n = fwrite(buf+FSP_DATA, 1, len-FSP_DATA, fp);
		  }
		  if(is_eof) {
		    close_fds(tfd);
		  }
		  return 0;
		}
		return -1;
}

// ----------------------------------------------------------------------------------
// command channel

static int _delete_callback(const int num_of_match, const char *name) {

	printf("%d: Calling DELETE on: %s\n", num_of_match, name);

	if (unlink(name) < 0) {
		// error handling
		log_errno("While trying to unlink");

		switch(errno) {
		case EIO:
			return -22;	// read error no data
		default:
			return -20;	// read error no header
		}
	}
	return 0;	
}

static int fs_delete(char *buf, int *outdeleted) {

	int matches = 0;
	char *p = buf+FSP_DATA;

	do {
		// comma is file pattern separator
		char *pnext = index(p, ',');
		if (pnext != NULL) {
			*pnext = 0;	// write file name terminator (replacing the ',')
		}
		
		int rv = dir_call_matches(p, _delete_callback);	
		if (rv < 0) {
			// error happened
			return -rv;
		}
		matches += rv;

		p = (pnext == NULL) ? NULL : pnext+1;
	} 
	while (p != NULL);

	*outdeleted = matches;

	return 1;	// FILES SCRATCHED message
}	



// ----------------------------------------------------------------------------------

static int readfile(int chan, char *retbuf, int len, int *eof) {
	int rv = 0;
	File *f = &files[chan];
	if (f->dp) {
		rv = read_dir(chan, retbuf, eof);
	} else
	if (f->fp) {
		// read a file
		rv = read_file(chan, retbuf, len, eof);
	}
	return rv;
}


provider_t fs_provider = {
	"fs",
	fsp_init,
	close_fds,
	open_file,
	open_dr,
	readfile,
	write_file,
	fs_delete
};


