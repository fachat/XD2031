/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2012,2018 Andre Fachat

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

/*
 * This file is a filesystem provider implementation, to be
 * used with the FSTCP program on an OS/A65 computer. 
 *
 * In this file the actual command work is done for a TCP socket connection
 */


#include "os.h"

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>


#include "provider.h"
#include "handler.h"
#include "errors.h"
#include "mem.h"

#include "charconvert.h"
#include "wireformat.h"

#include "log.h"

#undef DEBUG_READ

#define	MAX_BUFFER_SIZE	64

#define	TELNET_PORT	"23"

//#define	min(a,b)	(((a)<(b))?(a):(b))

static handler_t tcp_file_handler;
extern provider_t tcp_provider;

// list of endpoints
static registry_t endpoints;

typedef struct {
	file_t		file;		// embedded
	int		isroot;		// !=0 when root

	char		has_lastbyte;	// lastbyte is valid when set
	char		lastbyte;	// last byte read, to handle EOF correctly
	int		sockfd;		// socket file descriptor
} File;

static void file_init(const type_t *t, void *obj) {
        (void) t;       // silence unused warning
        File *fp = (File*) obj;

        //log_debug("initializing fp=%p (used to be chan %d)\n", fp, fp == NULL ? -1 : fp->chan);

        fp->file.handler = &tcp_file_handler;
        fp->file.recordlen = 0;

	fp->has_lastbyte = 0;
        fp->isroot = -0;
        fp->sockfd = -1;
}

static type_t file_type = {
        "tn_file",
        sizeof(File),
        file_init
};

typedef struct {
	direntry_t 	de;
	//direntry_t	*parent_de;
} tcp_dirent_t;

static type_t tcp_dirent_type = {
	"tcp_dirent",
	sizeof(tcp_dirent_t),
	NULL
};

typedef struct {
	// derived from endpoint_t
	endpoint_t 	base;
	// payload
	char			*hostname;	// from assign
} tn_endpoint_t;

static void endpoint_init(const type_t *t, void *obj) {
        (void) t;       // silence unused warning
        tn_endpoint_t *fsep = (tn_endpoint_t*)obj;
        reg_init(&(fsep->base.files), "tcp_endpoint_files", 16);

        fsep->base.ptype = &tcp_provider;

        fsep->base.is_temporary = 0;
        fsep->base.is_assigned = 0;

        reg_append(&endpoints, fsep);
}

static type_t endpoint_type = {
        "tcp_endpoint",
        sizeof(tn_endpoint_t),
        endpoint_init
};



// close a file descriptor
static int close_fd(File *file) {
	int er = 0;

	if (file->sockfd >= 0) {
		close(file->sockfd);
		file->sockfd = -1;
	}
	mem_free(file);
	return er;
}

static File *reserve_file(tn_endpoint_t *fsep) {

        File *file = mem_alloc(&file_type);

        file->file.endpoint = (endpoint_t*)fsep;

        reg_append(&fsep->base.files, file);

        return file;
}

static void tnp_free_file(registry_t *reg, void *en) {
	(void)reg;
        ((file_t*)en)->handler->fclose((file_t*)en, NULL, NULL);
}

static void tnp_free_ep(registry_t *reg, void *en) {
        (void) reg;
        tn_endpoint_t *tnpep = (tn_endpoint_t*)en;
        reg_free(&(tnpep->base.files), tnp_free_file);
	if (tnpep->hostname) {
		mem_free(tnpep->hostname);
	}
        mem_free(en);
}

static void tnp_end(void) {
	reg_free(&endpoints, tnp_free_ep);
}

static void tnp_init(void) {

	reg_init(&endpoints, "tcp_endpoints", 5);
}


// free the endpoint descriptor
static void tnp_do_free(endpoint_t *ep) {
        tn_endpoint_t *cep = (tn_endpoint_t*) ep;

        if (reg_size(&ep->files)) {
                log_warn("tcp_free(): trying to close endpoint %p with %d open files!\n",
                        ep, reg_size(&ep->files));
                return;
        }
        if (ep->is_assigned > 0) {
                log_warn("tcp_free(): trying to free endpoint %p still assigned\n", ep);
                return;
        }

        reg_remove(&endpoints, cep);

	mem_free(cep->hostname);
        mem_free(ep);
}

static void tnp_free(endpoint_t *ep) {

        if (ep->is_assigned > 0) {
                ep->is_assigned--;
        }

        if (ep->is_assigned == 0) {
                tnp_do_free(ep);
        }
}

// internal helper
static tn_endpoint_t *create_ep() {
	// alloc and init a new endpoint struct
	tn_endpoint_t *tnep = mem_alloc(&endpoint_type);

	tnep->hostname = NULL;

	return tnep;
}

// allocate a new endpoint, where the name parameter is giving the 
// hostname for the connections, plus the port name. The name parameter
// is modified such that it points to the port name after this endpoint
// is created.
// Syntax is:
//	<hostname>:<portname>
//
static endpoint_t *tnp_temp(char **name, charset_t cset, int priv) {

	(void) priv;

	char *end = strchr(*name, '/');
	
	if (end != NULL) {
		// no ':' separator between host and port found
		log_error("Do not use ':' name!\n");
		return NULL;
	}
	int n = strlen(*name); //end - *name;

	tn_endpoint_t *tnep = create_ep();

	// create new string and copy the first n bytes of *name into it
	char *hostname = mem_alloc_strn(*name, n);
	tnep->hostname = conv_name_alloc(hostname, cset, CHARSET_ASCII);
	mem_free(hostname);

	*name = *name+n;	// char after the ':'
	
	log_info("Telnet provider set to hostname '%s'\n", tnep->hostname);

	return (endpoint_t*) tnep;
}

// ----------------------------------------------------------------------------------
// error translation

static int errno_to_error(int err) {

	switch(err) {
	case EEXIST:
		return CBM_ERROR_FILE_EXISTS;
	case EACCES:
		return CBM_ERROR_NO_PERMISSION;
	case ENAMETOOLONG:
		return CBM_ERROR_FILE_NAME_TOO_LONG;
	case ENOENT:
		return CBM_ERROR_FILE_NOT_FOUND;
	case ENOSPC:
		return CBM_ERROR_DISK_FULL;
	case EROFS:
		return CBM_ERROR_WRITE_PROTECT;
	case ENOTDIR:	// mkdir, rmdir
	case EISDIR:	// open, rename
		return CBM_ERROR_FILE_TYPE_MISMATCH;
	case ENOTEMPTY:
		return CBM_ERROR_DIR_NOT_EMPTY;
	case EMFILE:
		return CBM_ERROR_NO_CHANNEL;
	case EINVAL:
		return CBM_ERROR_SYNTAX_INVAL;
	default:
		return CBM_ERROR_FAULT;
	}
}


// ----------------------------------------------------------------------------------
// commands as sent from the device


// open a socket for reading, writing, or appending
static int open_file(File *file, openpars_t *pars, const char *mode) {
	int ern;
	int er = CBM_ERROR_FAULT;
	struct addrinfo *addr, *ap;
	struct addrinfo hints;
	int sockfd;

	(void) pars;	// silence

	tn_endpoint_t *tnep = (tn_endpoint_t*) file->file.endpoint;

	const char *filename = mem_alloc_str2(file->file.filename, "tcp_filename");

	log_info("open file on host %s with service/port %s\n", tnep->hostname, filename);

	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = (AI_V4MAPPED | AI_ADDRCONFIG);

	// 1. get the internet address for it via getaddrinfo
	ern = getaddrinfo(tnep->hostname, filename, &hints, &addr);
	if (ern != 0) {
		log_error("Did not get address info for %s:%s, returns %d (%s)", tnep->hostname, filename,
				ern, gai_strerror(ern));
		mem_free(filename);
		return er;
	}
	mem_free(filename);

        /* getaddrinfo() returns a list of address structures.
           Try each address until we successfully bind(2).
           If socket(2) (or bind(2)) fails, we (close the socket
           and) try the next address. */

        for (ap = addr; ap != NULL; ap = ap->ai_next) {

	    log_debug("Trying to connect to: ap=%p\n", ap);
	    
            sockfd = socket(ap->ai_family, ap->ai_socktype,
                    ap->ai_protocol);

            if (sockfd == -1) {
		log_errno("Could not connect");
                continue;
	    }

	    int ern = connect(sockfd, ap->ai_addr, ap->ai_addrlen);
	    if (ern == 0) {
                break;                  /* Success */
	    }

	    log_warn("Could not connect due to %s\n", gai_strerror(ern));

            close(sockfd);
        }

        freeaddrinfo(addr);           /* No longer needed */

	ern = fcntl(sockfd, F_SETFL, O_NONBLOCK);

	if (ern != 0) {
		log_errno("Could not set to non-blocking!");
		close(sockfd);
		er = CBM_ERROR_FAULT;
		ap = NULL;
	}

        if (ap == NULL) {               /* No address succeeded */
            	log_error("Could not connect to %s:%s\n", tnep->hostname, file->file.filename);
        } else {

		log_debug("Connected with fd=%d\n", sockfd);

		file->sockfd = sockfd;
		er = CBM_ERROR_OK;
	}

	log_info("OPEN_RD/AP/WR(%s: %s:%s =%p (fd=%d)\n",mode, tnep->hostname, file->file.filename, (void*)file, sockfd);

	file->file.writable = 1;
	return er;
}


// read file data
//
// returns positive number of bytes read, or negative error number
//
static int read_file(file_t *fp, char *retbuf, int len, int *readflag, charset_t outcset) {

	(void) outcset;

	File *file = (File*)fp;

	if (file != NULL) {
		int sockfd = file->sockfd;

		int rv = CBM_ERROR_OK;

		retbuf[0] = file->lastbyte;

		ssize_t n = read(sockfd, file->has_lastbyte ? retbuf+1 : retbuf, 
					file->has_lastbyte ? len - 1 : len);

#ifdef DEBUG_READ
		log_debug("Read %ld bytes from socket fd=%d\n", n, sockfd);
#endif

		if (n < 0) {
			// error condition
			if (errno != EAGAIN && errno != EWOULDBLOCK) {
				// error reading from socket
				rv = errno_to_error(errno);
				log_errno("Error reading from socket!\n");
				return -rv;
			}
			// no data available, not in error, just non-blocking
			len = 0;
		} else 
		if (n > 0) {
			// got some real data
			// what's in the buffer?
			len = n + (file->has_lastbyte ? 1 : 0);

			// NOTE: this probably belongs into an option, or
			// make it different defaults for read only and read/write files

			// keep one for EOF handling (note: len here always >= 1)
			//len--;
			//file->lastbyte = retbuf[len];
			//file->has_lastbyte = 1;
			file->has_lastbyte = 0;
		} else
		if (n == 0) {
			// got an EOF
			*readflag = READFLAG_EOF;
			len = file->has_lastbyte ? 1 : 0;
			retbuf[0] = file->lastbyte;
		}
		return len;
	}
	return -CBM_ERROR_FAULT;
}

// write file data
static int write_file(file_t *fp, const char *buf, int len, int is_eof) {
	File *file = (File*)fp;

#ifdef DEBUG_WRITE
	log_debug("Write_file (tcp): fd=%p\n", file);
#endif

	if (file != NULL) {

		int sockfd = file->sockfd;

		while (len > 0) {
			ssize_t nw = write(sockfd, buf, len);

			if (nw < 0) {
				log_errno("Error writing to socket\n");
				break;
			}
			len -= nw;
		}

		if (is_eof) {
			//close_fd(file);
		}
		return 0;
	}
	return -CBM_ERROR_FAULT;
}

// ----------------------------------------------------------------------------------
// command channel

static int tn_direntry2(file_t *fp, direntry_t **outentry, int isdirscan, int *readflag, const char *preview, charset_t cset) {

	(void)readflag; // silence warning unused parameter;

        log_debug("ENTER: tcp_provider.direntry fp=%p, dirstate=%d\n", fp, fp->dirstate);

        if (fp->handler != &tcp_file_handler) {
                return CBM_ERROR_FAULT;
        }

	*outentry = NULL;

	if (isdirscan) {
		// escape, we don't show a dir
		return CBM_ERROR_OK;
	}

	char *name = conv_name_alloc((preview == NULL) ? TELNET_PORT : preview, CHARSET_ASCII, cset);

	tcp_dirent_t *de = mem_alloc(&tcp_dirent_type);
	de->de.handler = &tcp_file_handler;
	de->de.parent = fp;
	de->de.size = 0;
	de->de.moddate = 0;
	de->de.mode = FS_DIR_MOD_FIL;
	de->de.type = 0;
	de->de.attr = 0;
	de->de.name = (uint8_t*)name;

	*outentry = (direntry_t*) de;

	*readflag = READFLAG_EOF;

	return CBM_ERROR_OK;
}

static int tn_declose(direntry_t *dirent) {

	mem_free(dirent->name);
	mem_free(dirent);

	return CBM_ERROR_OK;
}

// ----------------------------------------------------------------------------------

// resolve directory path when searching a file path
static int tn_resolve2 (const char **pattern, charset_t cset, file_t **inoutdir) {

	(void) pattern;
	(void) cset;
	(void) inoutdir;

	return CBM_ERROR_FILE_EXISTS;
}


// ----------------------------------------------------------------------------------

static int tn_open2 (direntry_t * de, openpars_t * pars, int opentype, file_t **outfp) {

	int err = CBM_ERROR_OK;

	File *fp = mem_alloc(&file_type);
	fp->file.filename = mem_alloc_str2((char*)de->name, "tn_filename");

	switch (opentype) {
		case FS_OPEN_RD:
       			err = open_file(fp, pars, "rb");
			break;
		case FS_OPEN_WR:
		case FS_OPEN_OW:
       			err = open_file(fp, pars, "wb");
			break;
		case FS_OPEN_AP:
       			err = open_file(fp, pars, "ab");
			break;
		case FS_OPEN_RW:
       			err = open_file(fp, pars, "rwb");
			break;
		default:
			return CBM_ERROR_FAULT;
	}

	if (err == CBM_ERROR_OK) {
		*outfp = (file_t*)fp;
	} else {
		close_fd(fp);
		*outfp = NULL;
	}
	return err;
}


static file_t *tnp_root(endpoint_t *ep) {
	tn_endpoint_t *tep = (tn_endpoint_t*) ep;

	log_entry("tn_provider.tnp_root");

	File *fp = reserve_file(tep);

	return (file_t*) fp;
}

// ----------------------------------------------------------------------------------

static void tn_dump_file(file_t *fp, int recurse, int indent) {

        File *file = (File*)fp;
        const char *prefix = dump_indent(indent);

        log_debug("%shandler='%s';\n", prefix, file->file.handler->name);
        log_debug("%sparent='%p';\n", prefix, file->file.parent);
        if (recurse) {
                log_debug("%s{\n", prefix);
                if (file->file.parent != NULL && file->file.parent->handler->dump != NULL) {
                        file->file.parent->handler->dump(file->file.parent, 1, indent+1);
                }
                log_debug("%s}\n", prefix);

        }
        log_debug("%sisdir='%d';\n", prefix, file->file.isdir);
        log_debug("%sdirstate='%d';\n", prefix, file->file.dirstate);
        //log_debug("%spattern='%s';\n", prefix, file->file.pattern);
        log_debug("%sfilesize='%d';\n", prefix, file->file.filesize);
        log_debug("%sfilename='%s';\n", prefix, file->file.filename);
        log_debug("%srecordlen='%d';\n", prefix, file->file.recordlen);
        log_debug("%smode='%d';\n", prefix, file->file.mode);
        log_debug("%stype='%d';\n", prefix, file->file.type);
        log_debug("%sattr='%d';\n", prefix, file->file.attr);
        log_debug("%swritable='%d';\n", prefix, file->file.writable);
        log_debug("%sseekable='%d';\n", prefix, file->file.seekable);
}

static void tn_dump_ep(tn_endpoint_t *fsep, int indent) {

        const char *prefix = dump_indent(indent);
        int newind = indent + 1;
        const char *eppref = dump_indent(newind);

        log_debug("%sprovider='%s';\n", prefix, fsep->base.ptype->name);
        log_debug("%sis_temporary='%d';\n", prefix, fsep->base.is_temporary);
        log_debug("%sis_assigned='%d';\n", prefix, fsep->base.is_assigned);
        log_debug("%shostname='%d';\n", prefix, fsep->hostname);
        log_debug("%sfiles={;\n", prefix);
        for (int i = 0; ; i++) {
                File *file = (File*) reg_get(&fsep->base.files, i);
                log_debug("%s// file at %p\n", eppref, file);
                if (file != NULL) {
                        log_debug("%s{\n", eppref, file);
                        if (file->file.handler->dump != NULL) {
                                file->file.handler->dump((file_t*)file, 0, newind+1);
                        }
                        log_debug("%s{\n", eppref, file);
                } else {
                        break;
                }
        }
        log_debug("%s}\n", prefix);
}

static void tnp_dump(int indent) {

        const char *prefix = dump_indent(indent);
        int newind = indent + 1;
        const char *eppref = dump_indent(newind);

        log_debug("%s// tcp system provider\n", prefix);
        log_debug("%sendpoints={\n", prefix);
        for (int i = 0; ; i++) {
                tn_endpoint_t *fsep = (tn_endpoint_t*) reg_get(&endpoints, i);
                if (fsep != NULL) {
                        log_debug("%s// endpoint %p\n", eppref, fsep);
                        log_debug("%s{\n", eppref);
                        tn_dump_ep(fsep, newind+1);
                        log_debug("%s}\n", eppref);
                } else {
                        break;
                }
        }
        log_debug("%s}\n", prefix);
}

// ----------------------------------------------------------------------------------


static handler_t tcp_file_handler = {
        "tcp_file_handler",
        tn_resolve2,            // resolve2
	NULL,			// wrap
	NULL,			// fclose
	tn_declose,		// declose
        tn_open2,              	// open2
        handler_parent,         // default parent() implementation
        NULL,			// fs_seek,                // seek
        read_file,               // readfile
        write_file,              // writefile
        NULL,                   // truncate
        tn_direntry2,           // direntry2
        NULL,			// fs_create,              // create
	NULL,			// fs_flush,               // flush data out to disk
	NULL,			// fs_equals,              // check if two files (e.g. d64 files are the same)
        NULL,			// fs_realsize2,            // real size of file (same as file->filesize here)
        NULL,			// fs_delete2,              // delete file
        NULL,			// fs_mkdir,               // create a directory
        NULL,			// fs_rmdir2,               // remove a directory
        NULL,			// fs_move2,                // move a file or directory
        tn_dump_file            // dump file
};


provider_t tcp_provider = {
	"tcp",
	CHARSET_ASCII_NAME,		// not used as we don't do directories, but still
	tnp_init,
	tnp_end,
	tnp_temp,
	NULL,				// to_endpoint
	tnp_free,
	tnp_root,			// root - basically only a handle to open files (ports)
	NULL,				// block
	NULL,				// format
	tnp_dump			// dump
};


