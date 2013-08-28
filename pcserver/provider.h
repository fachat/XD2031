
/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2012 Andre Fachat

    Derived from:
    OS/A65 Version 1.3.12
    Multitasking Operating System for 6502 Computers
    Copyright (C) 1989-1997 Andre Fachat

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

#ifndef PROVIDER_H
#define PROVIDER_H

#include "charconvert.h"

//
// Endpoint providers communicate with the outside world with their 
// own protocol. Examples are local filesystem, or TCP/IP, or HTTP
// providers.
//
// Each provider can have multiple active endpoints. For example 
// different directories, or different hosts in case of internet
// access. Thus there is a provider definition, and a provider 
// state struct definition
//

#define MAX_NUMBER_OF_ENDPOINTS         10              // max 10 drives

#define MAX_NUMBER_OF_PROVIDERS         10              // max 10 different providers

#define MAX_LEN_OF_PROVIDER_NAME	16

typedef struct _endpoint endpoint_t;
typedef struct _file file_t;
typedef struct _handler handler_t;

typedef struct {
	const char	*name;				// provider name, used in ASSIGN as ID
	const char	*native_charset;		// name of the native charset for that provider
	void		(*init)(void);			// initialization routine
	endpoint_t* 	(*newep)(endpoint_t *parent, const char *par);	// create a new endpoint instance
	endpoint_t* 	(*tempep)(char **par);	// create a new temporary endpoint instance
	void 		(*freeep)(endpoint_t *ep);	// free an endpoint instance

	file_t*		(*wrap)(file_t *file);		// check if the given file is for the provider 
							// and wrap it into a container file_t
	// file-related	
	void		(*close)(endpoint_t *ep, int chan);	// close a channel
        int             (*open)(endpoint_t *ep, int chan, const char *name, const char *opts, int *reclen, int type); // open a file; type is the FS_OPEN_* command value
	int		(*opendir)(endpoint_t *ep, int chan, const char *name, const char *opts); // open a directory for reading
	int		(*readfile)(endpoint_t *ep, int chan, char *retbuf, int len, int *readflag);	// read file data
	int		(*writefile)(endpoint_t *ep, int chan, char *buf, int len, int is_eof);	// write a file

	// command channel
	int		(*scratch)(endpoint_t *ep, char *name, int *outdeleted);// delete
	int		(*rename)(endpoint_t *ep, char *nameto, char *namefrom); // rename a file or dir
	int		(*cd)(endpoint_t *ep, char *name);			// change into new dir
	int		(*mkdir)(endpoint_t *ep, char *name);			// make directory
	int		(*rmdir)(endpoint_t *ep, char *name);			// remove directory
	int		(*position)(endpoint_t *ep, int chan, int recordno);	// position to record
	int		(*block)(endpoint_t *ep, char *buf, char *retbuf, int *retlen); // B-A/B-F
} provider_t;

// values to be set in the out parameter readflag for readfile()
#define	READFLAG_EOF	1
#define	READFLAG_DENTRY	2

struct _endpoint {
	provider_t	*ptype;
	int		is_temporary;
};

// base struct for a file. Is extended in the separate handlers with handler-specific
// information.
//
// The endpoint is recorded for each file and contains the "uppermost" endpoint. A d64 file
// in a zip file in a file system file has three stacked endpoints, one for the file system,
// one for th zip file, and one for the d64. When FS_MOVEing a file, the endpoints of source
// and target are compared, and if identical, the move is forwarded to the endpoint, otherwise
// the file is copied.
struct _file {
        handler_t       *handler;
        endpoint_t      *endpoint;      // endpoint for that file
        file_t          *parent;        // for resolving ".."
        int             isdir;          // when set, is directory
};

int provider_assign(int drive, const char *name, const char *assign_to);

/**
 * looks up a provider like "tcp:" for "fs:" by drive number.
 * Iff the drive number is NAMEINFO_UNDEF_DRIVE then the name is used
 * to identify the provider and ad hoc create a new endpoint from the name.
 * The name pointer is then changed to point after the endpoint information
 * included in the name. For example:
 *
 * drive=NAMEINFO_UNDEF_DRIVE
 * name=tcp:localhost:telnet
 *
 * ends up with the "tcp:" provider, creates a temporary endpoint with
 * "localhost" as host name, and 
 *
 * name=telnet
 * 
 * as the rest of the filename.
 * The endpoint will be closed once the operation is done or the opened file
 * is closed.
 */
endpoint_t* provider_lookup(int drive, char **name);

/**
 * cleans up a temporary provider after it has been done with,
 * i.e. after a command, or after an opened file has been closed.
 * Also after error on open.
 */
void provider_cleanup(endpoint_t *ep);

/*
 * register a new provider, usually called at startup
 */
int provider_register(provider_t *provider);

/*
 * initialize the provider registry
 */
void provider_init(void);

// set the character set for the external communication (i.e. the wireformat)
// modifies the conversion routines for all the providers
void provider_set_ext_charset(char *charsetname);

// get the character set for the external communication (i.e. the wireformat)
// modifies the conversion routines for all the providers
const char* provider_get_ext_charset();

// get the converter TO the provider
charconv_t provider_convto(provider_t *prov);

// get the converter FROM the provider
charconv_t provider_convfrom(provider_t *prov);

// wrap a given (raw) file into a container file_t (i.e. a directory), when
// it can be identified by one of the providers - like a d64 file, or a ZIP file
file_t *provider_wrap(file_t *file);

#endif

