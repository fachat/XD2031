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


