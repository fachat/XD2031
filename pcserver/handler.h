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
 * resolve a file_t from an endpoint, for a block operation
 */
int handler_resolve_block(endpoint_t *ep, int chan, file_t **outfile);


/*
 * wrap a file_t (e.g. with x00* handler)
 */
int handler_wrap(file_t *infile, uint8_t type, const char *name,
                const char **outname, file_t **outfile);

/*
 * not really nice, but here's the list of existing handlers (before we do an own
 * header file for each one separately...
 */


// handles P00, S00, ... files
void x00_handler_init();

#endif

