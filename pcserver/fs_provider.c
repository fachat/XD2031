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
#include <stdlib.h>

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

typedef struct {
	// derived from endpoint_t
	struct provider_t 	*ptype;
	// payload
	File files[MAXFILES];
} fs_endpoint_t;

void fsp_init() {
}

extern provider_t fs_provider;

endpoint_t *fsp_new(const char *path) {

	fs_endpoint_t *fsep = malloc(sizeof(fs_endpoint_t));

	fsep->ptype = (struct provider_t *) &fs_provider;

	int i;
        for(i=0;i<MAXFILES;i++) {
          fsep->files[i].state = F_FREE;
        }
	return (endpoint_t*) fsep;
}

static void close_fds(endpoint_t *ep, int tfd);

void fsp_free(endpoint_t *ep) {
	int i;
        for(i=0;i<MAXFILES;i++) {
		close_fds(ep, i);
        }
	free(ep);
}

// ----------------------------------------------------------------------------------
// commands as sent from the device

// close a file descriptor
static void close_fds(endpoint_t *ep, int tfd) {
	File *files = ((fs_endpoint_t*)ep)->files;
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
static int open_file(endpoint_t *ep, int tfd, const char *buf, const char *mode) {
	File *files = ((fs_endpoint_t*)ep)->files;
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
		close_fds(ep, tfd);

		FILE *fp = open_first_match(nm, mode);
printf("OPEN_RD/AP/WR(%s: %s (@ %p))=%p\n",mode, buf, buf, (void*)fp);
		if(fp) {
		  files[tfd].fp = fp;
		  files[tfd].dp = NULL;
		  return 0;
		}
		// TODO: open error (maybe depending on errno?)
		return -1;
}

// open a directory read
static int open_dr(endpoint_t *ep, int tfd, const char *buf) {
	File *files = ((fs_endpoint_t*)ep)->files;
		// close the currently open files
		// so we don't loose references to open files
		close_fds(ep, tfd);

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
static int read_dir(endpoint_t *ep, int tfd, char *retbuf, int len, int *eof) {
	File *files = ((fs_endpoint_t*)ep)->files;
		int rv = 0;
		  if (files[tfd].is_first) {
		    files[tfd].is_first = 0;
		    int l = dir_fill_header(retbuf, 0, files[tfd].dirpattern);
		    rv = l;
		    files[tfd].de = dir_next(files[tfd].dp, files[tfd].dirpattern);
		    return rv;
		  }
		  if(!files[tfd].de) {
		    close_fds(ep, tfd);
		    *eof = 1;
		    int l = dir_fill_disk(retbuf);
		    rv = l;
		    return rv;
		  }
		  int l = dir_fill_entry(retbuf, files[tfd].de, len);
		  rv = l;
		  // prepare for next read (so we know if we're done)
		  files[tfd].de = dir_next(files[tfd].dp, files[tfd].dirpattern);
		  return rv;
}

// read file data
static int read_file(endpoint_t *ep, int tfd, char *retbuf, int len, int *eof) {
	File *files = ((fs_endpoint_t*)ep)->files;

		int rv = 0;

		  FILE *fp = files[tfd].fp;

		  int n = fread(retbuf, 1, len, fp);
		  rv = n;
		  if(n<MAX_BUFFER_SIZE) {
		    *eof = 1;
		    close_fds(ep, tfd);
		  } else {
		    // as feof() does not let us know if the file is EOF without
		    // having attempted to read it first, we need this kludge
		    int eofc = fgetc(fp);
		    if (eofc < 0) {
		      // EOF
		      *eof = 1;
		      close_fds(ep, tfd);
		    } else {
		      // restore fp, so we can read it properly on the next request
		      ungetc(eofc, fp);
		      // do not send EOF
		    }
		  }
		  return rv;
}

// write file data
static int write_file(endpoint_t *ep, int tfd, char *buf, int len, int is_eof) {
	File *files = ((fs_endpoint_t*)ep)->files;
		FILE *fp = files[tfd].fp;
		if(fp) {
		  // TODO: evaluate return value
		  int n = fwrite(buf, 1, len, fp);
		  if(is_eof) {
		    close_fds(ep, tfd);
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

static int fs_delete(endpoint_t *ep, char *buf, int *outdeleted) {
	File *files = ((fs_endpoint_t*)ep)->files;

	int matches = 0;
	char *p = buf;

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

static int open_file_rd(endpoint_t *ep, int tfd, const char *buf) {
       return open_file(ep, tfd, buf, "rb");
}

static int open_file_wr(endpoint_t *ep, int tfd, const char *buf) {
       return open_file(ep, tfd, buf, "wb");
}

static int open_file_ap(endpoint_t *ep, int tfd, const char *buf) {
       return open_file(ep, tfd, buf, "ab");
}


static int readfile(endpoint_t *ep, int chan, char *retbuf, int len, int *eof) {
	File *files = ((fs_endpoint_t*)ep)->files;
	int rv = 0;
	File *f = &files[chan];
	if (f->dp) {
		rv = read_dir(ep, chan, retbuf, len, eof);
	} else
	if (f->fp) {
		// read a file
		rv = read_file(ep, chan, retbuf, len, eof);
	}
	return rv;
}


provider_t fs_provider = {
	"fs",
	fsp_init,
	fsp_new,
	fsp_free,
	close_fds,
	open_file_rd,
	open_file_wr,
	open_file_ap,
	NULL, //open_file_rw,
	open_dr,
	readfile,
	write_file,
	fs_delete
};


