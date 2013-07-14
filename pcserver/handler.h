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

#ifndef HANDLER_H
#define HANDLER_H

#include <inttypes.h>

#include "provider.h"

// This file defines the file type handler interface. This is used
// to "wrap" files so that for example x00 (like P00, R00, ...) files 
// or compressed files can be handled.
//
// In the long run the di_provider will become a handler as well - so
// you can CD into a D64 file stored in a D81 image read from an FTP 
// server...

typedef struct _file file_t;

typedef struct {
	const char	*name;				// handler name, for debugging
	const char	*native_charset;		// get name of the native charset for that handler

							// used internally by a recursive resolve
							// starting from an endpoint and continuing until
							// all name parts are used
							// Actually called by the handler registry in turn
							// until one handler returns non-null
							// type is the FS_OPEN_* command as parameter
							// may return non-null but with error (e.g.
							// write open on read-only endpoint). Error can
							// be read on file_t
	int		(*resolve)(const file_t *infile, file_t **outfile, 
					uint8_t type, const char *name, const char *opts); 

	void		(*close)(file_t *fp);		// close the file

	int		(*open)(file_t *fp); 		// open a file

	// -------------------------
							// get the converter FROM the file
	charconv_t 	(*convfrom)(file_t *prov, const char *tocharset);

	// -------------------------
							// position the file
	int		(*seek)(file_t *fp, long abs_position);

							// read file data
	int		(*readfile)(file_t *fp, char *retbuf, int len, int *readflag);	

							// write file data
	int		(*writefile)(file_t *fp, char *buf, int len, int is_eof);	

	// -------------------------

	uint16_t	(*recordlen)(file_t *fp);	// return the record length for file

	uint8_t		(*filetype)(file_t *fp);	// return the type of the file as FS_DIR_TYPE_*

} handler_t;

// values to be set in the out parameter readflag for readfile()
#define	READFLAG_EOF	1
#define	READFLAG_DENTRY	2

struct _file {
	handler_t	*handler;
	file_t		*parent;	// for resolving ".."
};

/*
 * register a new provider, usually called at startup
 */
void handler_register(handler_t *handler);

/*
 * initialize the provider registry
 */
void handler_init(void);

/*
 * find a file
 */
file_t *handler_find(file_t *parent, uint8_t type, const char *name, const char *opts);

/*
 * resolve a file_t from an endpoint, i.e. being a root file_t for further resolves into
 * subdirs
 */
int handler_resolve_file(endpoint_t *ep, int chan, file_t **outfile, uint8_t type, const char *name, const char *opts);


/*
 * resolve a file_t from an endpoint, for a block operation
 */
int handler_resolve_block(endpoint_t *ep, int chan, file_t **outfile);

/*
 * not really nice, but here's the list of existing handlers (before we do an own
 * header file for each one separately...
 */


// handles P00, S00, ... files
void x00_handler_init();

#endif

