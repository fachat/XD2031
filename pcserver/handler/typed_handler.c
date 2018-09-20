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
	typed_wrap,		// wrap
	NULL,		// fclose
	typed_declose,	// declose
	typed_open2,	//int		(*open2)(direntry_t *fp); 	// open a file
	NULL,		// file_t* parent(file_t*)
	NULL,		// seek position the file
	NULL,		// readfile data
	NULL,		// writefile data
	NULL,		// truncate	(file_t *fp, long size);
	NULL,		// int direntry2(file_t *fp, file_t **outentry);
	NULL,		// int create(file_t *fp, file_t **outentry, cont char *name, uint8_t filetype, uint8_t reclen);
	NULL,		// flush
	typed_equals,
	NULL,		// realsize2
	typed_scratch2,	// delete2
	NULL,		// mkdir not supported
	NULL,		// rmdir2 not supported
	NULL,		// move2 not supported

	// -------------------------

	typed_dump
};


