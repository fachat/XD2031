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

#include "os.h"

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libgen.h>

#include "dir.h"
#include "fscmd.h"
#include "provider.h"
#include "errors.h"
#include "mem.h"
#include "wireformat.h"

#include "log.h"

#undef DEBUG_READ
#define DEBUG_CMD

#define	MAX_BUFFER_SIZE	64

//#define	min(a,b)	(((a)<(b))?(a):(b))

typedef struct {
	int		chan;		// channel for which the File is
	FILE		*fp;
	DIR 		*dp;
	char		dirpattern[MAX_BUFFER_SIZE];
	struct dirent	*de;
	unsigned int	is_first :1;	// is first directory entry?
	char		*block;		// direct channel block buffer, 256 byte when allocated
	unsigned char	block_ptr;
} File;

typedef struct {
	// derived from endpoint_t
	struct provider_t 	*ptype;
	// payload
	char			*basepath;			// malloc'd base path
	char			*curpath;			// malloc'd current path
	File 			files[MAXFILES];
} fs_endpoint_t;

static type_t block_type = {
	"direct_buffer",
	sizeof(char[256])
};

void fsp_init() {
}

extern provider_t fs_provider;

static void init_fp(File *fp) {

	//log_debug("initializing fp=%p (used to be chan %d)\n", fp, fp == NULL ? -1 : fp->chan);

        fp->chan = -1;
	fp->fp = NULL;
	fp->dp = NULL;
	fp->block = NULL;
	fp->block_ptr = 0;
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
	if (file->block != NULL) {
		mem_free(file->block);
		file->block = NULL;
	}
	init_fp(file);
	return er;
}


static void fsp_free(endpoint_t *ep) {
        fs_endpoint_t *cep = (fs_endpoint_t*) ep;
        int i;
        for(i=0;i<MAXFILES;i++) {
                close_fd(&(cep->files[i]));
        }
	mem_free(cep->basepath);
	mem_free(cep->curpath);
        mem_free(ep);
}

static endpoint_t *fsp_new(endpoint_t *parent, const char *path) {

	fs_endpoint_t *parentep = (fs_endpoint_t*) parent;

	// alloc and init a new endpoint struct
	fs_endpoint_t *fsep = malloc(sizeof(fs_endpoint_t));
	fsep->basepath = NULL;
	fsep->curpath = NULL;
        for(int i=0;i<MAXFILES;i++) {
		init_fp(&(fsep->files[i]));
        }

	fsep->ptype = (struct provider_t *) &fs_provider;

	char *dirpath = malloc_path((parentep == NULL) ? NULL : parentep->curpath,
				path);

	// malloc's a buffer and stores the canonical real path in it
	fsep->basepath = os_realpath(dirpath);

	mem_free(dirpath);

	if (fsep->basepath == NULL) {
		// some problem with dirpath - maybe does not exist...
		log_errno("Could not resolve path for assign");
		fsp_free((endpoint_t*)fsep);
		return NULL;
	}

	if (parentep != NULL) {
		// if we have a parent, make sure we do not
		// escape the parent container, i.e. basepath
		if (strstr(fsep->basepath, parentep->basepath) != fsep->basepath) {
			// the parent base path is not at the start of the new base path
			// so we throw an error
			log_error("ASSIGN broke out of container (%s), was trying %s\n",
				parentep->basepath, fsep->basepath);
			fsp_free((endpoint_t*)fsep);

			return NULL;
		}
	}

	// copy into current path
	fsep->curpath = malloc(strlen(fsep->basepath) + 1);
	strcpy(fsep->curpath, fsep->basepath);

	log_info("FS provider set to real path '%s'\n", fsep->basepath);

	return (endpoint_t*) fsep;
}

// ----------------------------------------------------------------------------------
// error translation

static int errno_to_error(int err) {

	switch(err) {
	case EEXIST:
		return ERROR_FILE_EXISTS;
	case EACCES:
		return ERROR_NO_PERMISSION;
	case ENAMETOOLONG:
		return ERROR_FILE_NAME_TOO_LONG;
	case ENOENT:
		return ERROR_FILE_NOT_FOUND;
	case ENOSPC:
		return ERROR_DISK_FULL;
	case EROFS:
		return ERROR_WRITE_PROTECT;
	case ENOTDIR:	// mkdir, rmdir
	case EISDIR:	// open, rename
		return ERROR_FILE_TYPE_MISMATCH;
	case ENOTEMPTY:
		return ERROR_DIR_NOT_EMPTY;
	case EMFILE:
		return ERROR_NO_CHANNEL;
	case EINVAL:
		return ERROR_SYNTAX_INVAL;
	default:
		return ERROR_FAULT;
	}
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

			log_debug("reserving file %p for chan %d\n", fp, chan);

                        return &(cep->files[i]);
                }
        }
        log_warn("Did not find free fs session for channel=%d\n", chan);
        return NULL;
}

static File *find_file(endpoint_t *ep, int chan) {
        fs_endpoint_t *cep = (fs_endpoint_t*) ep;

        for (int i = 0; i < MAXFILES; i++) {
                if (cep->files[i].chan == chan) {
                        return &(cep->files[i]);
                }
        }
        log_warn("Did not find fs session for channel=%d\n", chan);
        return NULL;
}

// ----------------------------------------------------------------------------------
// helpers

static char *safe_dirname (const char *path) {
/* a dirname that leaves it's parameter unchanged and doesn't
 * overwrite its result at subsequent calls. Allocates memory
 * that should be free()ed later */
	char *pathc, *dirname_result, *mem_dirname;

	pathc = mem_alloc_str(path);
	dirname_result = dirname(pathc);
	mem_dirname = mem_alloc_str(dirname_result);
	mem_free(pathc);
	return mem_dirname;
}

static char *safe_basename (const char *path) {
/* a basename that leaves it's parameter unchanged and doesn't
 * overwrite it's result at subsequent calls. Allocates memory
 * that should be free()ed later */
	char *pathc, *basename_result, *mem_basename;

	pathc = mem_alloc_str(path);
	basename_result = basename(pathc);
	mem_basename = mem_alloc_str(basename_result);
	mem_free(pathc);
	return mem_basename;
}

static int path_under_base(const char *path, const char *base) {
/*
 * Return
 * -3 if malloc() failed
 * -2 if realpath(path) failed
 * -1 if realpath(base) failed
 *  0 if it is
 *  1 if it is not
 */
	int res = 1;
	char *base_realpathc = NULL;
	char *base_dirc = NULL;
	char *path_dname = NULL;
	char *path_realpathc = NULL;

	if(!base) return -1;
	if(!path) return -2;

	base_realpathc = os_realpath(base);
	if(!base_realpathc) {
		res = -1;
		log_error("Unable to get real path for '%s'\n", base);
		goto exit;
	}
	base_dirc = mem_alloc_c(strlen(base_realpathc) + 2, "base realpath/");
	strcpy(base_dirc, base_realpathc);
	if(!base_dirc) {
		res = -3;
		goto exit;
	}
	strcat(base_dirc, dir_separator_string());

	path_realpathc = os_realpath(path);
	if(!path_realpathc) {
		path_dname = safe_dirname(path);
		path_realpathc = os_realpath(path_dname);
	}
	if(!path_realpathc) {
		log_error("Unable to get real path for '%s'\n", path);
		res = -2;
		goto exit;
	}
	path_realpathc = realloc(path_realpathc, strlen(path_realpathc) + 1);
	if(!path_realpathc) return -3;
	strcat(path_realpathc, dir_separator_string());

	log_debug("Check that path '%s' is under '%s'\n", path_realpathc, base_dirc);
	if(strstr(path_realpathc, base_dirc) == path_realpathc) {
		res = 0;
	} else {
		log_error("Path '%s' is not in base dir '%s'\n", path_realpathc, base_dirc);
	}
exit:
	mem_free(base_realpathc);
	mem_free(base_dirc);
	mem_free(path_realpathc);
	mem_free(path_dname);
	return res;
}

// ----------------------------------------------------------------------------------
// commands as sent from the device

// ----------------------------------------------------------------------------------
// block command handling

static int open_block_channel(File *fp) {

	log_debug("Opening block channel %p\n", fp);

	fp->block = mem_alloc(&block_type);
	if (fp->block == NULL) {
		// alloc failed

		log_warn("Buffer memory alloc failed!");

		return ERROR_NO_CHANNEL;
	}
	fp->block_ptr = 0;

	return ERROR_OK;
}

// U1/U2/B-P/B-R/B-W
static int fs_block(endpoint_t *ep, int chan, char *buf) {

	// note: that is not true for all commands - B-P for example
	unsigned char cmd = buf[0];
	unsigned char drive = buf[1];
	unsigned char track = buf[2];
	unsigned char sector = buf[3];

	log_debug("BLOCK cmd: %d, dr=%d, tr=%d, se=%d\n", cmd, drive, track, sector);

	File *file = find_file(ep, chan);

	if (file != NULL) {

		// note: not for B-P
		// also B-R / B-W use block[0] as length value!
		file->block_ptr = 0;
	}

	return ERROR_OK;
}

static int read_block(endpoint_t *ep, int tfd, char *retbuf, int len, int *eof) {
	File *file = find_file(ep, tfd);

	log_debug("read_block: file=%p, len=%d\n", file, len);

	if (file != NULL) {
		int avail = 256 - file->block_ptr;
		int n = len;
		if (len >= avail) {
			n = avail;
			*eof = 1;
		}

		log_debug("read_block: avail=%d, n=%d\n", avail, n);

		if (n > 0) {
			memcpy(retbuf, file->block+file->block_ptr, n);
			file->block_ptr += n;
		}
		return n;
	}
	return -ERROR_FAULT;
}

// ----------------------------------------------------------------------------------
// file command handling


// close a file descriptor
static void close_fds(endpoint_t *ep, int tfd) {
	File *file = find_file(ep, tfd); // ((fs_endpoint_t*)ep)->files;
	if (file != NULL) {
		close_fd(file);
		init_fp(file);
	}
}

// open a file for reading, writing, or appending
static int open_file(endpoint_t *ep, int tfd, const char *buf, int fs_cmd) {
	int er = ERROR_FAULT;
	File *file;
	enum boolean { FALSE, TRUE };

	fs_endpoint_t *fsep = (fs_endpoint_t*) ep;

	log_info("open file for fd=%d in dir %s with name %s\n", tfd, fsep->curpath, buf);

	char *fullname = malloc_path(fsep->curpath, buf);
	patch_dir_separator(fullname);
	if(path_under_base(fullname, fsep->basepath)) {
		mem_free(fullname);
		return ERROR_NO_PERMISSION;
	}

	char *path     = safe_dirname(fullname);
	char *filename = safe_basename(fullname);

	char *options;
	int file_required = FALSE;
	int file_must_not_exist = FALSE;

	switch(fs_cmd) {
		case FS_OPEN_RD:
			options = "rb";
			file_required = TRUE;
			break;
		case FS_OPEN_WR:
			options = "wb";
			file_must_not_exist = TRUE;
			break;
		case FS_OPEN_AP:
			options = "ap";
			file_required = TRUE;
			break;
		case FS_OPEN_OW:
			options = "wb";
			break;
		default:
			log_error("Internal error: open_file with fs_cmd %d\n", fs_cmd);
			goto exit;
	}

	char *name = find_first_match(path, filename, path_is_file);
	if(!name) {
		// something with that name exists that isn't a file
		log_error("Unable to open '%s': not a file\n", filename);
		er = ERROR_FILE_TYPE_MISMATCH;
		goto exit;
	}
	int file_exists = !access(name, F_OK);
	if(file_required && !file_exists) {
		log_error("Unable to open '%s': file not found\n", name);
		er = ERROR_FILE_NOT_FOUND;
		goto exit;
	}
	if(file_must_not_exist && file_exists) {
		log_error("Unable to open '%s': file exists\n", name);
		er = ERROR_FILE_EXISTS;
		goto exit;
	}

	FILE *fp = fopen(name, options);
	if(fp) {

		file = reserve_file(ep, tfd);

		if (file) {
			file->fp = fp;
			file->dp = NULL;
			er = ERROR_OK;
		} else {
			fclose(fp);
			log_error("Could not reserve file\n");
			er = ERROR_FAULT;
		}

	} else {

		log_errno("Error opening file '%s/%s'", path, filename);
		er = errno_to_error(errno);
	}

	log_info("OPEN_RD/AP/WR(%s: %s (@ %p))=%p (fp=%p)\n", options, filename, filename, (void*)file, (void*)fp);

exit:
	mem_free(name); mem_free(path); mem_free(filename);
	return er;
}

// open a directory read
static int open_dr(endpoint_t *ep, int tfd, const char *buf) {

	fs_endpoint_t *fsep = (fs_endpoint_t*) ep;

        File *file = reserve_file(ep, tfd);

	if (file != NULL) {

		// save pattern for later comparisons
		strcpy(file->dirpattern, buf);
		DIR *dp = opendir(fsep->curpath /*buf+FSP_DATA*/);

		log_info("OPEN_DR(%s)=%p, (chan=%d, file=%p, dp=%p)\n",buf,(void*)dp,
							tfd, (void*)file, (void*)dp);

		if(dp) {
		  file->fp = NULL;
		  file->dp = dp;
		  file->is_first = 1;
		  return ERROR_OK;
		} else {
		  log_errno("Error opening directory");
		  int er = errno_to_error(errno);
		  close_fd(file);
		  return er;
		}
	}
	return ERROR_FAULT;
}

// read directory
//
// Note: there is a race condition, as we do not save the current directory path
// on directory open, so if it is changed "in the middle" of the operation,
// we run into trouble here. Hope noone will do that...
//
// returns the number of bytes read (>= 0), or a negative error number
//
static int read_dir(endpoint_t *ep, int tfd, char *retbuf, int len, int *eof) {

	fs_endpoint_t *fsep = (fs_endpoint_t*) ep;

	File *file = find_file(ep, tfd);

	//log_debug("read_dir: file=%p, is_first=%d\n", file, (file == NULL) ? -1 : file->is_first);

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
		  int l = dir_fill_entry(retbuf, fsep->curpath, file->de, len);
		  rv = l;
		  // prepare for next read (so we know if we're done)
		  file->de = dir_next(file->dp, file->dirpattern);
		  return rv;
	}
	return -ERROR_FAULT;
}

// read file data
//
// returns positive number of bytes read, or negative error number
//
static int read_file(endpoint_t *ep, int tfd, char *retbuf, int len, int *eof) {
	File *file = find_file(ep, tfd);

	if (file != NULL) {
		int rv = 0;

		  FILE *fp = file->fp;

		  int n = fread(retbuf, 1, len, fp);
		  rv = n;
		  if(n<len) {
		    // short read, so either error or eof
		    *eof = 1;
		    log_debug("Close fd=%d on short read\n", tfd);
		    close_fds(ep, tfd);
		  } else {
		    // as feof() does not let us know if the file is EOF without
		    // having attempted to read it first, we need this kludge
		    int eofc = fgetc(fp);
		    if (eofc < 0) {
		      // EOF
		      *eof = 1;
		      log_debug("Close fd=%d on EOF read\n", tfd);
		      close_fds(ep, tfd);
		    } else {
		      // restore fp, so we can read it properly on the next request
		      ungetc(eofc, fp);
		      // do not send EOF
		    }
		  }
		return rv;
	}
	return -ERROR_FAULT;
}

// write file data
static int write_file(endpoint_t *ep, int tfd, char *buf, int len, int is_eof) {
	File *file = find_file(ep, tfd);

	//log_debug("write_file: file=%p\n", file);

	if (file != NULL) {

		FILE *fp = file->fp;
		if(fp) {
		  // TODO: evaluate return value
		  int n = fwrite(buf, 1, len, fp);
		  if (n < len) {
			// short write indicates an error
			log_debug("Close fd=%d on short write!\n", tfd);
			close_fds(ep, tfd);
			return -ERROR_WRITE_ERROR;
		  }
		  if(is_eof) {
		    log_debug("Close fd=%d normally on write file received an EOF\n", tfd);
		    close_fds(ep, tfd);
		  }
		  return ERROR_OK;
		}
	}
	return -ERROR_FAULT;
}

// ----------------------------------------------------------------------------------
// command channel

static int _delete_callback(const int num_of_match, const char *name) {

	printf("%d: Calling DELETE on: %s\n", num_of_match, name);

	if (unlink(name) < 0) {
		// error handling
		log_errno("While trying to unlink");

		return -errno_to_error(errno);
	}
	return ERROR_OK;
}

static int fs_delete(endpoint_t *ep, char *buf, int *outdeleted) {

	int matches = 0;
	char *p = buf;

	patch_dir_separator(buf);

	fs_endpoint_t *fsep = (fs_endpoint_t*) ep;

	do {
		// comma is file pattern separator
		char *pnext = index(p, ',');
		if (pnext != NULL) {
			*pnext = 0;	// write file name terminator (replacing the ',')
		}

		int rv = dir_call_matches(fsep->curpath, p, _delete_callback);
		if (rv < 0) {
			// error happened
			return -rv;
		}
		matches += rv;

		p = (pnext == NULL) ? NULL : pnext+1;
	}
	while (p != NULL);

	*outdeleted = matches;

	return ERROR_SCRATCHED;	// FILES SCRATCHED message
}

static int fs_rename(endpoint_t *ep, char *nameto, char *namefrom) {

	int er = ERROR_FAULT;

	fs_endpoint_t *fsep = (fs_endpoint_t*) ep;

#ifdef DEBUG_CMD
	log_debug("fs_rename: '%s' -> '%s'\n", namefrom, nameto);
#endif

	if ((index(nameto, '/') != NULL) || (index(nameto,'\\') != NULL)) {
		// no separator char
		log_error("target file name contained dir separator\n");
		return ERROR_SYNTAX_DIR_SEPARATOR;
	}

	char *frompath = malloc_path(fsep->curpath, namefrom);
	char *topath = malloc_path(fsep->curpath, nameto);

	char *fromreal = os_realpath(frompath);
	mem_free(frompath);
	char *toreal = os_realpath(topath);

	if (toreal != NULL) {
		// target already exists
		er = ERROR_FILE_EXISTS;
	} else {
		// check both paths against container boundaries
		if ((strstr(fromreal, fsep->basepath) == fromreal)
			&& (strstr(topath, fsep->basepath) == topath)) {
			// ok

			int rv = rename(fromreal, topath);

			if (rv < 0) {
				er = errno_to_error(errno);
				log_errno("Error renaming a file\n");
			} else {
				er = ERROR_OK;
			}
		}
	}
	mem_free(topath);
	mem_free(toreal);
	mem_free(fromreal);

	return er;
}


static int fs_cd(endpoint_t *ep, char *buf) {
	fs_endpoint_t *fsep = (fs_endpoint_t*) ep;

	patch_dir_separator(buf);

	log_debug("Change dir to: %s\n", buf);

	//  concat new path to current path
	char *newpath = malloc_path(fsep->curpath, buf);

	// canonicalize it
	char *newreal = os_realpath(newpath);
	if (newreal == NULL) {
		// target does not exist
		log_error("Unable to change dir to '%s'\n", newpath);
		return ERROR_FILE_NOT_FOUND;
	}

	// free buffer so we don't forget it
	mem_free(newpath);

	// check if the new path is still under the base path
	if(path_under_base(newreal, fsep->basepath)) {
		// -> security error
		mem_free(newreal);
		return ERROR_NO_PERMISSION;
	}

	// check if the new path really is a directory
	struct stat path;
	if(stat(newreal, &path) < 0) {
		log_error("Could not stat '%s'\n", newreal);
		return ERROR_DIR_ERROR;
	}
	if(!S_ISDIR(path.st_mode)) {
		log_error("CHDIR: '%s' is not a directory\n", newreal);
		return ERROR_DIR_ERROR;
	}

	mem_free(fsep->curpath);
	fsep->curpath = newreal;
	return ERROR_OK;
}

static int fs_mkdir(endpoint_t *ep, char *buf) {

	int er = ERROR_DIR_ERROR;

	fs_endpoint_t *fsep = (fs_endpoint_t*) ep;

	patch_dir_separator(buf);

	char *newpath = malloc_path(fsep->curpath, buf);

	char *newreal = os_realpath(newpath);

	if (newreal != NULL) {
		// file or directory exists
		er = ERROR_FILE_EXISTS;
	} else {
		mode_t oldmask=umask(0);
		int rv = mkdir(newpath, 0755);
		umask(oldmask);

		if (rv < 0) {
			log_errno("Error trying to make a directory");
			er = errno_to_error(errno);
		} else {
			// ok
			er = ERROR_OK;
		}
	}
	mem_free(newpath);
	mem_free(newreal);
	return er;
}


static int fs_rmdir(endpoint_t *ep, char *buf) {

	int er = ERROR_FAULT;

	fs_endpoint_t *fsep = (fs_endpoint_t*) ep;

	patch_dir_separator(buf);

	char *newpath = malloc_path(fsep->curpath, buf);

	char *newreal = os_realpath(newpath);
	mem_free(newpath);

	if (newreal == NULL) {
		// directory does not exist
		er = ERROR_FILE_NOT_FOUND;
	} else
	if (strstr(newreal, fsep->basepath) == newreal) {
		// current path is still at the start of new path
		// so it is not broken out of the container

		int rv = rmdir(newreal);

		if (rv < 0) {
			er = errno_to_error(errno);
			log_errno("Error trying to remove a directory");
		} else {
			// ok
			er = ERROR_OK;
		}
	}
	mem_free(newreal);
	return er;
}



// ----------------------------------------------------------------------------------

static int open_file_rd(endpoint_t *ep, int tfd, const char *buf) {
       return open_file(ep, tfd, buf, FS_OPEN_RD);
}

static int open_file_wr(endpoint_t *ep, int tfd, const char *buf, const int is_overwrite) {
	if (is_overwrite) {
       		return open_file(ep, tfd, buf, FS_OPEN_OW);
	} else {
       		return open_file(ep, tfd, buf, FS_OPEN_WR);
	}
}

static int open_file_ap(endpoint_t *ep, int tfd, const char *buf) {
       return open_file(ep, tfd, buf, FS_OPEN_AP);
}

static int open_file_rw(endpoint_t *ep, int tfd, const char *buf) {
	if (*buf == '#') {
		// ok, open a direct block channel

		fs_endpoint_t *fsep = (fs_endpoint_t*) ep;

		File *file = reserve_file(ep, tfd);

		int er = open_block_channel(file);

		if (er) {
			// error
			close_fd(file);
			log_error("Could not reserve file\n");
		}
		return er;

	} else {
		// no support for r/w for "standard" files for now
		return ERROR_DRIVE_NOT_READY;
	}
}


static int readfile(endpoint_t *ep, int chan, char *retbuf, int len, int *eof) {

	File *f = find_file(ep, chan); // ((fs_endpoint_t*)ep)->files;
#ifdef DEBUG_READ
	log_debug("readfile chan %d: file=%p (fp=%p, dp=%p, block=%p, eof=%d)\n",
		chan, f, f==NULL ? NULL : f->fp, f == NULL ? NULL : f->dp, f == NULL ? NULL : f->block, *eof);
#endif
	int rv = 0;

	if (f->dp) {
		rv = read_dir(ep, chan, retbuf, len, eof);
	} else
	if (f->fp) {
		// read a file
		// standard file read
		rv = read_file(ep, chan, retbuf, len, eof);
	} else
	if (f->block != NULL) {
		// direct channel block buffer read
		rv = read_block(ep, chan, retbuf, len, eof);
	}
	return rv;
}

// ----------------------------------------------------------------------------------

provider_t fs_provider = {
	"fs",
	fsp_init,
	fsp_new,
	fsp_free,
	close_fds,
	open_file_rd,
	open_file_wr,
	open_file_ap,
	open_file_rw,
	open_dr,
	readfile,
	write_file,
	fs_delete,
	fs_rename,
	fs_cd,
	fs_mkdir,
	fs_rmdir,
	fs_block
};


