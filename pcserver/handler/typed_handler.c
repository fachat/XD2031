/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2016 Andre Fachat

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
 This handler tests if the underlying file system has a ",P" or ",Lxxx" etc
 file type appended. If so, it hides the extension and makes the file accessible
 with the correct file type.

 This should be especially useful for REL files on a non-Dxx-media
*/

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



#define	MAX_NAME_LEN	17	/* 16 chars plus zero-terminator */

static handler_t typed_handler;

void typed_handler_init(void) {
	handler_register(&typed_handler);
}

typedef struct {
	file_t		file;		// embedded
} typed_file_t;

typedef struct {
        direntry_t      de;             // embedded
        direntry_t      *parent_de;
} typed_dirent_t;

static type_t typed_dirent_type = {
        "typed_dirent",
        sizeof(typed_dirent_t),
        NULL
};

static void typed_file_init(const type_t *t, void *p) {
	(void) t; // silence unused warning

	typed_file_t *x = (typed_file_t*) p;

	x->file.type = FS_DIR_TYPE_UNKNOWN;
}

static type_t typed_file_type = {
	"typed_file",
	sizeof(typed_file_t),
	typed_file_init
};

/*
 * identify whether a given file is a typed file type
 *
 * returns CBM_ERROR_OK even if no match found,
 * except in case of an error
 *
 * name is the current file name
 */
static int typed_wrap(direntry_t *dirent, direntry_t **outde) {

	log_debug("typed_resolve: infile=%s\n", dirent->name);

	// check the file name of the given file_t, if it actually is a ,P file.

	// must be at least one character, plus "," plus "P" ending
	if (dirent->name == NULL || strlen((char*)dirent->name) < 3) {
		// not found, but no error
		return CBM_ERROR_OK;
	}

        if (dirent->mode != FS_DIR_MOD_FIL) {
                // wrong de type
                return CBM_ERROR_OK;
        }

	char *name = conv_name_alloc((char*)dirent->name, dirent->cset, CHARSET_ASCII);

	//log_debug("typed_resolve: infile converted to=%s\n", name);

	char typechar = 0;
	uint8_t ftype = 0;
	char *p = strrchr(name, ',');
	int recordlen = 0;

	if (p == NULL) {
		mem_free((char*)name);
		return CBM_ERROR_OK;
	}
	// cut off type
	*p = 0;

	// which type is it?
	p++;
	typechar = *p;

	switch(typechar) {
	case 'P':
	case 'P'+128:
	case 'p':
		ftype = FS_DIR_TYPE_PRG;
		break;
	case 'S':
	case 'S'+128:
	case 's':
		ftype = FS_DIR_TYPE_SEQ;
		break;
	case 'U':
	case 'U'+128:
	case 'u':
		ftype = FS_DIR_TYPE_USR;
		break;
	case 'R':
	case 'R'+128:
	case 'r':
	case 'L':
	case 'L'+128:
	case 'l':
		ftype = FS_DIR_TYPE_REL;
		break;
	default:
		// anything else
		mem_free((char*)name);
		return CBM_ERROR_OK;
		break;
	}

	p++;
	if (ftype != FS_DIR_TYPE_REL) {
		if (*p) {
			// does not end with the type
			mem_free((char*)name);
			return CBM_ERROR_OK;
		}
	} else {
		// REL file record size
		if (*p) {
			int n = sscanf(p, "%d", &recordlen);
			if (n < 1) {
				mem_free((char*)name);
				return CBM_ERROR_OK;
			}
		}
	}

	// not needed anymore
	mem_free((char*)name);
	name = NULL;
			
	// ok, we found a real typed file
	log_info("Found %c typed file '%s' addressed as '%s'\n", typechar, name, dirent->name);

	// check with the open options

	// TODO: still true?
	// we don't care if write, overwrite, or read etc,
	// so there is no need to check for type

        typed_dirent_t *de = mem_alloc(&typed_dirent_type);

        de->de.parent = NULL;
        de->de.handler = &typed_handler;

        de->de.size = dirent->size;
        de->de.moddate = dirent->moddate;
        de->de.recordlen = recordlen;
        de->de.mode = dirent->mode;
        de->de.attr = dirent->attr;
        de->de.type = ftype;
        de->de.cset = dirent->cset;

        de->de.name = (uint8_t*) mem_alloc_str((char*)dirent->name);
	p = strrchr((char*)de->de.name, ',');
	*p = 0;

        de->parent_de = dirent;

        *outde = (direntry_t*)de;

	return CBM_ERROR_OK;
}

static int typed_declose(direntry_t *dirent) {

	typed_dirent_t *de = (typed_dirent_t*) dirent;

	// close parent
	de->parent_de->handler->declose(de->parent_de);

	// close our own
	mem_free(de->de.name);
        mem_free(de);

	return CBM_ERROR_OK;
}

static int typed_open2(direntry_t *de, openpars_t *pars, int opentype, file_t **outfp) {

        typed_dirent_t *dirent = (typed_dirent_t*) de;

	// just open the inner file and return its file_t, as we only did a name conversion
	if (de->type == FS_DIR_TYPE_REL) {
		if (pars->filetype != FS_DIR_TYPE_UNKNOWN 
			&& pars->filetype != FS_DIR_TYPE_REL) {
			return CBM_ERROR_FILE_TYPE_MISMATCH;
		}
		pars->filetype = FS_DIR_TYPE_REL;

		if (pars->recordlen > 0 && pars->recordlen != de->recordlen) {
			// recordlen mismatch
			return CBM_ERROR_RECORD_NOT_PRESENT;
		}
		pars->recordlen = de->recordlen;
	}
	
        file_t *infile = NULL;
        int rv = dirent->parent_de->handler->open2(dirent->parent_de, pars, opentype, &infile);

        if (rv != CBM_ERROR_OK) {
                return rv;
        }

        *outfp = (file_t*)infile;

        return CBM_ERROR_OK;
}

static int typed_scratch2(direntry_t *dirent) {

	typed_dirent_t *de = (typed_dirent_t*) dirent;

	return de->parent_de->handler->scratch2(de->parent_de);
}


/*
 * identify whether a given file is a typed file type
 *
 * returns CBM_ERROR_OK even if no match found,
 * except in case of an error
 *
 * name is the current file name
 */
static int typed_resolve(file_t *infile, file_t **outfile, const char *inname, charset_t cset, const char **outname) {

	log_debug("typed_resolve: infile=%s\n", infile->filename);

	// check the file name of the given file_t, if it actually is a ,P file.

	// must be at least one character, plus "," plus "P" ending
	if (infile->filename == NULL || strlen(infile->filename) < 3) {
		// not found, but no error
		return CBM_ERROR_OK;
	}

	//char *name = conv_name_alloc(infile->filename, cset, CHARSET_ASCII);
	char *name = mem_alloc_str(infile->filename);

	//log_debug("typed_resolve: infile converted to=%s\n", name);

	char typechar = 0;
	uint8_t ftype = 0;
	char *p = strrchr(name, ',');
	int recordlen = 0;

	if (p == NULL) {
		mem_free((char*)name);
		return CBM_ERROR_OK;
	}
	// cut off type
	*p = 0;

	// which type is it?
	p++;
	typechar = *p;

	switch(typechar) {
	case 'P':
	case 'P'+128:
	case 'p':
		ftype = FS_DIR_TYPE_PRG;
		break;
	case 'S':
	case 'S'+128:
	case 's':
		ftype = FS_DIR_TYPE_SEQ;
		break;
	case 'U':
	case 'U'+128:
	case 'u':
		ftype = FS_DIR_TYPE_USR;
		break;
	case 'R':
	case 'R'+128:
	case 'r':
	case 'L':
	case 'L'+128:
	case 'l':
		ftype = FS_DIR_TYPE_REL;
		break;
	default:
		mem_free((char*)name);
		return CBM_ERROR_OK;
		break;
	}

	p++;
	if (ftype != FS_DIR_TYPE_REL) {
		if (*p) {
			// does not end with the type
			mem_free((char*)name);
			return CBM_ERROR_OK;
		}
	} else {
		// REL file record size
		if (*p) {
			int n = sscanf(p, "%d", &recordlen);
			if (n < 1) {
				mem_free((char*)name);
				return CBM_ERROR_OK;
			}
		}
	}
			
	// ok, we found a real typed file
	log_info("Found %c typed file '%s' addressed as '%s'\n", typechar, name, infile->filename);

	// check with the open options

	// TODO: still true?
	// we don't care if write, overwrite, or read etc,
	// so there is no need to check for type

	// now compare the original file name with the search pattern
	if (!compare_dirpattern(name, inname, outname)) {
		return CBM_ERROR_FILE_NOT_FOUND;
	}

	// done, alloc typed_file and prepare for operation
	// no seek necessary, read pointer is already at start of payload

	typed_file_t *file = mem_alloc(&typed_file_type);

	file->file.isdir = 0;
	file->file.handler = &typed_handler;
	file->file.parent = infile;

	file->file.filename = name;

	file->file.recordlen = recordlen;
	file->file.type = ftype;

	file->file.attr = infile->attr;
	file->file.filesize = infile->filesize;

	*outfile = (file_t*)file;

	return CBM_ERROR_OK;
}


static void typed_dump(file_t *file, int recurse, int indent) {

	const char *prefix = dump_indent(indent);

	log_debug("%s// typed file\n", prefix);
	log_debug("%sparent={\n", prefix);
	if (file->parent != NULL && file->parent->handler->dump != NULL) {
		file->parent->handler->dump(file->parent, recurse, indent+1);
	}
	log_debug("%s}\n", prefix);
}

static int typed_equals(file_t *thisfile, file_t *otherfile) {

        if (otherfile->handler != &typed_handler) {
                return 1;
        }

        return thisfile->parent->handler->equals(thisfile->parent, otherfile->parent);

}



static handler_t typed_handler = {
	"typed", 	//const char	*name;			// handler name, for debugging

	NULL,		// resolve2

	typed_resolve,	//int		(*resolve)(file_t *infile, file_t **outfile, 
			//		uint8_t type, const char *name, const char *opts); 
	typed_wrap,		// wrap

	default_close, 	//void		(*close)(file_t *fp, int recurse);	// close the file

	NULL,		// fclose
	typed_declose,	// declose

	default_open,	//int		(*open)(file_t *fp); 	// open a file

	typed_open2,	//int		(*open2)(direntry_t *fp); 	// open a file

	// -------------------------
			// get converter for DIR entries
//	NULL,

	NULL,	// file_t* parent(file_t*)

	// -------------------------
	
	NULL,	// position the file
			//int		(*seek)(file_t *fp, long abs_position);

	NULL,	// read file data
			//int		(*readfile)(file_t *fp, char *retbuf, int len, int *readflag);	

	NULL,	// write file data
			//int		(*writefile)(file_t *fp, char *buf, int len, int is_eof);	

	NULL,	// truncate	(file_t *fp, long size);

	// -------------------------

	NULL,		// int direntry2(file_t *fp, file_t **outentry);

	NULL,		// int direntry(file_t *fp, file_t **outentry);

	NULL,		// int create(file_t *fp, file_t **outentry, cont char *name, uint8_t filetype, uint8_t reclen);

	// -------------------------

	NULL,

	typed_equals,

	NULL,		// realsize
	NULL,		// realsize2

	default_scratch,

	typed_scratch2,	// delete2

	NULL,		// mkdir not supported

	NULL,		// rmdir not supported

	NULL,		// rmdir2 not supported

	NULL,		// move not supported

	NULL,		// move2 not supported

	// -------------------------

	typed_dump
};


