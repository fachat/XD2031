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
#include <stdbool.h>

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

typedef struct {
	direntry_t	de;		// embedded
	direntry_t	*parent_de;
	uint8_t		name[17];
} x00_dirent_t;

static type_t x00_dirent_type = {
	"x00_dirent",
	sizeof(x00_dirent_t),
	NULL
};


/*
 * identify whether a given file is an x00 file type
 *
 * returns CBM_ERROR_OK even if no match found,
 * except in case of an error
 *
 * name is the current file name
 */
static int x00_wrap(direntry_t *dirent, direntry_t **outde) {

	log_debug("x00_wrap: infile=%s\n", dirent->name);

	// check the file name of the given file_t, if it actually is a Pxx file.

	// must be at least one character, plus "." plus "x00" ending
	if (dirent->name == NULL || strlen(dirent->name) < 5) {
		// not found, but no error
		return CBM_ERROR_OK;
	}

	const char *name = conv_name_alloc(dirent->name, dirent->cset, CHARSET_ASCII);

	//log_debug("x00_resolve: infile converted to=%s\n", name);

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

	// clean up
	mem_free((char*)name);

	// ok, we have ensured we have an x00 file name
	// now make sure it actually is an x00 file

	// open it first
	openpars_t pars;
	openpars_init_options(&pars);
	file_t *infile = NULL;
	int rv = dirent->handler->open2(dirent, &pars, FS_OPEN_RD, &infile);

	if (rv != CBM_ERROR_OK) {
		return rv;
	}

	// read x00 header
	uint8_t x00_buf[X00_HEADER_LEN];
	int flg;

	// read p00 header
	infile->handler->readfile(infile, (char*)x00_buf, X00_HEADER_LEN, &flg, CHARSET_ASCII);

	// close file again
	infile->handler->fclose(infile, NULL, NULL);

	if (strcmp("C64File", (char*)x00_buf) != 0) { 
		// not a C64 x00 file
		return CBM_ERROR_FILE_NOT_FOUND;
	}

	// check with the open options

	// we don't care if write, overwrite, or read etc,
	// so there is no need to check for type

	if (x00_buf[0x18] != 0) {
		// corrupt header - zero is needed here for string termination
		infile->handler->fclose(infile, NULL, NULL);
		return CBM_ERROR_FILE_TYPE_MISMATCH;
	}

	// ok, we found a real x00 file
	log_info("Found %c00 file '%s' addressed as '%s'\n", typechar, x00_buf+8, name);

	// done, alloc x00_file and prepare for operation
	// no seek necessary, read pointer is already at start of payload

	x00_dirent_t *de = mem_alloc(&x00_dirent_type);

	de->de.parent = NULL;
	de->de.handler = &x00_handler;

	de->de.size = dirent->size - X00_HEADER_LEN;
	de->de.moddate = dirent->moddate;
	de->de.recordlen = x00_buf[0x19];
	de->de.mode = dirent->mode;
	de->de.attr = dirent->attr;
	de->de.type = ftype;
	de->de.cset = CHARSET_PETSCII;

	memcpy(&de->name, (char*)&(x00_buf[8]), 16);
	de->name[16] = 0; // zero-terminate
	de->de.name = &de->name;

	de->parent_de = dirent;

	*outde = de;

	return CBM_ERROR_OK;
}

static int x00_open2(direntry_t *de, openpars_t *pars, int opentype, file_t **outfp) {

	x00_dirent_t *dirent = (x00_dirent_t*) de;

	// alloc x00_file and prepare for operation
	// no seek necessary, read pointer is already at start of payload

	file_t *infile = NULL;
	int rv = dirent->parent_de->handler->open2(dirent->parent_de, pars, opentype, &infile);

	if (rv != CBM_ERROR_OK) {
		return rv;
	}

	// TODO: if FS_OPEN_RD/RW, read, otherwise seek, return if not seekable
	// read p00 header
	uint8_t x00_buf[X00_HEADER_LEN];
	int flg;
	infile->handler->readfile(infile, (char*)x00_buf, X00_HEADER_LEN, &flg, CHARSET_ASCII);

	x00_file_t *file = mem_alloc(&x00_file_type);

	file->file.isdir = 0;
	file->file.handler = &x00_handler;
	file->file.parent = infile;
	file->file.filename = mem_alloc_str(dirent->name);
	file->file.recordlen = dirent->de.recordlen;
	file->file.attr = dirent->de.attr;
	file->file.type = dirent->de.type;
	file->file.filesize = dirent->de.size;

	*outfp = (file_t*)file;

	return CBM_ERROR_OK;
}

/*
 * identify whether a given file is an x00 file type
 *
 * returns CBM_ERROR_OK even if no match found,
 * except in case of an error
 *
 * name is the current file name
 */
static int x00_resolve(file_t *infile, file_t **outfile, const char *inname, charset_t cset, const char **outname) {

	//log_debug("x00_resolve: infile=%s\n", infile->filename);

	// check the file name of the given file_t, if it actually is a Pxx file.

	// must be at least one character, plus "." plus "x00" ending
	if (infile->filename == NULL || strlen(infile->filename) < 5) {
		// not found, but no error
		return CBM_ERROR_OK;
	}

	const char *name = conv_name_alloc(infile->filename, cset, CHARSET_ASCII);

	//log_debug("x00_resolve: infile converted to=%s\n", name);

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

	if (!infile->seekable) {
		log_warn("Found x00 file '%s', but is not seekable!\n", infile->filename);
		return CBM_ERROR_FILE_TYPE_MISMATCH;
	}

	// seek to start of file
	infile->handler->seek(infile, 0, SEEKFLAG_ABS);
	// read p00 header
	infile->handler->readfile(infile, (char*)x00_buf, X00_HEADER_LEN, &flg, cset);

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

	const char *xname = conv_name_alloc((char*)&(x00_buf[8]), CHARSET_PETSCII, cset);

	// now compare the original file name with the search pattern
	if (!compare_dirpattern(xname, inname, outname)) {
		mem_free(xname);
		return CBM_ERROR_FILE_NOT_FOUND;
	}


	// done, alloc x00_file and prepare for operation
	// no seek necessary, read pointer is already at start of payload

	x00_file_t *file = mem_alloc(&x00_file_type);

	file->file.isdir = 0;
	file->file.handler = &x00_handler;
	file->file.parent = infile;

	file->file.filename = xname;

	file->file.recordlen = x00_buf[0x19];
	file->file.type = ftype;

	file->file.attr = infile->attr;
	file->file.filesize = infile->filesize - X00_HEADER_LEN;

	*outfile = (file_t*)file;

	return CBM_ERROR_OK;
}

static int x00_declose(direntry_t *de) {

	mem_free(de);
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


static void x00_dump(file_t *file, int recurse, int indent) {

	const char *prefix = dump_indent(indent);

	log_debug("%s// x00 file\n", prefix);
	log_debug("%sparent={\n", prefix);
	if (file->parent != NULL && file->parent->handler->dump != NULL) {
		file->parent->handler->dump(file->parent, recurse, indent+1);
	}
	log_debug("%s}\n", prefix);
}

static int x00_equals(file_t *thisfile, file_t *otherfile) {

        if (otherfile->handler != &x00_handler) {
                return 1;
        }

        return thisfile->parent->handler->equals(thisfile->parent, otherfile->parent);

}

static size_t x00_realsize(file_t *file) {

	if (file->parent != NULL) {
		return file->parent->handler->realsize(file->parent) - X00_HEADER_LEN;
	}
	return CBM_ERROR_FAULT;
}



static handler_t x00_handler = {
	"X00", 		//const char	*name;			// handler name, for debugging

	NULL,		// resolve2

	x00_resolve,	//int		(*resolve)(file_t *infile, file_t **outfile, 
			//		uint8_t type, const char *name, const char *opts); 
	x00_wrap,	// wrap

	default_close, 	//void		(*close)(file_t *fp, int recurse);	// close the file

	default_fclose,	// fclose
	x00_declose,	// declose
	
	default_open,	//int		(*open)(file_t *fp); 	// open a file

	x00_open2,	//int		(*open2)(direntry_t *fp); 	// open a file

	// -------------------------
			// get converter for DIR entries
//	NULL,

	default_parent,	// file_t* parent(file_t*)

	// -------------------------
	
	x00_seek,	// position the file
			//int		(*seek)(file_t *fp, long abs_position);

	default_read,	// read file data
			//int		(*readfile)(file_t *fp, char *retbuf, int len, int *readflag);	

	default_write,	// write file data
			//int		(*writefile)(file_t *fp, char *buf, int len, int is_eof);	

	x00_truncate,	// truncate	(file_t *fp, long size);

	// -------------------------

	NULL,		// int direntry2(file_t *fp, file_t **outentry);

	NULL,		// int direntry(file_t *fp, file_t **outentry);

	NULL,		// int create(file_t *fp, file_t **outentry, cont char *name, uint8_t filetype, uint8_t reclen);

	// -------------------------

	default_flush,

	x00_equals,

	x00_realsize,

	default_scratch,

	NULL,		// delete2

	NULL,		// mkdir not supported

	NULL,		// rmdir not supported

	NULL,		// rmdir2 not supported

	NULL,		// move not supported

	NULL,		// move2 not supported

	// -------------------------

	x00_dump
};


