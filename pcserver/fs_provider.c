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
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "dir.h"
#include "fscmd.h"
#include "provider.h"

#include "log.h"

#undef DEBUG_READ

#define	MAX_BUFFER_SIZE	64

//#define	min(a,b)	(((a)<(b))?(a):(b))

typedef struct {
	int		chan;		// channel for which the File is
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
	char			*basepath;			// malloc'd base path
	char			*curpath;			// malloc'd current path
	File 			files[MAXFILES];
} fs_endpoint_t;

void fsp_init() {
}

extern provider_t fs_provider;

static void init_fp(File *fp) {

	//log_debug("initializing fp=%p (used to be chan %d)\n", fp, fp == NULL ? -1 : fp->chan);

        fp->chan = -1;
}

endpoint_t *fsp_new(const char *path) {

	fs_endpoint_t *fsep = malloc(sizeof(fs_endpoint_t));

	fsep->ptype = (struct provider_t *) &fs_provider;

	// malloc's a buffer and stores the canonical real path in it
	fsep->basepath = realpath(path, NULL);
	// copy into current path
	fsep->curpath = malloc(strlen(fsep->basepath) + 1);
	strcpy(fsep->curpath, fsep->basepath);

	log_info("FS provider set to real path '%s'\n", fsep->basepath);

	int i;
        for(i=0;i<MAXFILES;i++) {
		init_fp(&(fsep->files[i]));
        }
	return (endpoint_t*) fsep;
}

// close a file descriptor
static int close_fd(File *file) {
	int er = 0;
	if (file->fp != NULL) {
		er = fclose(file->fp);
		if (er < 0) {
			log_errno("Error closing fd");
		}
		file->fp = NULL;
	}
	if (file->dp != NULL) {
		er = closedir(file->dp);
		if (er < 0) {
			log_errno("Error closing dp");
		}
		file->dp = NULL;
	}
	init_fp(file);
	return er;
}


void fsp_free(endpoint_t *ep) {
        fs_endpoint_t *cep = (fs_endpoint_t*) ep;
        int i;
        for(i=0;i<MAXFILES;i++) {
                close_fd(&(cep->files[i]));
        }
        free(ep);
}


// ----------------------------------------------------------------------------------
//

static File *reserve_file(endpoint_t *ep, int chan) {
        fs_endpoint_t *cep = (fs_endpoint_t*) ep;

        for (int i = 0; i < MAXFILES; i++) {
                if (cep->files[i].chan == chan) {
                        close_fd(&(cep->files[i]));
                }
                if (cep->files[i].chan < 0) {
                        File *fp = &(cep->files[i]);
                        init_fp(fp);
                        fp->chan = chan;

			//log_debug("reserving file %p for chan %d\n", fp, chan);

                        return &(cep->files[i]);
                }
        }
        log_warn("Did not find free curl session for channel=%d\n", chan);
        return NULL;
}

static File *find_file(endpoint_t *ep, int chan) {
        fs_endpoint_t *cep = (fs_endpoint_t*) ep;

        for (int i = 0; i < MAXFILES; i++) {
                if (cep->files[i].chan == chan) {
                        return &(cep->files[i]);
                }
        }
        log_warn("Did not find curl session for channel=%d\n", chan);
        return NULL;
}

// ----------------------------------------------------------------------------------
// commands as sent from the device

// close a file descriptor
static void close_fds(endpoint_t *ep, int tfd) {
	File *file = find_file(ep, tfd); // ((fs_endpoint_t*)ep)->files;
	if (file != NULL) {
		close_fd(file);
		init_fp(file);
	}
}

// open a file for reading, writing, or appending
static int open_file(endpoint_t *ep, int tfd, const char *buf, const char *mode) {

        File *file = reserve_file(ep, tfd);

	if (file != NULL) {
		/* no directory separators - security rules! */
		char *nm = (char*)buf;
		if(*nm=='/') nm++;
		if(strchr(nm, '/')) {
			// should give a security error
			// TODO: replace with correct error number
			return -1;
		}

		FILE *fp = open_first_match(nm, mode);

		log_info("OPEN_RD/AP/WR(%s: %s (@ %p))=%p\n",mode, buf, buf, (void*)fp);

		if(fp) {
		  file->fp = fp;
		  file->dp = NULL;
		  return 0;
		}
		// TODO: open error (maybe depending on errno?)
	}
	return -1;
}

// open a directory read
static int open_dr(endpoint_t *ep, int tfd, const char *buf) {

        File *file = reserve_file(ep, tfd);
	
	if (file != NULL) {

		// save pattern for later comparisons
		strcpy(file->dirpattern, buf);
		DIR *dp = opendir("." /*buf+FSP_DATA*/);

		log_info("OPEN_DR(%s)=%p, (chan=%d, file=%p, dp=%p)\n",buf,(void*)dp,
							tfd, (void*)file, (void*)dp);

		if(dp) {
		  file->fp = NULL;
		  file->dp = dp;
		  file->is_first = 1;
		  return 0;
		}
		// TODO: open error (maybe depending on errno?)
	}
	return -1;
}

// read directory
static int read_dir(endpoint_t *ep, int tfd, char *retbuf, int len, int *eof) {

	File *file = find_file(ep, tfd);

	//log_debug("read_dir: file=%p\n", file);

	if (file != NULL) {
		int rv = 0;
		  if (file->is_first) {
		    file->is_first = 0;
		    int l = dir_fill_header(retbuf, 0, file->dirpattern);
		    rv = l;
		    file->de = dir_next(file->dp, file->dirpattern);
		    return rv;
		  }
		  if(!file->de) {
		    close_fds(ep, tfd);
		    *eof = 1;
		    int l = dir_fill_disk(retbuf);
		    rv = l;
		    return rv;
		  }
		  int l = dir_fill_entry(retbuf, file->de, len);
		  rv = l;
		  // prepare for next read (so we know if we're done)
		  file->de = dir_next(file->dp, file->dirpattern);
		  return rv;
	}
	return -22;
}

// read file data
static int read_file(endpoint_t *ep, int tfd, char *retbuf, int len, int *eof) {
	File *file = find_file(ep, tfd);

	if (file != NULL) {
		int rv = 0;

		  FILE *fp = file->fp;

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
	return -22;
}

// write file data
static int write_file(endpoint_t *ep, int tfd, char *buf, int len, int is_eof) {
	File *file = find_file(ep, tfd);

	if (file != NULL) {

		FILE *fp = file->fp;
		if(fp) {
		  // TODO: evaluate return value
		  int n = fwrite(buf, 1, len, fp);
		  if(is_eof) {
		    close_fds(ep, tfd);
		  }
		  return 0;
		}
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

static int fs_rename(endpoint_t *ep, char *buf) {
}


static int fs_cd(endpoint_t *ep, char *buf) {
	fs_endpoint_t *fsep = (fs_endpoint_t*) ep;

	log_debug("Change dir to: %s\n", buf);

	//  concat new path to current path
	char *newpath = malloc(strlen(fsep->curpath) + strlen(buf) + 2);
	strcpy(newpath, fsep->curpath);
	strcat(newpath, "/");
	strcat(newpath, buf);

	// canonicalize it
	char *newreal = realpath(newpath, NULL);

	// free buffer so we don't forget it
	free(newpath);

	// check if the new path is still under the base path
	if (strstr(newreal, fsep->basepath) == newreal) {
		// the needle base path is found at the start of the new real path
		// -> ok
		int rv = chdir(newreal);
		if (rv < 0) {
			log_errno("Error changing directory");
			free(newreal);
			return -22;
		} 
		free(fsep->curpath);
		fsep->curpath = newreal;
	} else {
		// needle (base path) is not in haystack (new path)
		// -> security error
		log_error("Tried to chdir outside base dir %s, to %s\n", fsep->basepath, newreal);
		free(newreal);
		return -22;
	}
	return 0;
}


static int fs_mkdir(endpoint_t *ep, char *buf) {

	int rv = mkdir(buf, 0557);

	if (rv < 0) {
		log_errno("Error trying to make a directory");
		return -22;
	}
	return 0;
}


static int fs_rmdir(endpoint_t *ep, char *buf) {

	int rv = rmdir(buf);

	if (rv < 0) {
		log_errno("Error trying to remove a directory");
		return -22;
	}
	return 0;
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

	File *f = find_file(ep, chan); // ((fs_endpoint_t*)ep)->files;

	//log_debug("read_dir chan %d: file=%p (fp=%p, dp=%p)\n", 
	//	chan, f, f==NULL ? NULL : f->fp, f == NULL ? NULL : f->dp);

	int rv = 0;

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
	fs_delete,
	fs_rename,
	fs_cd,
	fs_mkdir,
	fs_rmdir
};


