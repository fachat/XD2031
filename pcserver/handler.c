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
	err = ep_create(ep, chan, outfile, inname, &outname);

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


// -----------------------------------------------------------------------------------------

// TODO: belongs into endpoint
static int ep_create(endpoint_t *ep, int chan, file_t **outfile,
                                        const char *name, const char **outname) {

	const char *dirname = "/";

	*outname = NULL;

	int err = ep->ptype->opendir(ep, chan, dirname, NULL);

	if (err == CBM_ERROR_OK) {
		
		ep_file_t *file = mem_alloc(&ep_file_type);

		file->file.endpoint = ep;
		file->file.parent = NULL;
		file->file.handler = &ep_handler;
		//file->isdir = 1;
		//file->channel = chan;

		// remove root directory indicator
		char *p = name;
		while (*p == '/') {
			p++;
		}
		if (*p) {
			*outname = p;
		}

	}

	return err;	
}

// used internally by a recursive resolve
// starting from an endpoint and continuing until
// all name parts are used
// Actually called by the handler registry in turn
// until one handler returns non-null
// type is the FS_OPEN_* command as parameter
// may return non-null but with error (e.g.
// write open on read-only endpoint). Error can
// be read on file_t
// The outname contains the start of the next 
// path part, or NULL if the file found was
// the last part of the given name. outname
// can directly be given into the next 
// (child) resolve method (i.e. path separators
// are filtered out). 
//
// The endpoint-file resolve() method opens the top level path (actually "." or "/") as
// directory, so that lower paths can be examined by the resolver
// 
int ep_resolve(const file_t *infile, file_t **outfile,
                                        uint8_t type, const char *name, const char *opts, char **outname) {
}


static void ep_close(file_t *file, int recurse) {
	(void) recurse;

        ep_file_t *xfile = (ep_file_t*)file;

        // no resources to clean here, so just forward the close
        xfile->file.endpoint->ptype->close(xfile->file.endpoint, xfile->channel);

	// cleanup provider when not needed anymore
	provider_cleanup(xfile->file.endpoint);

        // and then free the file struct memory
        mem_free(xfile);
}

static uint16_t ep_recordlen(file_t *file) {
        ep_file_t *xfile = (ep_file_t*)file;

        return xfile->recordlen;
}

static uint8_t ep_filetype(file_t *file) {
	(void)file;	// silence unused param warning

        return FS_DIR_TYPE_PRG;		// default fallback
}

static int ep_seek(file_t *file, long pos) {
        ep_file_t *xfile = (ep_file_t*)file;

        // add header offset, that's all
	// TODO: kludge!
        return xfile->file.endpoint->ptype->position(xfile->file.endpoint, xfile->channel, 
			(pos == 0) ? pos : pos / xfile->recordlen);
}

static int ep_read(file_t *file, char *buf, int len, int *readflg) {
        ep_file_t *xfile = (ep_file_t*)file;

        return xfile->file.endpoint->ptype->readfile(xfile->file.endpoint, xfile->channel, buf, len, readflg );
}

static int ep_write(file_t *file, char *buf, int len, int writeflg) {
        ep_file_t *xfile = (ep_file_t*)file;

        return xfile->file.endpoint->ptype->writefile(xfile->file.endpoint, xfile->channel, buf, len, writeflg );
}

static charconv_t ep_convfrom(file_t *file, const char *tocharset) {
        ep_file_t *xfile = (ep_file_t*)file;

	// TODO kludge
	return provider_convfrom(xfile->file.endpoint->ptype);
}

// if the given file is a directory, return the next directory entry wrapped
// in a file_t. If no file is found, or not a directory, return NULL
static file_t* ep_direntry(file_t *fp) {

	if (fp->isdir == 0) {
		return NULL;
	}


}




static handler_t ep_handler = {
        "EPH",          //const char    *name;                  // handler name, for debugging
        "ASCII",        //const char    *native_charset;        // get name of the native charset for that handler
        ep_resolve,    	// root handler has no resolve
			//int           (*resolve)(file_t *infile, file_t **outfile, 
                        //              uint8_t type, const char *name, const char *opts, char *outname); 

        ep_close,      //void          (*close)(file_t *fp, int recurse);   // close the file

        NULL,           //int           (*open)(file_t *fp);    // open a file

        // -------------------------
			// get converter to convert DIR entries
	ep_convfrom,

        // -------------------------

        ep_seek,       // position the file
                        //int           (*seek)(file_t *fp, long abs_position);

        ep_read,       // read file data
                        //int           (*readfile)(file_t *fp, char *retbuf, int len, int *readflag);  

        ep_write,      // write file data
                        //int           (*writefile)(file_t *fp, char *buf, int len, int is_eof);       

	ep_direntry,	// file_t*         (*direntry)(file_t *fp);

        // -------------------------

        ep_recordlen,  //uint16_t      (*recordlen)(file_t *fp);       // return the record length for file

        ep_filetype    //uint8_t       (*filetype)(file_t *fp);        // return the type of the file as FS_DIR_TYPE_*

};


