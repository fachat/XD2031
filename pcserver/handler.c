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

#include "registry.h"
#include "handler.h"
#include "errors.h"
#include "log.h"
#include "mem.h"
#include "provider.h"
#include "wireformat.h"

// This file defines the file type handler interface. This is used
// to "wrap" files so that for example x00 (like P00, R00, ...) files 
// or compressed files can be handled.
//
// In the long run the di_provider will become a handler as well - so
// you can CD into a D64 file stored in a D81 image read from an FTP 
// server...

static registry_t handlers = { 0 };

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


file_t* handler_find(file_t *parent, uint8_t type, const char *name, const char *opts) {

	file_t *newfile = NULL;

	for (int i = 0; ; i++) {
		handler_t *handler = reg_get(&handlers, i);
		if (handler == NULL) {
			// not found
			return NULL;
		}
		int err = handler->resolve(parent, &newfile, type, name, opts);
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
	endpoint_t	*endpoint;
	int		channel;	// for the endpoint
	uint16_t	recordlen;
} ep_file_t;

static type_t ep_file_type = {
	"ep_file",
	sizeof(ep_file_t)
};


/*
 * open a file
 */

int handler_resolve_file(endpoint_t *ep, int chan, file_t **outfile, uint8_t type, const char *name, const char *opts) {

	int reclen = 0;
	int err = CBM_ERROR_FAULT;

	// assuming infile is NULL

	switch(type) {
		case FS_OPEN_RD:
			err = ep->ptype->open_rd(ep, chan, name, opts, &reclen);
			break;
		case FS_OPEN_WR:
			err = ep->ptype->open_wr(ep, chan, name, opts, &reclen, 0);
			break;
		case FS_OPEN_AP:
			err = ep->ptype->open_ap(ep, chan, name, opts, &reclen);
			break;
		case FS_OPEN_OW:
			err = ep->ptype->open_wr(ep, chan, name, opts, &reclen, 1);
			break;
		case FS_OPEN_RW:
			err = ep->ptype->open_rw(ep, chan, name, opts, &reclen);
			break;
		case FS_OPEN_DR:
			err = ep->ptype->opendir(ep, chan, name, opts);
			break;
	}

	if (err != CBM_ERROR_OK) {
		return err;
	}

	ep_file_t *file = mem_alloc(&ep_file_type);

	file->file.handler = &ep_handler;
	file->file.parent = NULL;

	file->endpoint = ep;
	file->channel = chan;
	file->recordlen = reclen;

	*outfile = (file_t*) file;

	return CBM_ERROR_OK;
}

/*
 * resolve a file_t from an endpoint, for a block operation
 */
int handler_resolve_block(endpoint_t *ep, int chan, file_t **outfile) {

	ep_file_t *file = mem_alloc(&ep_file_type);

	file->file.handler = &ep_handler;
	file->file.parent = NULL;

	file->endpoint = ep;
	file->channel = chan;
	file->recordlen = 0;

	*outfile = (file_t*) file;

	return CBM_ERROR_OK;
}




static void ep_close(file_t *file) {
        ep_file_t *xfile = (ep_file_t*)file;

        // no resources to clean here, so just forward the close
        xfile->endpoint->ptype->close(xfile->endpoint, xfile->channel);

	// cleanup provider when not needed anymore
	provider_cleanup(xfile->endpoint);

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
        return xfile->endpoint->ptype->position(xfile->endpoint, xfile->channel, pos / xfile->recordlen);
}

static int ep_read(file_t *file, char *buf, int len, int *readflg) {
        ep_file_t *xfile = (ep_file_t*)file;

        return xfile->endpoint->ptype->readfile(xfile->endpoint, xfile->channel, buf, len, readflg );
}

static int ep_write(file_t *file, char *buf, int len, int writeflg) {
        ep_file_t *xfile = (ep_file_t*)file;

        return xfile->endpoint->ptype->writefile(xfile->endpoint, xfile->channel, buf, len, writeflg );
}

static charconv_t ep_convfrom(file_t *file, const char *tocharset) {
        ep_file_t *xfile = (ep_file_t*)file;

	// TODO kludge
	return provider_convfrom(xfile->endpoint->ptype);
}



static handler_t ep_handler = {
        "EPH",          //const char    *name;                  // handler name, for debugging
        "ASCII",        //const char    *native_charset;        // get name of the native charset for that handler
        NULL,	    	// root handler has no resolve
			//int           (*resolve)(file_t *infile, file_t **outfile, 
                        //              uint8_t type, const char *name, const char *opts); 

        ep_close,      //void          (*close)(file_t *fp);   // close the file

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

        // -------------------------

        ep_recordlen,  //uint16_t      (*recordlen)(file_t *fp);       // return the record length for file

        ep_filetype    //uint8_t       (*filetype)(file_t *fp);        // return the type of the file as FS_DIR_TYPE_*

};


