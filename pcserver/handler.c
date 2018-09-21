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

#include "os.h"

#include <string.h>

#include "registry.h"
#include "provider.h"
#include "handler.h"
#include "errors.h"
#include "log.h"
#include "mem.h"
#include "wireformat.h"
#include "wildcard.h"
#include "openpars.h"



// This file defines the file type handler interface. This is used
// to "wrap" files so that for example x00 (like P00, R00, ...) files 
// or compressed files can be handled.
//
// In the long run the di_provider will become a handler as well - so
// you can CD into a D64 file stored in a D81 image read from an FTP 
// server...

static registry_t handlers;

/*
 * register a new provider, usually called at startup
 */
void handler_register(handler_t *handler) {
	
	reg_append(&handlers, handler);
}

/*
 * initialize the provider registry
 */
void handler_init(void) {

	reg_init(&handlers, "handlers", 10);
}

/*
 * clean up
 */
void handler_free(void) {

	reg_free(&handlers, NULL);
}


void path_append(char **path, const char *filename) {
	// construct path
	if (strcmp(".", filename) != 0) {
		// not the "same directory"
		if (strcmp("..", filename) == 0) {
			// "up"
			char *p = strrchr(*path, dir_separator_char());
			if (p != NULL) {
				// not on root yet - shorten path
				p[0] = 0;
			}
		} else {
			// mem_append_str2 re-allocs the path
			mem_append_str2(path, dir_separator_string(), filename);
		}
	}
	log_debug("path_append(%s) -> %s\n", filename, *path);
}

//----------------------------------------------------------------------------


int handler_wrap(direntry_t *dirent, direntry_t **outde) {

	log_debug("handler_wrap(infile=%s)\n", dirent->name);

	int err = CBM_ERROR_OK;
	*outde = NULL;

	for (int i = 0; ; i++) {
		handler_t *handler = reg_get(&handlers, i);
		if (handler == NULL) {
			// no handler found
			break;
		}
		// outpattern then points into the pattern string
		if ( handler->wrap && handler->wrap(dirent, outde) == CBM_ERROR_OK) {
			// worked ok.
			if (*outde != NULL) {
				// found a handler
				err = CBM_ERROR_OK;
				break;
			}
		} else {
			log_error("Got %d as error from handler %s\n", 
				err, handler->name);
			return err;
		}
	}

	return err;
}

file_t *handler_parent(file_t *file) {
	return file->parent;
}

// --------------------------------------------------------------------------------------------
// default implementations for handler

int default_fclose(file_t *file, char *outbuf, int *outlen) {

	if (file->filename) {
		mem_free(file->filename);
		file->filename = NULL;
	}

	// we are a resolve wrapper, so close the inner file as well
	int err = file->parent->handler->fclose(file->parent, outbuf, outlen);

	// and then free the file struct memory
	mem_free(file);

	return err;
}

#if 0
int default_close(file_t *file, int recurse, char *outbuf, int *outlen) {

	if (file->filename) {
		mem_free(file->filename);
		file->filename = NULL;
	}

	// we are a resolve wrapper, so close the inner file as well
	int err = file->parent->handler->close(file->parent, recurse, outbuf, outlen);

	// and then free the file struct memory
	mem_free(file);

	return err;
}
#endif

int default_seek(file_t *file, long pos, int flag) {

	return file->parent->handler->seek(file->parent, pos, flag );
}

int default_truncate(file_t *file, long pos) {

	return file->parent->handler->truncate(file->parent, pos);
}

int default_read(file_t *file, char *buf, int len, int *readflg, charset_t outcset) {

	return file->parent->handler->readfile(file->parent, buf, len, readflg, outcset );
}

int default_write(file_t *file, const char *buf, int len, int writeflg) {

	return file->parent->handler->writefile(file->parent, buf, len, writeflg );
}

#if 0
int default_open(file_t *file, openpars_t *pars, int opentype) {

        if (pars->filetype != FS_DIR_TYPE_UNKNOWN && pars->filetype != file->type) {
                log_debug("Expected file type %d, found file type %d\n", pars->filetype, file->type);
                return CBM_ERROR_FILE_TYPE_MISMATCH;
        }

        if (file->type == FS_DIR_TYPE_REL && (pars->recordlen != 0 && pars->recordlen != file->recordlen)) {
                return CBM_ERROR_RECORD_NOT_PRESENT;
        }

        openpars_t wrappedpars;
        wrappedpars.filetype = FS_DIR_TYPE_UNKNOWN;
        wrappedpars.recordlen = 0;

	cbm_errno_t rv = file->parent->handler->open(file->parent, &wrappedpars, opentype);
	if (rv == CBM_ERROR_OK) {
		rv = file->handler->seek(file, 0, SEEKFLAG_ABS);
	}
	return rv;
}
#endif

#if 0
int default_scratch(file_t *file) {

	cbm_errno_t rv = file->parent->handler->scratch(file->parent);

	if (rv == CBM_ERROR_OK) {	
		// parent file is closed
		mem_free(file);
	}

	return rv;
}
#endif

#if 1
file_t* default_parent(file_t *file) {
	if (file->parent != NULL) {
		return file->parent->handler->parent(file->parent);
	}
	return NULL;
}
#endif

int default_flush(file_t *file) {
	if (file->parent != NULL) {
		return file->parent->handler->flush(file->parent);
	}
	return CBM_ERROR_FAULT;
}

#if 0
size_t default_realsize(file_t *file) {

	if (file->parent != NULL) {
		return file->parent->handler->realsize(file->parent);
	}
	return CBM_ERROR_FAULT;
}
#endif


