/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2013 Andre Fachat

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

#include <inttypes.h>
#include <string.h>
#include <ctype.h>

#include "mem.h"
#include "log.h"
#include "provider.h"
#include "log.h"
#include "handler.h"
#include "types.h"
#include "errors.h"
#include "wireformat.h"
#include "openpars.h"
#include "wildcard.h"

#define	X00_HEADER_LEN	0x1a

#define	MAX_NAME_LEN	17	/* 16 chars plus zero-terminator */

static handler_t x00_handler;

void x00_handler_init(void) {
	handler_register(&x00_handler);
}

typedef struct {
	file_t		file;		// embedded
} x00_file_t;

static void x00_file_init(const type_t *t, void *p) {
	(void) t; // silence unused warning

	x00_file_t *x = (x00_file_t*) p;

	x->file.type = FS_DIR_TYPE_UNKNOWN;
}

static type_t x00_file_type = {
	"x00_file",
	sizeof(x00_file_t),
	x00_file_init
};

/*
 * identify whether a given file is an x00 file type
 *
 * returns CBM_ERROR_OK even if no match found,
 * except in case of an error
 *
 * name is the current file name
 */
static int x00_resolve(file_t *infile, file_t **outfile, uint8_t type, const char *inname, const openpars_t *pars, const char **outname) {

	(void) type;

	log_debug("x00_resolve: infile=%s\n", infile->filename);

	// check the file name of the given file_t, if it actually is a Pxx file.

	// must be at least one character, plus "." plus "x00" ending
	if (infile->filename == NULL || strlen(infile->filename) < 5) {
		return CBM_ERROR_FILE_NOT_FOUND;
	}

	const char *name = conv_to_name_alloc(infile->filename, x00_handler.native_charset);

	log_debug("x00_resolve: infile converted to=%s\n", name);

	char typechar = 0;
	uint8_t ftype = 0;
	const char *p = name + strlen(name) - 4;

	if (*p != '.') {
		mem_free((char*)name);
		return CBM_ERROR_OK;
	}
	p++;
	typechar = *p;
	p++;
	if (!isdigit(*p)) {
		mem_free((char*)name);
		return CBM_ERROR_OK;
	}
	p++;
	if (!isdigit(*p)) {
		mem_free((char*)name);
		return CBM_ERROR_OK;
	}

	switch(typechar) {
	case 'P':
	case 'p':
		ftype = FS_DIR_TYPE_PRG;
		break;
	case 'S':
	case 's':
		ftype = FS_DIR_TYPE_SEQ;
		break;
	case 'U':
	case 'u':
		ftype = FS_DIR_TYPE_USR;
		break;
	case 'R':
	case 'r':
		ftype = FS_DIR_TYPE_REL;
		break;
	default:
		mem_free((char*)name);
		return CBM_ERROR_OK;
		break;
	}

	// ok, we have ensured we have an x00 file name
	// now make sure it actually is an x00 file

	// read x00 header
	uint8_t x00_buf[X00_HEADER_LEN];
	int flg;

	// seek to start of file
	infile->handler->seek(infile, 0, SEEKFLAG_ABS);
	// read p00 header
	infile->handler->readfile(infile, (char*)x00_buf, X00_HEADER_LEN, &flg);

	if (strcmp("C64File", (char*)x00_buf) != 0) { 
		mem_free((char*)name);
		return CBM_ERROR_FILE_NOT_FOUND;
	}

	// ok, we found a real x00 file
	log_info("Found %c00 file '%s' addressed as '%s'\n", typechar, x00_buf+8, name);

	// clear up
	mem_free((char*)name);

	// check with the open options

	// we don't care if write, overwrite, or read etc,
	// so there is no need to check for type

	if (x00_buf[0x18] != 0) {
		// corrupt header - zero is needed here for string termination
		return CBM_ERROR_FILE_TYPE_MISMATCH;
	}

	// now compare the original file name with the search pattern
	if (!compare_dirpattern((char*)&(x00_buf[8]), inname, outname)) {
		return CBM_ERROR_FILE_NOT_FOUND;
	}

	// check opts parameters
	
	if (pars->filetype != FS_DIR_TYPE_UNKNOWN && pars->filetype != ftype) {
		log_debug("Expected file type %d, found file type %d\n", pars->filetype, ftype);
		return CBM_ERROR_FILE_TYPE_MISMATCH;
	}

	if (ftype == FS_DIR_TYPE_REL && (pars->recordlen != 0 && pars->recordlen != x00_buf[0x19])) {
		return CBM_ERROR_RECORD_NOT_PRESENT;
	}

	// done, alloc x00_file and prepare for operation
	// no seek necessary, read pointer is already at start of payload

	x00_file_t *file = mem_alloc(&x00_file_type);

	file->file.isdir = 0;
	file->file.handler = &x00_handler;
	file->file.parent = infile;

	file->file.filename = mem_alloc_str((char*)(x00_buf+8));

	file->file.recordlen = x00_buf[0x19];
	file->file.type = ftype;

	file->file.attr = infile->attr;
	file->file.filesize = infile->filesize - X00_HEADER_LEN;

	*outfile = (file_t*)file;

	return CBM_ERROR_OK;
}

static void x00_close(file_t *file, int recurse) {
	x00_file_t *xfile = (x00_file_t*)file;

	// no resources to clean here, so just forward the close
	// we are a resolve wrapper, so close the inner file as well
	xfile->file.parent->handler->close(xfile->file.parent, recurse);

	// and then free the file struct memory
	mem_free(xfile);
}

static int x00_seek(file_t *file, long pos, int flag) {

	// add header offset, that's all
	switch(flag) {
	case SEEKFLAG_ABS:
		return file->parent->handler->seek(file->parent, pos + X00_HEADER_LEN, flag );
	case SEEKFLAG_END:
		return file->parent->handler->seek(file->parent, pos, flag );
	default:
		return CBM_ERROR_FAULT;
	}
}

static int x00_truncate(file_t *file, long pos) {

	// add header offset, that's all
	return file->parent->handler->truncate(file->parent, pos + X00_HEADER_LEN);
}

static int x00_read(file_t *file, char *buf, int len, int *readflg) {

	return file->parent->handler->readfile(file->parent, buf, len, readflg );
}

static int x00_write(file_t *file, char *buf, int len, int writeflg) {

	return file->parent->handler->writefile(file->parent, buf, len, writeflg );
}

static int x00_open(file_t *file, int opentype) {

	cbm_errno_t rv = file->parent->handler->open(file->parent, opentype);
	if (rv == CBM_ERROR_OK) {
		rv = x00_seek(file, 0, SEEKFLAG_ABS);
	}
	return rv;
}

static file_t* x00_parent(file_t *file) {
	if (file->parent != NULL) {
		return file->parent->handler->parent(file->parent);
	}
	return NULL;
}

static handler_t x00_handler = {
	"X00", 		//const char	*name;			// handler name, for debugging
	"ASCII",	//const char	*native_charset;	// get name of the native charset for that handler
	x00_resolve,	//int		(*resolve)(file_t *infile, file_t **outfile, 
			//		uint8_t type, const char *name, const char *opts); 

	x00_close, 	//void		(*close)(file_t *fp, int recurse);	// close the file

	x00_open,	//int		(*open)(file_t *fp); 	// open a file

	// -------------------------
			// get converter for DIR entries
//	NULL,

	x00_parent,	// file_t* parent(file_t*)

	// -------------------------
	
	x00_seek,	// position the file
			//int		(*seek)(file_t *fp, long abs_position);

	x00_read,	// read file data
			//int		(*readfile)(file_t *fp, char *retbuf, int len, int *readflag);	

	x00_write,	// write file data
			//int		(*writefile)(file_t *fp, char *buf, int len, int is_eof);	

	x00_truncate,	// truncate	(file_t *fp, long size);

	// -------------------------

	NULL,		// int direntry(file_t *fp, file_t **outentry);

	NULL		// int create(file_t *fp, file_t **outentry, cont char *name, uint8_t filetype, uint8_t reclen);
};


