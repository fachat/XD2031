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

struct _handler {
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
							// The outname contains the start of the next 
							// path part, or NULL if the file found was
							// the last part of the given name. outname
							// can directly be given into the next 
							// (child) resolve method (i.e. path separators
							// are filtered out). 
	int		(*resolve)(file_t *infile, file_t **outfile, 
				uint8_t type, const char *name, const char *opts, const char **outname); 

							// close the file; do so recursively by closing
							// parents if recurse is set
	void		(*close)(file_t *fp, int recurse);	

	int		(*open)(file_t *fp); 		// open a file

	// -------------------------
							// get the converter FROM the file
	charconv_t 	(*convfrom)(file_t *prov, const char *tocharset);

	// -------------------------
							// position the file
	int		(*seek)(file_t *fp, long position, int flag);

							// read file data
	int		(*readfile)(file_t *fp, char *retbuf, int len, int *readflag);	

							// write file data
	int		(*writefile)(file_t *fp, char *buf, int len, int is_eof);	

							// truncate to a given size
	int		(*truncate)(file_t *fp, long size);	

	// -------------------------

	int		(*direntry)(file_t *dirfp, file_t **outentry, int *readflag);
							// create a new file in the directory
	int		(*create)(file_t *dirfp, file_t **outentry, const char *name, uint8_t filetype, 
				uint16_t recordlen);
};

// values to be set in the out parameter readflag for readfile()
#define	READFLAG_EOF		1
#define	READFLAG_DENTRY		2

// values to be set in the seek() flag parameter
#define	SEEKFLAG_ABS		0		/* count from the start */
#define	SEEKFLAG_END		1		/* count from the end of the file */

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
file_t *handler_find(file_t *parent, uint8_t type, const char *name, const char *opts, const char **outname);

/*
 * recursively resolve a file from an endpoint using the given inname as path
 */
int handler_resolve_file(endpoint_t *ep, file_t **outfile,
                const char *inname, const char *opts, uint8_t type);

/*
 * recursively resolve a dir from an endpoint using the given inname as path
 */
int handler_resolve_dir(endpoint_t *ep, file_t **outdir,
                const char *inname, const char *opts);

/*
 * get next directory entry
 */
int handler_direntry(file_t *dir, file_t **direntry, int *readflag);

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

