
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

#include <time.h>
#include <string.h>

#include "mem.h"
#include "charconvert.h"
#include "openpars.h"
#include "registry.h"


// constant for provider tables (deprecated)
#define MAXFILES        16

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

							// create a new endpoint instance
							// this is used on ASSIGN calls
	endpoint_t* 	(*newep)(endpoint_t *parent, const char *par, int from_cmdline);	

							// create a new temporary endpoint instance;
							// this happens when a file with direct provider
							// name is opened, like "ftp:host/dir"
	endpoint_t* 	(*tempep)(char **par);	

							// convert dir to endpoint for assign
	int		(*to_endpoint)(file_t *f, endpoint_t **outep);	
							// free an endpoint instance
	void 		(*freeep)(endpoint_t *ep);	

							// start directory for the endpoint 
	file_t*		(*root)(endpoint_t *ep); 

							// check if the given file is for 
							// the provider 
							// (e.g. a d64 file for the di_provider)
							// and wrap it into a container file_t
							// (pointing to a new temp. endpoint created
							// for it)
	int		(*wrap)(file_t *file, file_t **wrapped); 

	// command channel
							// B-A/B-F
	int		(*block)(endpoint_t *ep, char *buf, char *retbuf, int *retlen); 
	// dump / debug
	void		(*dump)(int indent);
} provider_t;

// values to be set in the out parameter readflag for readfile()
#define	READFLAG_EOF	1
#define	READFLAG_DENTRY	2

struct _endpoint {
	provider_t	*ptype;
	int		is_temporary;
	int 		is_assigned;
	registry_t	files;
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
	// for traversing a directory
	int		dirstate;	// 0 = first line, 1 = file matches, 2 = end line
	const char	*pattern;	// pattern for dir matching
	// file/dir attributes
	size_t		filesize;	// size of file
	time_t		lastmod;	// last modification timestamp
	const char	*filename;	// file name
	uint16_t	recordlen;	// record length (if REL file)
	uint8_t		mode;		// same as FS_DIR_MODE
	uint8_t		type;		// same as FS_DIR_TYPE
	uint8_t		attr;		// same as FS_DIR_ATTR
	uint8_t		writable;	// is file writable?
	uint8_t		seekable;	// is file seekable?	
};

// note: go from FIRST to ENTRIES to END by +1
#define	DIRSTATE_NONE		0
#define	DIRSTATE_FIRST		1
#define	DIRSTATE_ENTRIES	2
#define	DIRSTATE_END		3


// file operations
// This file defines the file type handler interface. This is used
// to "wrap" files so that for example x00 (like P00, R00, ...) files 
// or compressed files can be handled.
//
// In the long run the di_provider will become a handler as well - so
// you can CD into a D64 file stored in a D81 image read from an FTP 
// server...

struct _handler {
        const char      *name;                          // handler name, for debugging

        int             (*resolve)(file_t *infile, file_t **outfile,
                                uint8_t type, const char *name, const openpars_t *pars, const char **outname);

                                                        // close the file; do so recursively by closing
                                                        // parents if recurse is set
        void            (*close)(file_t *fp, int recurse);

        int             (*open)(file_t *fp, openpars_t *pars, int opentype);      // open a file

        // -------------------------
                                                        // get the converter FROM the file
//        charconv_t      (*convfrom)(file_t *prov, const char *tocharset);

							// return the real parent dir; e.g. for x00_parent
							// do not return the wrapped file, but its
							// parent()
	file_t		*(*parent)(file_t *fp);		
						

        // -------------------------
                                                        // position the file
        int             (*seek)(file_t *fp, long position, int flag);

                                                        // read file data
							// if return value >= 0 then is number of bytes read,
							// <0 means -CBM_ERROR_*
							// readflag returns READFLAG_* values
        int             (*readfile)(file_t *fp, char *retbuf, int len, int *readflag);

                                                        // write file data
        int             (*writefile)(file_t *fp, const char *buf, int len, int is_eof);

                                                        // truncate to a given size
        int             (*truncate)(file_t *fp, long size);

        // -------------------------

        int             (*direntry)(file_t *dirfp, file_t **outentry, int isresolve, int *readflag, const char **outpattern);
                                                        // create a new file in the directory
        int             (*create)(file_t *dirfp, file_t **outentry, const char *name, openpars_t *pars, int opentype);

        // -------------------------

	int		(*flush)(file_t *fp);		// flush data out to disk

							// check if the other file is the same
							// as thisfile. Used to check if an opened
							// file is part of an endpoint root
							// returns 0 on equal, 1 on different
	int		(*equals)(file_t *thisfile, file_t *otherfile);

	size_t		(*realsize)(file_t *file);	// returns the real (correct) size of the file

	int		(*scratch)(file_t *file);	// delete file

	int		(*mkdir)(file_t *dir, const char *name, openpars_t *pars);	// make directory

	int		(*rmdir)(file_t *dir);		// remove directory

	int		(*move)(file_t *fromfile, file_t *todir, const char *toname);	// move file

        // -------------------------

	void		(*dump)(file_t *fp, int recurse, int indent); // dump info for analysis / debug

};

// values to be set in the out parameter readflag for readfile()
#define READFLAG_EOF            1
#define READFLAG_DENTRY         2

// values to be set in the seek() flag parameter
#define SEEKFLAG_ABS            0               /* count from the start */
#define SEEKFLAG_END            1               /* count from the end of the file */



int provider_assign(int drive, const char *name, const char *assign_to, int from_cmdline);

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
 *
 * If a default drive is given, and no named provider could be found, the default drive
 * is used instead.
 */
endpoint_t* provider_lookup(const char *inname, int namelen, const char **outname, int default_drive);

/**
 * change directory for an endpoint
 */
int provider_chdir(const char *inname, int namelen);

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

/*
 * dump the in-memory structures (for analysis / debug)
 */
void provider_dump();

// set the character set for the external communication (i.e. the wireformat)
// modifies the conversion routines for all the providers
void provider_set_ext_charset(const char *charsetname);

// get the character set for the external communication (i.e. the wireformat)
const char* provider_get_ext_charset();

// get the converter from external charset TO the provider charset
charconv_t provider_convto(provider_t *prov);

// get the converter FROM the provider charset to the external charset
charconv_t provider_convfrom(provider_t *prov);

// wrap a given (raw) file into a container file_t (i.e. a directory), when
// it can be identified by one of the providers - like a d64 file, or a ZIP file
file_t *provider_wrap(file_t *file);

// default endpoint if none given in assign. Assign path may be adapted in case
// we are from comand line and have relative path, in which case the root path as
// prepended. *new_assign_path is mem_alloc'd and always set.
endpoint_t *fs_root_endpoint(const char *assign_path, char **new_assign_path, int from_cmdline);


// convert string inline from provider to external charset
static inline void conv_from(char *str, provider_t *prov) {
	int len = strlen(str);

	provider_convfrom(prov)(str, len, str, len);
}

static inline char* conv_from_alloc(const char *str, provider_t *prov) {
	int len = strlen(str);

	char *trg = mem_alloc_c(len + 1, "converted_from_name");

	provider_convfrom(prov)(str, len, trg, len);

	trg[len] = 0;
	return trg;
}

static inline char* conv_to_name_alloc(const char *str, const char *csetname) {
	int len = strlen(str);

	log_debug("convert %s from %s to %s\n", str, provider_get_ext_charset(), csetname);

	char *trg = mem_alloc_c(len + 1, "converted_to_name");

	// this is horribly inefficient
	cconv_converter(cconv_getcharset(provider_get_ext_charset()), 
						cconv_getcharset(csetname))(str, len, trg, len);

	trg[len] = 0;
	return trg;
}

static inline char* conv_to_alloc(const char *str, provider_t *prov) {
	int len = strlen(str);

	char *trg = mem_alloc_c(len + 1, "converted_to_name");

	provider_convto(prov)(str, len, trg, len);

	trg[len] = 0;
	return trg;
}

#endif

