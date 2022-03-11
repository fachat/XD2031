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
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libgen.h>
#include <stdbool.h>

#include "provider.h"
#include "dir.h"
#include "handler.h"
#include "errors.h"
#include "mem.h"
#include "wireformat.h"
#include "channel.h"
#include "os.h"
#include "openpars.h"
#include "registry.h"
#include "wildcard.h"

#include "log.h"

#undef DEBUG_READ
#define DEBUG_CMD
#define DEBUG_BLOCK

#define	MAX_BUFFER_SIZE	64

//#define	min(a,b)	(((a)<(b))?(a):(b))

extern provider_t fs_provider;
extern handler_t fs_file_handler;

// prototype
static void fs_dump_file(file_t *fp, int recurse, int indent);
static void fsp_free(endpoint_t *ep);

// list of endpoints
static registry_t endpoints;

typedef struct {
	file_t		file;
	FILE		*fp;
	DIR 		*dp;
	uint8_t		temp_open;	// set when fp is temporary (for wrapper)
	const char	*ospath;	// full path to the file (incl. filename)
	struct dirent	*de;
	direntry_t	direntry;
	char		*block;		// direct channel block buffer, 256 byte when allocated
	unsigned char	block_ptr;
} File;

static void file_init(const type_t *t, void *obj) {
	(void) t;	// silence unused warning
	File *fp = (File*) obj;

	//log_debug("initializing fp=%p (used to be chan %d)\n", fp, fp == NULL ? -1 : fp->chan);

	//fp->file.pattern = NULL;
	//for (int i = 0; i < MAX_NAMEINFO_FILES; i++) {
	//	fp->file.searchpattern[i] = NULL;
	//}
	fp->file.handler = &fs_file_handler;
	fp->file.recordlen = 0;

	fp->fp = NULL;
	fp->dp = NULL;
	fp->de = NULL;
	fp->block = NULL;
	fp->block_ptr = 0;
	fp->temp_open = 0;
	fp->ospath = NULL;
}

static type_t file_type = {
	"fs_file",
	sizeof(File),
	file_init
};

typedef struct {
	// derived from endpoint_t
	endpoint_t	 	base;
	// payload
	char			*basepath;			// malloc'd base path
	char			*curpath;			// malloc'd current path
} fs_endpoint_t;

// root endpoint ("/" for resolves without parent from command line)
//static fs_endpoint_t *root_endpoint = NULL;
// home endpoint (run directory for resolves without parent from server)
//static fs_endpoint_t *home_endpoint = NULL;


static void endpoint_init(const type_t *t, void *obj) {
	(void) t;	// silence unused warning
	fs_endpoint_t *fsep = (fs_endpoint_t*)obj;
	reg_init(&(fsep->base.files), "fs_endpoint_files", 16);

	fsep->basepath = NULL;
	fsep->curpath = NULL;

	fsep->base.ptype = &fs_provider;

	fsep->base.is_temporary = 0;
	fsep->base.is_assigned = 0;

	reg_append(&endpoints, fsep);
}

static type_t endpoint_type = {
	"fs_endpoint",
	sizeof(fs_endpoint_t),
	endpoint_init
};

#if 0
static type_t block_type = {
	"direct_buffer",
	sizeof(char[256]),
	NULL
};
#endif

static type_t record_type = {
	"record_buffer",
	sizeof(char[65536]),
	NULL
};

static int expand_relfile(File *file, long cursize, long curpos);
static size_t file_get_size(FILE *fp);



static fs_endpoint_t *create_home_ep() {
	// ---------------------
	// root endpoint for non-parent resolves

	// alloc and init a new endpoint struct
	fs_endpoint_t *fsep = mem_alloc(&endpoint_type);

	// mallocs a buffer and stores the canonical real path in it
	// might get into os.c (Linux (m)allocates the buffer automatically in the right size)
	fsep->basepath = getcwd(NULL, 0);

	// copy into current path
	fsep->curpath = mem_alloc_str2(fsep->basepath, "fs_home_ep_path");
	fsep->base.is_assigned++;

	return fsep;
}

static fs_endpoint_t *create_root_ep() {
	// ---------------------
	// root endpoint for non-parent resolves

	// alloc and init a new endpoint struct
	fs_endpoint_t *fsep = mem_alloc(&endpoint_type);

	// malloc the root path "/"
	fsep->basepath = strdup(dir_separator_string());

	// copy into current path
	fsep->curpath = mem_alloc_str2(fsep->basepath, "fs_root_ep_path");
	fsep->base.is_assigned++;

	return fsep;
}

static void fsp_free_file(registry_t *reg, void *en) {

	(void) reg;

	((file_t*)en)->handler->fclose((file_t*)en, NULL, NULL);
}

static void fsp_free_ep(registry_t *reg, void *en) {
	(void) reg;
	fs_endpoint_t *fep = (fs_endpoint_t*) en;

	reg_free(&(fep->base.files), fsp_free_file);

	if (fep->basepath) {
		free(fep->basepath);
	}
	if (fep->curpath) {
		mem_free(fep->curpath);
	}
	mem_free(fep);
}

static void fsp_end() {
	reg_free(&endpoints, fsp_free_ep);
}

static void fsp_init() {

	// ---------------------
	// init endpoint registry
	reg_init(&endpoints, "fs endpoints", 10);

	//root_endpoint = create_root_ep();
	//home_endpoint = create_home_ep();

	// ---------------------
	// register provider

	provider_register(&fs_provider);
}

#if 0
// default endpoint if none given in assign
endpoint_t *fs_root_endpoint(const char *assign_path, char **new_assign_path, int from_cmdline) {
	if (from_cmdline) {
		if (assign_path[0] == dir_separator_char()) {
			// is root path
			*new_assign_path = mem_alloc_str2(assign_path, "fs_root_ep_assign_path");
		} else {
			*new_assign_path = malloc_path(home_endpoint->basepath, assign_path);
		}
		return (endpoint_t*) root_endpoint;
	} else {
		*new_assign_path = mem_alloc_str2(assign_path, "fs_root_ep_assign_path");
		return (endpoint_t*) home_endpoint;
	}
}
#endif

static File *reserve_file(fs_endpoint_t *fsep) {

	File *file = mem_alloc(&file_type);

	file->file.endpoint = (endpoint_t*)fsep;

	reg_append(&fsep->base.files, file);

	return file;
}

// close a file descriptor
static int close_fd(File *file) {

	log_debug("Closing file descriptor %p (%s)\n", file, file->ospath);
	int er = 0;

	if (file->ospath != NULL) {
		// ospath is malloc'd
		free((void*)file->ospath);
	}

	//for (int i = 0; i < MAX_NAMEINFO_FILES; i++) {
	//	if (file->file.searchpattern[i] != NULL) {
	//		mem_free((void*) file->file.searchpattern[i]);
	//	}
	//}
	//if (file->file.pattern != NULL) {
	//	mem_free((void*)file->file.pattern);
	//}
	if (file->file.filename != NULL) {
		mem_free((void*)file->file.filename);
	}

	if (file->fp != NULL) {
		fflush(file->fp);
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

	endpoint_t *ep = file->file.endpoint;

	// remove file from endpoint registry
	reg_remove(&(ep->files), file);

	// CHECK: only do this if endpoint is temporary?
	//if (ep->is_temporary && reg_size(&(ep->files)) == 0) {
	//	fsp_free(ep);
	//}

	mem_free(file);

	return er;
}


static void fsp_free(endpoint_t *ep) {
        fs_endpoint_t *cep = (fs_endpoint_t*) ep;

        if (reg_size(&ep->files)) {
                log_warn("fsp_free(): trying to close endpoint %p with %d open files!\n",
                        ep, reg_size(&ep->files));
                return;
        }
	if (ep->is_assigned > 0) {
		log_warn("fsp_free(): trying to free endpoint %p still assigned\n", ep);
		return;
	}

	reg_remove(&endpoints, cep);

	// basepath is malloc'd
	free(cep->basepath);
	// others are mem_alloc'd
	mem_free(cep->curpath);
        mem_free(ep);
}

static void fsp_ep_free(endpoint_t *ep) {

	if (ep->is_assigned > 0) {
		ep->is_assigned--;
	}

	if (ep->is_assigned == 0) {
		fsp_free(ep);
	}
}

#if 0
static endpoint_t *fsp_new(endpoint_t *parent, const char *path, charset_t cset, int from_cmdline) {

	(void) cset;

	char *new_assign_path = NULL;

	log_debug("fsp_new(parent=%p, path=%s\n", parent, path);

	if((path == NULL) || (*path == 0)) {
		log_error("Empty path for assign\n");
		return NULL;
	}

	if (parent != NULL && parent->ptype != &fs_provider) {
		log_error("Parent is not file system (but %s)\n", parent->ptype->name);
		return NULL;
	}

	endpoint_t *parentep = parent;

	// when no parent given (i.e. path without base), use root endpoint as base
	if (parentep == NULL) {
		parentep = fs_root_endpoint(path, &new_assign_path, from_cmdline);
	}


	if (new_assign_path != NULL) {
		// use handler_resolve_file with resolve_path and wrap into endpoint
		endpoint_t *assign_ep = NULL;
		int err = -1; //handler_resolve_assign(parentep, &assign_ep, new_assign_path, cset);
		mem_free(new_assign_path);
		if (err != CBM_ERROR_OK || assign_ep == NULL) {
			log_error("resolve path returned err=%d, p=%p\n", err, assign_ep);
			return NULL;
		}
		return assign_ep;
		
	}

	return parentep;
}
#endif

static endpoint_t *fsp_temp2(char **path, charset_t cset, int privileged) {

	(void) cset;

	log_debug("fsp_new(path=%s\n", *path);

	if((path == NULL) || (*path == 0)) {
		log_error("Empty path for assign\n");
		return NULL;
	}

	fs_endpoint_t *parentep = NULL;

	// use root endpoint as base if privileged and absolute path, use home otherwise
	parentep = (privileged && (**path == '/')) ? create_root_ep() : create_home_ep();

	// skip leading "/"
	if (**path == '/') {
		(*path)++;
	}

	return (endpoint_t*) parentep;
}


/**
 *make a dir into an endpoint (only called with isdir=1)
 */
static int fsp_to_endpoint(file_t *file, endpoint_t **outep) {

        if (file->handler != &fs_file_handler) {
                log_error("Wrong file type (unexpected)\n");
                return CBM_ERROR_FAULT;
        }

	File *fp = (File*) file;
	fs_endpoint_t *parentep = (fs_endpoint_t*) file->endpoint;

	// basepath is real malloc'd, not mem_alloc_*'d
	char *basepath = os_realpath(fp->ospath);

	// alloc and init a new endpoint struct
	fs_endpoint_t *fsep = mem_alloc(&endpoint_type);

	// mallocs a buffer and stores the canonical real path in it
	fsep->basepath = basepath;

	if (parentep != NULL) {
		// if we have a parent, make sure we do not
		// escape the parent container, i.e. basepath
		if (strstr(fsep->basepath, parentep->basepath) != fsep->basepath) {
			// the parent base path is not at the start of the new base path
			// so we throw an error
			log_error("ASSIGN broke out of container (%s), was trying %s\n",
				parentep->basepath, fsep->basepath);
			fsp_free((endpoint_t*)fsep);

			return CBM_ERROR_FAULT;
		}
	}

	// copy into current path
	fsep->curpath = mem_alloc_str2(fsep->basepath, "fs_curpath");

	// free resources
	close_fd(fp);

	*outep = (endpoint_t*)fsep;
	return CBM_ERROR_OK;
}

// ----------------------------------------------------------------------------------
// error translation

static int errno_to_error(int err) {

	switch(err) {
	case EEXIST:
		return CBM_ERROR_FILE_EXISTS;
	case EACCES:
		return CBM_ERROR_NO_PERMISSION;
	case ENAMETOOLONG:
		return CBM_ERROR_FILE_NAME_TOO_LONG;
	case ENOENT:
		return CBM_ERROR_FILE_NOT_FOUND;
	case ENOSPC:
		return CBM_ERROR_DISK_FULL;
	case EROFS:
		return CBM_ERROR_WRITE_PROTECT;
	case ENOTDIR:	// mkdir, rmdir
	case EISDIR:	// open, rename
		return CBM_ERROR_FILE_TYPE_MISMATCH;
	case ENOTEMPTY:
		return CBM_ERROR_DIR_NOT_EMPTY;
	case EMFILE:
		return CBM_ERROR_NO_CHANNEL;
	case EINVAL:
		return CBM_ERROR_SYNTAX_INVAL;
	default:
		return CBM_ERROR_FAULT;
	}
}


// ----------------------------------------------------------------------------------
// helpers

static char *safe_dirname (const char *path) {
/* a dirname that leaves it's parameter unchanged and doesn't
 * overwrite its result at subsequent calls. Allocates memory
 * that should be free()ed later */
	char *pathc, *dirname_result, *mem_dirname;

	pathc = mem_alloc_str2(path, "fs_safe_path");
	dirname_result = dirname(pathc);
	mem_dirname = mem_alloc_str2(dirname_result, "fs_safe_dirname");
	mem_free(pathc);
	return mem_dirname;
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
	base_dirc = mem_alloc_c_str(strlen(base_realpathc) + 2, "base realpath/");
	if(!base_dirc) {
		res = -3;
		goto exit;
	}
	strcpy(base_dirc, base_realpathc);
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
	path_realpathc = realloc(path_realpathc, strlen(path_realpathc) + 2);	// don't forget the null
	if(!path_realpathc) return -3;
	strcat(path_realpathc, dir_separator_string());

	log_debug("Check that path '%s' is under '%s'\n", path_realpathc, base_dirc);
	if(strstr(path_realpathc, base_dirc) == path_realpathc) {
		res = 0;
	} else {
		log_error("Path '%s' is not in base dir '%s'\n", path_realpathc, base_dirc);
	}
exit:
	// *_realpathc are malloc'd
	free(base_realpathc);
	free(path_realpathc);
	// others are mem_alloc'd
	mem_free(base_dirc);
	mem_free(path_dname);
	return res;
}



// ----------------------------------------------------------------------------------
// commands as sent from the device

// ----------------------------------------------------------------------------------
// block command handling

#if 0
static int open_block_channel(File *fp) {

	log_debug("Opening block channel %p\n", fp);

	fp->block = mem_alloc(&block_type);
	if (fp->block == NULL) {
		// alloc failed

		log_warn("Buffer memory alloc failed!");

		return CBM_ERROR_NO_CHANNEL;
	} else {
		memset(fp->block, 0, 256);
	}
	fp->block_ptr = 0;

	return CBM_ERROR_OK;
}
#endif

// in Firmware currently used for:
// B-A/B-F/U1/U2
static int fs_direct(endpoint_t *ep, const char *buf, char *retbuf, int *retlen) {

	(void) ep;

	// Note that buf has already consumed the drive (first byte), so all indexes are -1
	unsigned char cmd = buf[FS_BLOCK_PAR_CMD-1];
	unsigned int track = (buf[FS_BLOCK_PAR_TRACK-1]&0xff) | ((buf[FS_BLOCK_PAR_TRACK]<<8)&0xff00);
	unsigned int sector = (buf[FS_BLOCK_PAR_SECTOR-1]&0xff) | ((buf[FS_BLOCK_PAR_SECTOR]<<8)&0xff00);
	unsigned char channel = buf[FS_BLOCK_PAR_CHANNEL-1];

	log_debug("DIRECT cmd: %d, tr=%d, se=%d, chan=%d\n", cmd, track, sector, channel);

	// (bogus) check validity of parameters, otherwise fall through to error
	// need to be validated for other commands besides U1/U2
	if (track > 0 && sector < 100 && track < 100) {
		switch (cmd) {
		case FS_BLOCK_U1:
			// U1 basically opens a short-lived channel to read the contents of a
			// buffer into the device
			// channel is closed by device with separate FS_CLOSE
#if 0
			file = reserve_file((fs_endpoint_t*)ep);
			open_block_channel(file);
			// copy the file contents into the buffer
			// test
			for (int i = 0; i < 256; i++) {
				file->block[i] = i;
			}

			// TODO
			//handler_resolve_block(ep, channel, &fp);

			channel_set(channel, fp);
#endif		
			return CBM_ERROR_DRIVE_NOT_READY;
		case FS_BLOCK_U2:
			// U2 basically opens a short-lived channel to write the contents of a
			// buffer from the device
			// channel is closed by device with separate FS_CLOSE
#if 0
			file = reserve_file((fs_endpoint_t*)ep/*, channel*/);
			open_block_channel(file);

			// TODO
			//handler_resolve_block(ep, channel, &fp);

			channel_set(channel, fp);
		
			return CBM_ERROR_OK;
#endif
			return CBM_ERROR_DRIVE_NOT_READY;
		}
	}

	retbuf[0] = track & 0xff;
	retbuf[1] = (track >> 8) & 0xff;
	retbuf[2] = sector & 0xff;
	retbuf[3] = (sector >> 8) & 0xff;
	*retlen = 4;

	return CBM_ERROR_ILLEGAL_T_OR_S;
}

static int read_block(File *file, char *retbuf, int len, int *eof) {

#ifdef DEBUG_BLOCK
	log_debug("read_block: file=%p, len=%d\n", file, len);
#endif

	int avail = 256 - file->block_ptr;
	int n = len;
	if (len >= avail) {
		n = avail;
		*eof = READFLAG_EOF;
	}

#ifdef DEBUG_BLOCK
	log_debug("read_block: avail=%d, n=%d\n", avail, n);
#endif

	if (n > 0) {
		memcpy(retbuf, file->block+file->block_ptr, n);
		file->block_ptr += n;
	}
	return n;
}

static int write_block(File *file, const char *buf, int len, int is_eof) {

	(void)is_eof; // silence warning unused parameter

	log_debug("write_block: file=%p, len=%d\n", file, len);

	int avail = 256 - file->block_ptr;
	int n = len;
	if (len >= avail) {
		n = avail;
	}

	log_debug("read_block: avail=%d, n=%d\n", avail, n);

	if (n > 0) {
		memcpy(file->block+file->block_ptr, buf, n);
		file->block_ptr += n;
	}

#ifdef DEBUG_BLOCK
	log_debug("Block:");
	for (int i = 0; i < 256; i++) {
		log_debug(" %02x", file->block[i] & 0xff);
	}
#endif
	return CBM_ERROR_OK;
}

// ----------------------------------------------------------------------------------
// file command handling


/**
 * return a malloc'd string containing the concatenated contents of the
 * given strings (if not null)
 */
static char *str_concat(const char *str1, const char *str2, const char *str3) {

	char *rv = NULL;

	uint16_t len = 0;
	if (str1 != NULL) {
		len += strlen(str1);
	}
	if (str2 != NULL) {
		len += strlen(str2);
	}
	if (str3 != NULL) {
		len += strlen(str3);
	}

	rv = mem_alloc_c_str(len + 1, "str_concat");

	rv[0] = 0;
	if (str1 != NULL) {
		strcpy(rv, str1);
	}
	if (str2 != NULL) {
		strcat(rv, str2);
	}
	if (str3 != NULL) {
		strcat(rv, str3);
	}
	rv[len] = 0;	// just in case
	return rv;
}

/**
 * return a malloc'd string containing the concatenated contents of the
 * given strings (if not null)
 */
static char *str_concat_os(const char *str1, const char *str2, const char *str3) {

	char *rv = NULL;

	uint16_t len = 0;
	if (str1 != NULL) {
		len += strlen(str1);
	}
	if (str2 != NULL) {
		len += strlen(str2);
	}
	if (str3 != NULL) {
		len += strlen(str3);
	}

	rv = malloc(len + 1);

	rv[0] = 0;
	if (str1 != NULL) {
		strcpy(rv, str1);
	}
	if (str2 != NULL) {
		strcat(rv, str2);
	}
	if (str3 != NULL) {
		strcat(rv, str3);
	}
	rv[len] = 0;	// just in case
	return rv;
}


// open a directory read
static int open_dir(File *file) {

	DIR *dp = opendir(file->ospath); 

	log_debug("ENTER: OPEN_DR(%p), (file=%p, dp=%p)\n",(void*)dp,
			(void*)file, (file == NULL)?"<nil>":file->file.filename);

	if(dp != NULL) {
	  file->fp = NULL;
	  file->dp = dp;
	  file->file.dirstate = DIRSTATE_FIRST;
		  
	  log_exitr(CBM_ERROR_OK);
	  return CBM_ERROR_OK;
	} else {
	  log_errno("Error opening directory");
	  int er = errno_to_error(errno);
	  log_exitr(er);
	  return er;
	}
}

static int open_dr(fs_endpoint_t *fsep, const char *name, charset_t cset, File **outfile) {

	char *tmpnamep = NULL;
       	char *fullname = str_concat(fsep->curpath, dir_separator_string(), name);

	log_debug("ENTER: fs_provider.open_dr(name=%s, path=%s)\n", name, fullname);

	os_patch_dir_separator(fullname);
	if(path_under_base(fullname, fsep->basepath)) {
		mem_free(fullname);
		log_exitr(CBM_ERROR_NO_PERMISSION);
		return CBM_ERROR_NO_PERMISSION;
	}
	mem_free(fullname);

	File *file = reserve_file((fs_endpoint_t*)fsep);

	//file->file.pattern = NULL;
	// convert filename to external charset
	//tmpnamep = mem_alloc_str(name);
	tmpnamep = conv_name_alloc(name, cset, CHARSET_ASCII);
	file->file.filename = tmpnamep;

	file->ospath = os_realpath(fsep->curpath);

	*outfile = file;

	return CBM_ERROR_OK;
}


// root directory
static file_t *fsp_root(endpoint_t *ep) {

	fs_endpoint_t *fsep = (fs_endpoint_t*) ep;

	log_entry("fs_provider.fps_root");

	File *file = NULL;

	int err = open_dr(fsep, "/", CHARSET_ASCII, &file);

	if (err == CBM_ERROR_OK) {
		log_exitr(CBM_ERROR_OK);
		return (file_t*) file;
	}

	if (file != NULL) {
		file->file.handler->fclose((file_t*)file, NULL, NULL);
	}
	log_exitr(err);
	return NULL;
}


// TODO: put that ... into dir.c?
static size_t file_get_size(FILE *fp) {
	struct stat fdstat;
	fflush(fp);
	if(fstat(fileno(fp), &fdstat) < 0) {
		log_error("Could not stat file\n");
		return CBM_ERROR_DRIVE_NOT_READY;
	}
	return fdstat.st_size;
}


/**
 * return a malloc'd string with the full path of the file,
 * and the path separator char appended. if child is not NULL,
 * append it after the path separator
 */
static char *get_path(File *parent, const char *child) {

	char *path = NULL;

	if (child != NULL) {
		path = str_concat(parent->ospath, dir_separator_string(), child);
	} else {
		path = mem_alloc_str2(parent->ospath, "fs_os_path");
	}

	return path;
}

/**
 * get the next directory entry in the directory given as fp.
 * If isresolve is set, then the disk header and blocks free entries are skipped
 *
 * outpattern then points into fp->pattern
 */
static int fs_direntry2(file_t *fp, direntry_t **outentry, int isdirscan, int *readflag, const char *preview, charset_t cset) {
	
	(void) preview;
	(void) cset;

	  File *file = (File*) fp;

	  int rv = CBM_ERROR_FAULT;
	  struct stat sbuf;
	  fs_endpoint_t *fsep = (fs_endpoint_t*) fp->endpoint;
	  char *ospath = NULL;
	  char *path = NULL;

	  if (readflag) {
		  *readflag = READFLAG_DENTRY;
	  }

	  direntry_t *dirent = &(file->direntry);
	  dirent->handler = &fs_file_handler;
	  dirent->parent = fp;
	  *outentry = NULL;
	
	  log_debug("ENTER: fs_provider.direntry2 fp=%p, dirstate=%d\n", fp, fp->dirstate);

	  if (fp->handler != &fs_file_handler) {
		return CBM_ERROR_FAULT;
	  }

          if (strstr(file->ospath, fsep->basepath) != file->ospath) {
          	// the parent base path is not at the start of the new base path
          	// so we throw an error
          	log_error("DIR broke out of container (%s), was trying %s\n",
                                fsep->basepath, file->ospath);
          	return CBM_ERROR_FAULT;
          }

	  if (file->dp == NULL) {
		rv = open_dir(file);
		if (rv != CBM_ERROR_OK) {
			return rv;
		}
	  }
	  // do we have to send the disk header?
	  if ((isdirscan) && (fp->dirstate == DIRSTATE_FIRST)) {
		    // not first anymore
		fp->dirstate = DIRSTATE_ENTRIES;

		char *hdr = mem_alloc_c_str(17, "fs direntry header name");
		strncpy(hdr, preview, 16);
		hdr[16] = 0;
		int l = strlen(hdr);
		for (; l < 16; l++) {
			hdr[l] = 0x20;
		}
		
		dirent->name = (uint8_t*)hdr;
		dirent->cset = CHARSET_ASCII;
		dirent->mode = FS_DIR_MOD_NAM;

		rv = CBM_ERROR_OK;
		*outentry = dirent;
		return rv;
	  } 
	  // check if we have to send a file entry
	  if((!isdirscan) || (fp->dirstate == DIRSTATE_ENTRIES)) {

	            // read entry from underlying dir
		    do {
	            	file->de = readdir(file->dp);

	    	        if (file->de == NULL) {
				log_debug("Got NULL next dir entry\n");
				dirent->name = NULL;
				if (!isdirscan) {
					rv = CBM_ERROR_OK;
				} else {
					fp->dirstate = DIRSTATE_END;
				}
				rv = CBM_ERROR_FILE_NOT_FOUND;
				// done with search
				break;
			} else {
				log_debug("Got next dir entry for: %s\n", file->de->d_name);
				rv = CBM_ERROR_OK;

			    	path = get_path(file, file->de->d_name);
				ospath = os_realpath(path);
				mem_free(path);
				path = NULL;
					
		            	int rvx = stat(ospath, &sbuf);
        		    	if (rvx < 0) {
                			log_errno("Problem stat'ing dir entry (%s)", file->de->d_name);
					if (errno != EOVERFLOW) {
						rv = errno_to_error(errno);
						break;
					}
        		    	} else {
		    			dirent->name = (uint8_t*) file->de->d_name;

			  	  	dirent->mode = FS_DIR_MOD_FIL;
					// we don't know the type yet for sure
			    		dirent->type = FS_DIR_TYPE_PRG;
			    		dirent->attr = 0;

					if (S_ISREG(sbuf.st_mode)) {
				    		dirent->attr |= FS_DIR_ATTR_SEEK;
					}

					// write check
			        	// TODO: error handling
                			int writecheck = access(ospath, W_OK);
                			if ((writecheck < 0) && (errno != EACCES)) {
                            			writecheck = -errno;
	                            		log_error("Could not get write access to %s\n", file->de->d_name);
        	                    		log_errno("Reason");
                			}
					log_debug("WRITE Check: %s -> %d\n", ospath, writecheck);
					if (writecheck < 0) {
				    		dirent->attr |= FS_DIR_ATTR_LOCKED;
					}

					dirent->moddate = sbuf.st_mtime;
					dirent->size = sbuf.st_size;
					if (S_ISDIR(sbuf.st_mode)) {
						dirent->mode = FS_DIR_MOD_DIR;
					}
	  				*outentry = dirent;
					break;
				}
			}
			// read next entry
		    } while (1);
	  }
	  // end of dir entry - blocks free
	  if (isdirscan && (fp->dirstate == DIRSTATE_END)) {

		    dirent->name = NULL;
		    dirent->mode = FS_DIR_MOD_FRE;
		    size_t total = os_free_disk_space(file->ospath);
		    if (total > FS_DIR_LEN_MAX) {
			total = FS_DIR_LEN_MAX;
		    }
		    dirent->size = total;
		    if (readflag) {
			    *readflag = READFLAG_EOF;
		    }
		    rv = CBM_ERROR_OK;
	  	    *outentry = dirent;
	  	    return rv;
	  }
	  return rv;
}



// read file data
//
// returns positive number of bytes read, or negative error number
//
static int read_file(File *file, char *retbuf, int len, int *eof) {

	int rv = 0;

	FILE *fp = file->fp;

	int n = fread(retbuf, 1, len, fp);
	rv = n;
	if(n<len) {
		    // short read, so either error or eof
		    *eof = READFLAG_EOF;
		    log_debug("short read on %p\n", file);
	} else {
		    // as feof() does not let us know if the file is EOF without
		    // having attempted to read it first, we need this kludge
		    int eofc = fgetc(fp);
		    if (eofc < 0) {
		      // EOF
		      *eof = READFLAG_EOF;
		      log_debug("EOF on read %p\n", file);
		    } else {
		      // restore fp, so we can read it properly on the next request
		      ungetc(eofc, fp);
		      // do not send EOF
		    }
	}
	return rv;
}

static int expand_relfile(File *file, long cursize, long curpos) {
		FILE *fp = file->fp;
				char *buf = mem_alloc(&record_type);
				size_t n;
				int nrec;
				int brec;
				unsigned int rest;
				unsigned int rest2;
				long pos;
				memset(buf, 0, record_type.sizeoftype);
				// reset to end of file
				fseek(fp, cursize, SEEK_SET);
				// fill rest of last existing record when needed
				rest = cursize % file->file.recordlen;
				if (rest > 0) {
					rest = file->file.recordlen - rest;	
					log_debug("having to fill %d rest bytes\n", rest);
					cursize += rest;	// adjust
					while(rest) {
						if (rest > 256) {
							rest2 = 256;
						} else {
							rest2 = rest;
						}
						n = fwrite(buf, 1, rest2, fp);
						if (n < rest2) {
							log_errno("Could not write first filler record");
							mem_free(buf);
							return -CBM_ERROR_WRITE_ERROR;
						}
						rest -= n;
					}
				}
				// done filling rest of last record. Now all the other records
				// calculate up to what record would be filled by the drive
				// adjust newpos to record boundary (absolute file size)
				pos = curpos;
				n = pos % file->file.recordlen;
				// partial record used, adjust (ignore rest)
				pos = pos - n;
				// now check for blocks
				n = pos % 254;	// part used in last drive block
				if (n > 0) {
					// bytes in need to fill in last block
					n = 254 - n;
				}
				pos += n;
				// which make up this number of records
				// ignoring the rest of the division, as partial record 
				// at end of block is ignored. cursize is record-aligned,
				// so no problem just substracting it
				nrec = (pos - cursize) / file->file.recordlen;
				log_debug("calculate file to be %d bytes - %d records to write\n", pos, nrec);
				// prepare buffer
				n = 0;
				while (n < record_type.sizeoftype) {
					buf[n] = 255;
					n+= file->file.recordlen;
				}
				// number of records in buffer
				brec = (n / file->file.recordlen) - 1;
				// write filler records
				n = 1;
				while (n > 0 && nrec) {
					log_debug("append min(nrec=%d, brec=%d) records\n", nrec, brec);
					if (nrec >= brec) {
						n = fwrite(buf, brec * file->file.recordlen, 1, fp);
						nrec -= brec;
					} else {
						n = fwrite(buf, nrec * file->file.recordlen, 1, fp);
						// done
						break;
					}
				}
				// done with it
				mem_free(buf);
				if (n == 0) {
					log_errno("Could not write filler record");
					return -CBM_ERROR_WRITE_ERROR;
				}
	return CBM_ERROR_OK;
}

// write file data
static int write_file(File *file, const char *buf, int len, int is_eof) {

	//log_debug("write_file: file=%p\n", file);

	int err = CBM_ERROR_OK;

	FILE *fp = file->fp;

	if (file->file.recordlen > 0) {
		long curpos = ftell(fp);
		long cursize = file_get_size(fp);
		log_debug(">>>> current position is %ld, size=%ld\n", curpos, cursize);
		// now if curpos > cursize, fill up with "0xff 0x00 ..." for each missing
		// record
		if ((curpos + file->file.recordlen) > cursize) {
			// fill up such that currently written record has space
			expand_relfile(file, cursize, curpos + file->file.recordlen);
			// back to original file position
			fseek(fp, curpos, SEEK_SET);
		}
	}

	if(fp) {
	  // TODO: evaluate return value
	  int n = fwrite(buf, 1, len, fp);
	  if (n < len) {
		// short write indicates an error
		log_debug("Close fd=%p on short write (was %d, should be %d)!\n", file, n, len);
		log_debug("errno=%d, ferror()=%d\n", errno, ferror(fp));
		file->file.handler->dump((file_t*)file, 1, 1);
		if (ferror(fp)) {
			err = errno_to_error(errno);
		}
		fflush(fp);
		fclose(fp);
		file->fp = NULL;
		return -err;
	  } else {
		err = n;
	  }
	  if(is_eof) {
	    log_debug("fd=%d received an EOF on write file\n", file);
	    fflush(fp);
	    //close_fds(ep, tfd);
	  }
	  return err;
	}
	return -CBM_ERROR_FAULT;
}


// ----------------------------------------------------------------------------------
// command channel

static int fs_delete2(direntry_t *dirent) {

	int rv = CBM_ERROR_OK;

	File *parent = (File*) dirent->parent;

	const char *newospath = str_concat_os(parent->ospath, dir_separator_string(), (const char*) dirent->name);

	log_debug("fs_delete2 '%s'\n", newospath);

	if (unlink(newospath) < 0) {
		// error handling
		log_errno("While trying to unlink %s", newospath);

		rv = errno_to_error(errno);
	}

	free((void*)newospath);
	return rv;
}


static int fs_move2(direntry_t *dirent, file_t *todir, const char *toname, charset_t cset) {
	int er = CBM_ERROR_FAULT;

	File *parent = (File*) dirent->parent;
	char *frompath = str_concat_os(parent->ospath, dir_separator_string(), (const char*) dirent->name);

#ifdef DEBUG_CMD
	log_debug("fs_rename: '%s' -> '%s%s'\n", frompath, todir->filename, toname);
#endif

	if (strchr(toname, dir_separator_char()) != NULL) {
		return CBM_ERROR_DIR_ERROR;
	}

        // convert filename to external charset
        const char *tmpname = conv_name_alloc(toname, cset, CHARSET_ASCII);

	File *tofp = (File*) todir;
	const char *topath = malloc_path(tofp->ospath, tmpname);

	char *newreal = os_realpath(topath);

	if (newreal != NULL) {
		// file or directory exists
		log_errno("File exists %s", topath);
		er = CBM_ERROR_FILE_EXISTS;
	} else {
		int rv = rename(frompath, topath);
		if (rv < 0) {
			er = errno_to_error(errno);
			log_errno("Error renaming a file\n");
		} else {
			er = CBM_ERROR_OK;
		}
	}

	free(newreal);
	mem_free(topath);
	free(frompath);

	return er;
}


static int fs_mkdir(file_t *file, const char *name, charset_t cset, openpars_t *pars) {

	int er = CBM_ERROR_FAULT;

	(void) pars;	// silence unused warning

	fs_endpoint_t *fsep = (fs_endpoint_t*) file->endpoint;

	bool matched = false;
        const char *p = cconv_scan(name, cset, dir_separator_char(), "*?", &matched);
	if (p != NULL) {
		// no separator char
		log_error("target file name contained dir separator\n");
		return CBM_ERROR_SYNTAX_DIR_SEPARATOR;
	}
	if (matched) {
		// no separator char
		log_error("target file name contained wildcards\n");
		return CBM_ERROR_SYNTAX_WILDCARDS;
	}

        // convert filename to external charset
        const char *tmpnamep = conv_name_alloc(name, cset, CHARSET_ASCII);

	char *newpath = malloc_path(fsep->curpath, tmpnamep);

	mem_free(tmpnamep);

	char *newreal = os_realpath(newpath);

	if (newreal != NULL) {
		// file or directory exists
		log_errno("Error finding directory path %s", newpath);
		er = errno_to_error(errno);
	} else {
		mode_t oldmask=umask(0);
		int rv = os_mkdir(newpath, 0755);
		umask(oldmask);

		if (rv < 0) {
			log_errno("Error trying to make a directory");
			er = errno_to_error(errno);
		} else {
			// ok
			er = CBM_ERROR_OK;
		}
	}
	mem_free(newpath);
	free(newreal);
	return er;
}

static int fs_rmdir2(direntry_t *dirent) {

	int rv = CBM_ERROR_OK;

	File *parent = (File*) dirent->parent;

	const char *newospath = str_concat_os(parent->ospath, dir_separator_string(), (const char*) dirent->name);

	log_debug("fs_rmdir2 '%s'\n", newospath);

	if (rmdir(newospath) < 0) {
		// error handling
		log_errno("While trying to unlink %s", newospath);

		rv = errno_to_error(errno);
	}

	mem_free(newospath);
	return rv;
}


// ----------------------------------------------------------------------------------

static int fs_open_temp(File *file) {
	
	cbm_errno_t rv = CBM_ERROR_OK;

	if (file->file.mode == FS_DIR_MOD_FIL) {
	
		if (file->fp == NULL) {
			file->fp = fopen(file->ospath, file->file.writable ? "r+b" : "rb");
			file->temp_open = 1;
			if (file->fp == NULL) {
				log_errno("fopen");
				rv = CBM_ERROR_FAULT;
			}
		}
	}
	return rv;
}

static int readfile(file_t *fp, char *retbuf, int len, int *readflag, charset_t outcset) {

	(void) outcset;

	File *f = (File*) fp;
#ifdef DEBUG_READ
	log_debug("fs_readfile file=%p (fp=%p, dp=%p, block=%p, len=%d, *readflag=%d)\n",
		f, f==NULL ? NULL : f->fp, f == NULL ? NULL : f->dp, f == NULL ? NULL : f->block, len, *readflag);
#endif
	int rv = fs_open_temp(f);
	if (rv != CBM_ERROR_OK) {
		return rv;
	}

	if (f->dp) {
		// read a directory entry
		rv = CBM_ERROR_FAULT; //read_dir(f, retbuf, len, outcset, readflag);
	} else
	if (f->fp) {
		// read a file
		// standard file read
		rv = read_file(f, retbuf, len, readflag);
	} else
	if (f->block != NULL) {
		// direct channel block buffer read
		rv = read_block(f, retbuf, len, readflag);
	}
	return rv;
}

// write file data
static int writefile(file_t *fp, const char *buf, int len, int is_eof) {

	File *file = (File*) fp;

	int rv = -CBM_ERROR_FAULT;

	//log_debug("write_file: file=%p\n", file);

	if (file->block != NULL) {
		rv = write_block(file, buf, len, is_eof);
	} else {
		rv = write_file(file, buf, len, is_eof);
	}	
	return rv;
}

static int fs_seek(file_t *fp, long position, int flag) {

	cbm_errno_t rv = CBM_ERROR_OK;

	int seekflag = (flag == SEEKFLAG_END) ? SEEK_END : SEEK_SET;

	log_debug("fs_seek(%p, %ld, %d)\n", fp, position, flag);

	File *file = (File*) fp;

	rv = fs_open_temp(file);

	if ((rv == CBM_ERROR_OK) && (file->fp != NULL)) {
		if (fseek(file->fp, position, seekflag) < 0) {
			rv = CBM_ERROR_FAULT;
			log_errno("Seek");
		}
	}
	return rv;
}



static int fs_open(file_t *fp, openpars_t *pars, int type) {

	cbm_errno_t rv = CBM_ERROR_OK;
	File *file = (File*) fp;

	if (file->temp_open && file->fp != NULL) {
		fclose(file->fp);
		file->temp_open = 0;
		file->fp = NULL;
	}

	int file_required = false;
	int file_must_not_exist = false;
	
	if (type == FS_OPEN_DR) {
		rv = open_dir(file);
	} else {
		if (type == FS_OPEN_RW) {
			// TODO check open block?
			// TODO: di_provider needs RW open

			// RW is only supported for REL files at the moment
			//if (pars->filetype != FS_DIR_TYPE_REL) {
			//	return CBM_ERROR_FILE_TYPE_MISMATCH;
			//}
		}

		file->file.recordlen = pars->recordlen;
		if (pars->filetype != FS_DIR_TYPE_REL) {
			// without REL files there is no record len
			file->file.recordlen = 0;
		} else {
			if (file->file.recordlen == 0) {
				// not specifying a record length means reading it 
				// from the file, which we do not support
				return CBM_ERROR_DRIVE_NOT_READY;
			}
		}

		int file_exists = !access(file->ospath, F_OK);

		char *flags = "";
		switch(type) {
		case FS_OPEN_RD: 
			flags = "rb";
			file_required = true;
			break;
		case FS_OPEN_WR: 
			flags = "wb"; 
			file_must_not_exist = true;
			break;
		case FS_OPEN_AP: 
			flags = "ab"; 
			file_required = true;
			break;
		case FS_OPEN_RW: 
			if (file_exists) {
				flags = "rb+"; 
			} else {
				flags = "wb+"; 
			}
			break;
		case FS_OPEN_OW: 
			flags = "wb"; 
			break;
		}

		if (file_required && !file_exists) {
			log_error("Unable to open '%s': file not found\n", file->ospath);
			rv = CBM_ERROR_FILE_NOT_FOUND;
		} else 
		if (file_must_not_exist && file_exists) {
			log_error("Unable to open '%s': file exists\n", file->ospath);
			rv = CBM_ERROR_FILE_EXISTS;
		} else {
			file->fp = fopen(file->ospath, flags);
			if (pars->filetype != FS_DIR_TYPE_UNKNOWN) {
				// set file type (we cross-match, as we don't save the file type
				// in the file system
				file->file.type = pars->filetype;
			}
			if (file->fp == NULL) {
				rv = errno_to_error(errno);
				log_errno("Error opening file '%s'\n", file->ospath);
			}
		}
	}
	return rv;
}

static File* create_file(direntry_t *dirent) {

	File *parent = (File*) dirent->parent;

	const char *newospath = str_concat_os(parent->ospath, dir_separator_string(), (const char*)dirent->name);

	File *newfp = reserve_file((fs_endpoint_t*)parent->file.endpoint);
	newfp->ospath = newospath;
	newfp->file.writable = (dirent->attr & FS_DIR_ATTR_LOCKED) ? 0 : 1;

	return newfp;
}

static int fs_open2(direntry_t *dirent, openpars_t *pars, int type, file_t **outfile) {

	File *file = create_file(dirent);
	*outfile = (file_t*) file;

	return fs_open((file_t*) file, pars, type);
}


static int fs_create(file_t *dirfp, file_t **outentry, const char *name, charset_t cset, openpars_t *pars,
                                int opentype) {

	cbm_errno_t rv = CBM_ERROR_OK;
	File *dir = (File*) dirfp;
	File *retfile = NULL;

        const char *tmpname = conv_name_alloc(name, cset, CHARSET_ASCII);

	if ((rv = os_filename_is_legal(tmpname)) == CBM_ERROR_OK) {

		const char *ospath = str_concat_os(dir->ospath, dir_separator_string(), tmpname);
		
		retfile = reserve_file((fs_endpoint_t*)dirfp->endpoint);
		
		retfile->ospath = ospath;

		retfile->file.writable = 1;
		retfile->file.seekable = 1;

		if ((rv = fs_open((file_t*)retfile, pars, opentype)) == CBM_ERROR_OK) {
			*outentry = (file_t*)retfile;	
		} else {
			log_debug("close on failing open (%p)", retfile);
			close_fd(retfile);
		}
	}

	mem_free(tmpname);

	return rv;
}

static int fs_fclose(file_t *fp, char *outbuf, int *outlen) {
	(void) outbuf;

	log_debug("fs_fclose(%p '%s')\n", fp, ((File*)fp)->ospath);

	//fs_dump_file(fp, 0, 1);

	close_fd((File*)fp);

	if (outlen != NULL) {
		*outlen = 0;
	}
	return CBM_ERROR_OK;
}

static int fs_declose(direntry_t *de) {

	log_debug("fs_declose(%p '%s')\n", de, de->name);

	if (de->mode == FS_DIR_MOD_NAM) {
		mem_free(de->name);
	}

	// do nothing, as our direntry is part of the directory's File struct
	
	return CBM_ERROR_OK;
}

// ----------------------------------------------------------------------------------

static int fs_resolve2(const char **pattern, charset_t cset, file_t **inoutdir) {
	
	if (*inoutdir == NULL) {
		return CBM_ERROR_FAULT;
	}
	
	file_t *f = *inoutdir;
	if (f->handler != &fs_file_handler) {
		return CBM_ERROR_FAULT;
	}

	File *fp = (File*) f;

	// we have a File from our fs_provider

	const char *pt = *pattern;
	bool matched = false;
	do {
		log_debug("Scanning pattern '%s'\n", pt);
	
		const char *p = cconv_scan(pt, cset, '/', "*?", &matched);

		if (p == NULL) {
			// final part of file-path, i.e. the filename
			log_debug("-> filename detected\n");
			return CBM_ERROR_FILE_EXISTS;
		}

		if (matched || strlen(pt) == 0) {
			// there are wildcards, so exit and let outer loop resolve it
			log_debug("-> wildcards detected\n");
			return CBM_ERROR_SYNTAX_WILDCARDS;
		}

		// TODO: handle charset

		const char *tmpname = mem_alloc_strn(pt, (p - pt));

		char *newospath = str_concat_os(fp->ospath, dir_separator_string(), tmpname);

		log_debug("check path-part '%s' as new os path:'%s'\n", tmpname, newospath);

	  	struct stat sbuf;
		if(stat(newospath, &sbuf) < 0) {
			log_errno("Error stat'ing file '%s'", newospath);
			mem_free(tmpname);
			free(newospath);
			return CBM_ERROR_DIR_NOT_FOUND;
		}
	
		// create new struct for subdir
	
		File *newfp = reserve_file((fs_endpoint_t*)fp->file.endpoint);
		newfp->ospath = newospath;
	
		// close old
		close_fd(fp);
	
		fp = newfp;
		pt = p;

		*inoutdir = (file_t*) fp;
		*pattern = pt;
	} while (true);
	
}

// ----------------------------------------------------------------------------------

static int fs_flush(file_t *fp) {
	
	File *file = (File*)fp;
	if (file->fp != NULL) {
		fflush(file->fp);
	}
	return CBM_ERROR_OK;
}

static int fs_equals(file_t *thisfile, file_t *otherfile) {

	if (otherfile->handler != &fs_file_handler) {
		return 1;
	}

	return strcmp(((File*)thisfile)->ospath, ((File*)otherfile)->ospath);
}

// ----------------------------------------------------------------------------------

static void fs_dump_file(file_t *fp, int recurse, int indent) {

	File *file = (File*)fp;
	const char *prefix = dump_indent(indent);

	log_debug("%shandler='%s';\n", prefix, file->file.handler->name);
	log_debug("%sparent='%p';\n", prefix, file->file.parent);
        if (recurse) {
                log_debug("%s{\n", prefix);
                if (file->file.parent != NULL && file->file.parent->handler->dump != NULL) {
                        file->file.parent->handler->dump(file->file.parent, 1, indent+1);
                }
                log_debug("%s}\n", prefix);
                
        }
	log_debug("%sisdir='%d';\n", prefix, file->file.isdir);
	log_debug("%sdirstate='%d';\n", prefix, file->file.dirstate);
	//log_debug("%spattern='%s';\n", prefix, file->file.pattern);
	log_debug("%sfilesize='%d';\n", prefix, file->file.filesize);
	log_debug("%sfilename='%s';\n", prefix, file->file.filename);
	log_debug("%srecordlen='%d';\n", prefix, file->file.recordlen);
	log_debug("%smode='%d';\n", prefix, file->file.mode);
	log_debug("%stype='%d';\n", prefix, file->file.type);
	log_debug("%sattr='%d';\n", prefix, file->file.attr);
	log_debug("%swritable='%d';\n", prefix, file->file.writable);
	log_debug("%sseekable='%d';\n", prefix, file->file.seekable);
	log_debug("%stemp_open='%d';\n", prefix, file->temp_open);
	log_debug("%sospath='%s';\n", prefix, file->ospath);
	
}

static void fs_dump_ep(fs_endpoint_t *fsep, int indent) {

	const char *prefix = dump_indent(indent);
	int newind = indent + 1;
	const char *eppref = dump_indent(newind);

	log_debug("%sprovider='%s';\n", prefix, fsep->base.ptype->name);
	log_debug("%sis_temporary='%d';\n", prefix, fsep->base.is_temporary);
	log_debug("%sis_assigned='%d';\n", prefix, fsep->base.is_assigned);
	log_debug("%sbasepath='%s';\n", prefix, fsep->basepath);
	log_debug("%scurrent_path='%s';\n", prefix, fsep->curpath);
	log_debug("%sfiles={;\n", prefix);
	for (int i = 0; ; i++) {
		File *file = (File*) reg_get(&fsep->base.files, i);
		log_debug("%s// file at %p\n", eppref, file);
		if (file != NULL) {
			log_debug("%s{\n", eppref, file);
			if (file->file.handler->dump != NULL) {
				file->file.handler->dump((file_t*)file, 0, newind+1);
			}
			log_debug("%s{\n", eppref, file);
		} else {
			break;
		}
	}
	log_debug("%s}\n", prefix);
}

static void fs_dump(int indent) {

	const char *prefix = dump_indent(indent);
	int newind = indent + 1;
	const char *eppref = dump_indent(newind);

	log_debug("%s// file system provider\n", prefix);
	log_debug("%sendpoints={\n", prefix);
	for (int i = 0; ; i++) {
		fs_endpoint_t *fsep = (fs_endpoint_t*) reg_get(&endpoints, i);
		if (fsep != NULL) {
			log_debug("%s// endpoint %p\n", eppref, fsep);
			log_debug("%s{\n", eppref);
			fs_dump_ep(fsep, newind+1);
			log_debug("%s}\n", eppref);
		} else {
			break;
		}
	}
	log_debug("%s}\n", prefix);
}

static size_t fs_realsize2(direntry_t *file) {

	// our direntry has the correct size already
	return file->size;
}

// ----------------------------------------------------------------------------------

handler_t fs_file_handler = {
	"fs_file_handler",
	fs_resolve2,		// resolve2
	NULL,			// wrap
	fs_fclose,		// close
	fs_declose,		// close
	fs_open2,		// open2
	handler_parent,		// default parent() implementation
	fs_seek,		// seek
	readfile,		// readfile
	writefile,		// writefile
	NULL,			// truncate
	fs_direntry2,		// direntry2
	fs_create,		// create
	fs_flush,		// flush data out to disk
        fs_equals,		// check if two files (e.g. d64 files are the same)
	fs_realsize2,		// real size of file (same as file->filesize here)
	fs_delete2,		// delete2 file
	fs_mkdir,		// create a directory
	fs_rmdir2,		// rmdir2 remove a directory
	fs_move2,		// move2 a file or directory
	fs_dump_file		// dump file
};

provider_t fs_provider = {
	"fs",
	CHARSET_ASCII_NAME,
	fsp_init,
	fsp_end,
	fsp_temp2,
	fsp_to_endpoint,	// to_endpoint
	fsp_ep_free,
	fsp_root,		// file_t* (*root)(endpoint_t *ep);  // root directory for the endpoint
	fs_direct,
	NULL,			// format
	fs_dump			// dump
};


