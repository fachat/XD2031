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
#include "handler.h"
#include "types.h"
#include "errors.h"
#include "wireformat.h"
#include "openpars.h"

static handler_t x00_handler;

void x00_handler_init(void) {
	handler_register(&x00_handler);
}

typedef struct {
	file_t		file;		// embedded
	uint8_t		recordlen;
	uint8_t		filetype;
} x00_file_t;

static type_t x00_file_type = {
	"x00_file",
	sizeof(x00_file_t)
};

/*
 * identify whether a given file is an x00 file type
 *
 * name is the current file name
 */
static int x00_resolve(const file_t *infile, file_t **outfile, uint8_t type, const char *name, const char *opts) {

	(void) type;

	// must be at least one character, plus "." plus "x00" ending
	if (name == NULL || strlen(name) < 5) {
		return CBM_ERROR_FILE_NOT_FOUND;
	}

	char typechar = 0;
	uint8_t ftype = 0;
	const char *p = name + strlen(name) - 4;

	if (*p != '.') {
		return CBM_ERROR_FILE_NOT_FOUND;
	}
	p++;
	typechar = *p;
	p++;
	if (!isdigit(*p)) {
		return CBM_ERROR_FILE_NOT_FOUND;
	}
	p++;
	if (!isdigit(*p)) {
		return CBM_ERROR_FILE_NOT_FOUND;
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
		return CBM_ERROR_FILE_NOT_FOUND;
		break;
	}

	// ok, we have ensured we have an x00 file name
	// now make sure it actually is an x00 file

	// read x00 header
	uint8_t x00_buf[0x1a];
	int flg;

	// seek to start of file
	infile->handler->seek(infile, 0);
	// read p00 header
	infile->handler->readfile(infile, (char*)x00_buf, 0x1a, &flg);

	if (strcmp("C64File", (char*)x00_buf) != 0) { 
		return CBM_ERROR_FILE_NOT_FOUND;
	}

	// ok, we found a real x00 file
	log_info("Found %c00 file '%s' addressed as '%s'\n", typechar, x00_buf+8, name);

	// check with the open options

	// we don't care if write, overwrite, or read etc,
	// so there is no need to check for type

	// check opts parameters
	uint8_t open_ftype;
	uint16_t open_reclen;
	openpars_process_options((uint8_t*) opts, &open_ftype, &open_reclen);
	
	if (open_ftype != 0 && open_ftype != ftype) {
		log_debug("Expected file type %d, found file type %d\n", open_ftype, ftype);
		return CBM_ERROR_FILE_TYPE_MISMATCH;
	}

	if (ftype == FS_DIR_TYPE_REL && open_reclen != x00_buf[0x19]) {
		return CBM_ERROR_RECORD_NOT_PRESENT;
	}

	// done, alloc x00_file and prepare for operation
	// no seek necessary, read pointer is already at start of payload

	x00_file_t *file = mem_alloc(&x00_file_type);

	file->file.handler = &x00_handler;
	file->file.parent = infile;

	file->recordlen = x00_buf[0x19];
	file->filetype = ftype;

	*outfile = (file_t*)file;

	return CBM_ERROR_OK;
}

static void x00_close(file_t *file) {
	x00_file_t *xfile = (x00_file_t*)file;

	// no resources to clean here, so just forward the close
	xfile->file.parent->handler->close(xfile->file.parent);

	// and then free the file struct memory
	mem_free(xfile);
}

static uint16_t x00_recordlen(file_t *file) {
	x00_file_t *xfile = (x00_file_t*)file;

	return xfile->recordlen;
}

static uint8_t x00_filetype(file_t *file) {
	x00_file_t *xfile = (x00_file_t*)file;

	return xfile->filetype;
}

static int x00_seek(file_t *file, long pos) {

	// add header offset, that's all
	return file->parent->handler->seek(file->parent, pos + 0x1a );
}

static int x00_read(file_t *file, char *buf, int len, int *readflg) {

	return file->parent->handler->readfile(file->parent, buf, len, readflg );
}

static int x00_write(file_t *file, char *buf, int len, int writeflg) {

	return file->parent->handler->writefile(file->parent, buf, len, writeflg );
}


static handler_t x00_handler = {
	"X00", 		//const char	*name;			// handler name, for debugging
	"ASCII",	//const char	*native_charset;	// get name of the native charset for that handler
	x00_resolve,	//int		(*resolve)(file_t *infile, file_t **outfile, 
			//		uint8_t type, const char *name, const char *opts); 

	x00_close, 	//void		(*close)(file_t *fp);	// close the file

	NULL,		//int		(*open)(file_t *fp); 	// open a file

	// -------------------------
			// get converter for DIR entries
	NULL,

	// -------------------------
	
	x00_seek,	// position the file
			//int		(*seek)(file_t *fp, long abs_position);

	x00_read,	// read file data
			//int		(*readfile)(file_t *fp, char *retbuf, int len, int *readflag);	

	x00_write,	// write file data
			//int		(*writefile)(file_t *fp, char *buf, int len, int is_eof);	

	// -------------------------

	x00_recordlen,	//uint16_t	(*recordlen)(file_t *fp);	// return the record length for file

	x00_filetype	//uint8_t	(*filetype)(file_t *fp);	// return the type of the file as FS_DIR_TYPE_*

};


