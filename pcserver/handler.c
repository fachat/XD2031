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


#include <sys/types.h>
#include <string.h>

#include "registry.h"
#include "handler.h"
#include "errors.h"
#include "log.h"
#include "mem.h"
#include "provider.h"
#include "wireformat.h"
#include "wildcard.h"

// prototypes
static int ep_create(endpoint_t *ep, int chan, file_t **outfile,
                                        const char *name, const char **outname);


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

// unused as of now
file_t* handler_find(file_t *parent, uint8_t type, const char *name, const char *opts, const char **outname) {

	file_t *newfile = NULL;

	for (int i = 0; ; i++) {
		handler_t *handler = reg_get(&handlers, i);
		if (handler == NULL) {
			// not found
			return NULL;
		}
		int err = handler->resolve(parent, &newfile, type, name, opts, outname);
		if (err != CBM_ERROR_OK) {
			log_error("Got %d as error\n", err);
			return NULL;
		}
	}

	return newfile;
}


//----------------------------------------------------------------------------
// handler that wraps the operations to an endpoint

static handler_t ep_handler;

typedef struct {
	file_t 		file;		// embedded
	int		epchan;		// channel for endpoint (-1 is not opened yet)
	uint16_t	recordlen;
} ep_file_t;

static type_t ep_file_type = {
	"ep_file",
	sizeof(ep_file_t),
	NULL
};


/*
 * open a file
 *
 * This function takes the file name from an OPEN, and the endpoint defined for the 
 * drive as given in the OPEN, and recursively walks the path parts of that file name.
 *
 * Each path part is checked against the list of handlers for a match
 */

int handler_resolve_file(endpoint_t *ep, int chan, file_t **outfile, uint8_t type, const char *inname, const char *opts) {

	const char *outname = NULL;
	int err = CBM_ERROR_FAULT;
	file_t *file = NULL;
	file_t *direntry = NULL;

	// initial file struct, corresponds to root directory of endpoint
	//err = ep_create(ep, chan, outfile, inname, &outname);
	ep->ptype->root(ep);

	if (err != CBM_ERROR_OK) {
		return err;
	}
	const char *name = outname;

	// as long as we have filename parts left
	while (outfile != NULL && name != NULL && *name != 0) {

		file = *outfile;

		// loop over directory entries
		while ((direntry = file->handler->direntry(file)) != NULL) {

			// test each dir entry against the different handlers
			// handlers implement checks e.g. for P00 files and wrap them
			// in another file_t level
			// the handler->resolve() method also matches the name, as the
			// P00 files may contain their own "real" name within them
			*outfile = NULL;
			outname = strchr(name, '/');	// default end of name (if no handler matches)

			for (int i = 0; ; i++) {
				handler_t *handler = reg_get(&handlers, i);
				if (handler == NULL) {
					// no handler found
					break;
				}
				err = handler->resolve(direntry, outfile, type, name, opts, &outname);
				if (err != CBM_ERROR_OK) {
					log_error("Got %d as error from handler %s for %s\n", 
						err, handler->name, name);
				} else {
					// found a handler
					break;
				}
			}
		
			// replace original dir entry with wrapped one	
			if (*outfile != NULL) {
				file = *outfile;
				// do not check any further dir entries
				break;
			}
			// now check if the original filename matches the pattern
			if (compare_dirpattern(direntry->handler->getname(direntry), name, &outname)) {
				// matches, so go on with it
				file = direntry;
				// do not check any further dir entries
				break;
			}
		}

		// found our directory entry and wrapped it in file
		// now check if we have/need a container wrapper (with e.g. a ZIP file in a P00 file)
		if (outname == NULL || *outname == 0) {
			// no more pattern left, so outfile should be what was required
			// i.e. we have raw access to container files like D64 or ZIP
			// (which is similar to the "$" file on non-load secondary addresses)
			*outfile = file;
			return CBM_ERROR_OK;
		}

		// check with the container providers (i.e. endpoint providers)
		// whether to wrap it 
		*outfile = provider_wrap(file);
	}

	return err;
}
	
//	int reclen = 0;
//	int err = CBM_ERROR_FAULT;
//
//	// assuming infile is NULL
//
//	switch(type) {
//		case FS_OPEN_RD:
//			err = ep->ptype->open_rd(ep, chan, name, opts, &reclen);
//			break;
//		case FS_OPEN_WR:
//			err = ep->ptype->open_wr(ep, chan, name, opts, &reclen, 0);
//			break;
//		case FS_OPEN_AP:
//			err = ep->ptype->open_ap(ep, chan, name, opts, &reclen);
//			break;
//		case FS_OPEN_OW:
//			err = ep->ptype->open_wr(ep, chan, name, opts, &reclen, 1);
//			break;
//		case FS_OPEN_RW:
//			err = ep->ptype->open_rw(ep, chan, name, opts, &reclen);
//			break;
//		case FS_OPEN_DR:
//			err = ep->ptype->opendir(ep, chan, name, opts);
//			break;
//	}
//
//	if (err != CBM_ERROR_OK) {
//		return err;
//	}
//
//	ep_file_t *file = mem_alloc(&ep_file_type);
//
//	file->file.handler = &ep_handler;
//	file->file.parent = NULL;
//
//	file->endpoint = ep;
//	file->channel = chan;
//	file->recordlen = reclen;
//
//
//	// now wrap into handler for file types
//	file_t *oldfile = (file_t*) file;
//	file_t *newfile = NULL;
//	int i = 0;
//	err = CBM_ERROR_FILE_NOT_FOUND;
//	do {
//		handler_t *handler = reg_get(&handlers, i);
//		if (handler == NULL) {
//			break;
//		}
//		err = handler->resolve(oldfile, &newfile, type, name, opts);
//		i++;
//	} while (err == CBM_ERROR_FILE_NOT_FOUND);
//
//log_info("resolved file to %p, with err=%d\n", newfile, err);
//
//	if (err == CBM_ERROR_OK) {
//		*outfile = newfile;
//	} else
//	if (err == CBM_ERROR_FILE_NOT_FOUND) {
//		// no handler found, stay with original
//		*outfile = oldfile;
//		err = CBM_ERROR_OK;
//	} else {
//		// file found, but serious error, so close and be done
//		oldfile->handler->close(oldfile);
//	}
//
//	// TODO: here would be the check if the current file would be a container
//	// (directory) and if the file name contained more parts to recursively 
//	// traverse
//
//	return err;
//}

/*
 * resolve a file_t from an endpoint, for a block operation
 */
int handler_resolve_block(endpoint_t *ep, int chan, file_t **outfile) {

	ep_file_t *file = mem_alloc(&ep_file_type);

	file->file.handler = &ep_handler;
	file->file.parent = NULL;

	file->file.endpoint = ep;
	//file->channel = chan;
	file->recordlen = 0;

	*outfile = (file_t*) file;

	return CBM_ERROR_OK;
}


