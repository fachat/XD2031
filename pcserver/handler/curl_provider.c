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

/*
 * This file is a curl provider implementation, to be
 * used with the FSTCP program on an OS/A65 computer. 
 *
 * In this file the actual command work is done for the 
 * curl-library-based internet access filesystem.
 */

#ifndef _WIN32

#define DEBUG_CURL

#include "os.h"

#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdbool.h>


#include "wireformat.h"
#include "provider.h"
#include "handler.h"
#include "log.h"
#include "filetypes.h"
#include "dir.h"


#define	MAX_BUFFER_SIZE	64

#define	MAX_PROTO_SIZE	11	// max length of "<proto>://", like "webdavs://", plus one "/" path separator

#define	MAX_SESSIONS	10

extern provider_t ftp_provider;
extern provider_t http_provider;

static handler_t curl_file_handler;

// has init been done?
static int curl_init_done = 0;
// list of endpoints
static registry_t endpoints;

static int curl_fclose(file_t *fp, char *outbuf, int *outlen);

typedef enum {
	PROTO_FTP,
	PROTO_FTPS,
	PROTO_HTTP,
	PROTO_HTTPS,
	PROTO_TELNET
} proto_t;

struct curl_endpoint_t;

typedef struct curl_file_s curl_file_t;

struct curl_file_s {
	file_t	file;			// embedded
	int	chan;			// channel
	CURL 	*session;		// curl session info
	CURLM 	*multi;			// curl session info
	char 	*path;			// path to current file
	int	(*read_converter)(struct curl_endpoint_t *cep, curl_file_t *fp, char *retbuf, int len, int *eof);

	char	*wrbuffer;		// write transfer buffer (for callback) - malloc'd
	int	wrbuflen;		// write transfer buffer length (for callback)
	int	wrbufdatalen;		// write transfer buffer content length (for callback)
	int	bufwp;			// buffer write pointer
	char	*rdbuffer;		// read transfer buffer (for callback) - malloc'd
	int	rdbuflen;		// read transfer buffer length (for callback)
	int	rdbufdatalen;		// read transfer buffer content length (for callback)
	int	bufrp;			// buffer read pointer
	// directory read state
	int	read_state;		// data for read_converter / read_file
};


static void file_init(const type_t *t, void *obj) {
        (void) t;       // silence unused warning
        curl_file_t *fp = (curl_file_t*) obj;

        //log_debug("initializing fp=%p (used to be chan %d)\n", fp, fp == NULL ? -1 : fp->chan);

        fp->file.handler = &curl_file_handler;
        fp->file.recordlen = 0;

	fp->chan = -1;
	fp->session = NULL;
	fp->multi = NULL;

	fp->rdbuffer = NULL;
	fp->rdbuflen = 0;
	fp->rdbufdatalen = 0;
	fp->bufrp = 0;

	fp->wrbuffer = NULL;
	fp->wrbuflen = 0;
	fp->wrbufdatalen = 0;
	fp->bufwp = 0;

	fp->read_converter = NULL;
	fp->read_state = 0;

	fp->path = NULL;
}

static type_t file_type = {
        "curl_file",
        sizeof(curl_file_t),
        file_init
};


static int open_dr(curl_file_t *file, openpars_t *pars);


//#define	min(a,b)	(((a)<(b))?(a):(b))

typedef struct curl_endpoint_t {
	// derived from endpoint_t
	endpoint_t 		base;
	// payload
	proto_t			protocol;	// type of provider
	char			error_buffer[CURL_ERROR_SIZE];
	char			*name_buffer;
	char			*host_buffer;
	char			*path_buffer;

} curl_endpoint_t;


static void endpoint_init(const type_t *t, void *obj) {
        (void) t;       // silence unused warning
        curl_endpoint_t *fsep = (curl_endpoint_t*)obj;

        reg_init(&(fsep->base.files), "curl_endpoint_files", 16);

        fsep->base.ptype = &ftp_provider;

        fsep->base.is_temporary = 0;
        fsep->base.is_assigned = 0;

	fsep->error_buffer[0] = 0;
	fsep->name_buffer = NULL;
	fsep->path_buffer = NULL;
	fsep->host_buffer = NULL;

        reg_append(&endpoints, fsep);
}

static type_t endpoint_type = {
        "curl_endpoint",
        sizeof(curl_endpoint_t),
        endpoint_init
};

typedef struct {
	direntry_t	de;
} curl_dirent_t;

static void curl_dirent_init(const type_t *t, void *obj) {

	(void) t;

	curl_dirent_t *de = (curl_dirent_t*)obj;

	de->de.handler = &curl_file_handler;
	de->de.parent = NULL;
	de->de.size = 0;
	de->de.moddate = 0;
	de->de.mode = FS_DIR_MOD_FIL;
	de->de.type = 0;
	de->de.attr = 0;
	de->de.name = NULL;
	de->de.cset = CHARSET_ASCII;
}

static type_t curl_dirent_type = {
        "curl_dirent",
        sizeof(curl_dirent_t),
	curl_dirent_init
};


static void curl_free_file(registry_t *reg, void *en) {
	(void)reg;
        ((file_t*)en)->handler->fclose((file_t*)en, NULL, NULL);
}

static void curl_free_ep(registry_t *reg, void *en) {
        (void) reg;
        curl_endpoint_t *diep = (curl_endpoint_t*)en;
        reg_free(&(diep->base.files), curl_free_file);

        mem_free(en);
}

static void curl_end() {
	reg_free(&endpoints, curl_free_ep);
}

// note: curl_init is being called twice, for HTTP as well as FTP
static void curl_init() {

	if (!curl_init_done) {

		reg_init(&endpoints, "curl_endpoints", 10);

		// according to the curl man page, this init can be done multiple times
		CURLcode cc = curl_global_init(CURL_GLOBAL_ALL);
		if (cc != 0) {
			// error handling!
		}

		curl_init_done = 1;
	}
}


static curl_endpoint_t *new_endpoint(const char *path, charset_t cset) {

	curl_endpoint_t *fsep = mem_alloc(&endpoint_type);


	// separate host from path
	// if hostend is NULL, then we only have the host
	char *hostend=strchr(path, '/');
	// remove leading "/"
	while (hostend == path) {
		path++;
		hostend = strchr(path, '/');
	}

	fsep->host_buffer = conv_name_alloc(path, cset, CHARSET_ASCII);
	char *p = strchr(fsep->host_buffer, '/');
	if (p != NULL) {
		*p = 0;
	}

	if (hostend != NULL) {
		fsep->path_buffer = conv_name_alloc(hostend+1, cset, CHARSET_ASCII);
	}

	return fsep;
}


//-----------------------------------------------------
// protocol specific endpoint handling

static endpoint_t *ftp_temp(char **name, charset_t cset, int priv) {

	(void) priv;

	log_debug("trying to create temporary drive for '%s'\n", *name);

	// path ends with last '/'
	char *end = strrchr(*name, '/');
	if (end != NULL) {
		*end = 0;
	}

	curl_endpoint_t *fsep = new_endpoint(*name, cset);

	if (end == NULL) {
		*name = (*name)+strlen(*name);
	} else {
		*name = end+1;	// filename part
	}

	// not sure if this is needed...
	fsep->protocol = PROTO_FTP;

	fsep->base.ptype = &ftp_provider;
	
	return (endpoint_t*) fsep;
}

static endpoint_t *http_temp(char **name, charset_t cset, int priv) {

	(void) priv;

	log_debug("trying to create temporary drive for '%s'\n", *name);

	// path ends with last '/'
	char *end = strrchr(*name, '/');
	if (end != NULL) {
		*end = 0;
	}

	curl_endpoint_t *fsep = new_endpoint(*name, cset);

	if (end == NULL) {
		*name = (*name)+strlen(*name);
	} else {
		*name = end+1;	// filename part
	}

	// not sure if this is needed...
	fsep->protocol = PROTO_HTTP;

	fsep->base.ptype = &http_provider;
	
	return (endpoint_t*) fsep;
}

static char* add_parent_path(char *buffer, file_t *file) {

	if (file->parent != NULL) {
		buffer = add_parent_path(buffer, file->parent);
	}
	if (file->filename != NULL) {

		//strcat(buffer, file->filename);

		char *p = mem_alloc_str2(file->filename, "curl_parent_path");
		char *newbuf = malloc_path(buffer, p);
		mem_free(p);
		mem_free(buffer);
		buffer = newbuf;
	}
	return buffer;
}

/**
 *make a dir into an endpoint (only called with isdir=1)
 */
static int curl_to_endpoint(file_t *file, endpoint_t **outep) {

        if (file->handler != &curl_file_handler) {
                log_error("Wrong file type (unexpected)\n");
                return CBM_ERROR_FAULT;
        }

        curl_file_t *fp = (curl_file_t*) file;
        curl_endpoint_t *parentep = (curl_endpoint_t*) file->endpoint;

        if (parentep == NULL) {
		return CBM_ERROR_FAULT;
	}

        // alloc and init a new endpoint struct
        curl_endpoint_t *newep = mem_alloc(&endpoint_type);


	newep->host_buffer = mem_alloc_str2(parentep->host_buffer, "curl_dirname_to_endpoint");
	if (parentep->path_buffer != NULL) {
		newep->path_buffer = mem_alloc_str2(parentep->path_buffer, "curl_to_endpoint_name");
	}
	
	newep->path_buffer = add_parent_path(newep->path_buffer, file);

        // free resources
        curl_fclose((file_t*)fp, NULL, NULL);

        *outep = (endpoint_t*)newep;
        return CBM_ERROR_OK;

}


//-----------------------------------------------------


static void close_fd(curl_file_t *fp) {

	reg_remove(&fp->file.endpoint->files, fp);

	if (fp->session != NULL) {
		curl_multi_remove_handle(fp->multi, fp->session);
		curl_easy_cleanup(fp->session);
		curl_multi_cleanup(fp->multi);
		if (fp->rdbuffer != NULL) {
			free(fp->rdbuffer);
		}
		if (fp->wrbuffer != NULL) {
			free(fp->wrbuffer);
		}
	}

	mem_free(fp->file.filename);
	mem_free(fp->path);
	mem_free(fp);
}

static curl_file_t *reserve_file(endpoint_t *ep) {

	curl_file_t *file = mem_alloc(&file_type);

	file->file.endpoint = ep;

	reg_append(&ep->files, file);

	return file;
}

void curl_pfree(endpoint_t *ep) {

        if (reg_size(&ep->files)) {
                log_warn("curl_free(): trying to close endpoint %p with %d open files!\n",
                        ep, reg_size(&ep->files));
                return;
        }
        if (ep->is_assigned > 0) {
                log_warn("curl_free(): trying to free endpoint %p still assigned\n", ep);
                return;
        }

        reg_remove(&endpoints, ep);

	curl_endpoint_t *cep = (curl_endpoint_t*) ep;
	
	if (cep->host_buffer != NULL) {
		mem_free(cep->host_buffer);
	}
	if (cep->path_buffer != NULL) {
		mem_free(cep->path_buffer);
	}
	if (cep->name_buffer != NULL) {
		mem_free(cep->name_buffer);
	}

        mem_free(ep);
}

// ----------------------------------------------------------------------------------
// commands as sent from the device

// close a file descriptor
static int curl_fclose(file_t *fp, char *outbuf, int *outlen) {
	(void) outbuf;

        close_fd((curl_file_t*)fp);

	if (outlen != NULL) 
		*outlen = 0;
	return CBM_ERROR_OK;
}

static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *user) {

	curl_file_t *fp = (curl_file_t*) user;

	fp->rdbufdatalen = 0;

	long inlen = size * nmemb;

#ifdef DEBUG_CURL
printf("write_cb-> %ld (buffer is %d): ", inlen, fp->rdbuflen);
if (fp->rdbuflen > 0) {
	printf("%02x ", ptr[0]);
}
printf("\n");
#endif

	if (inlen > fp->rdbuflen) {
		free(fp->rdbuffer);	// fp->rdbuffer NULL is ok, nothing is done
		int alloclen = inlen < 8192 ? 8192 : inlen;
		fp->rdbuffer = malloc(alloclen);
		if (fp->rdbuffer == NULL) {
			// malloc failed
			printf("malloc failed!\n");
			return 0;
		}
		fp->rdbuflen = alloclen;
	}
	memcpy(fp->rdbuffer, ptr, inlen);
	
	fp->rdbufdatalen = inlen;
	fp->bufrp = 0;

	return inlen;
}

static CURLMcode pull_data(curl_endpoint_t *cep, curl_file_t *fp, int *eof) {
	int running_handles = 0;
	
	*eof = 0;
	fp->rdbufdatalen = 0;
	fp->bufrp = 0;

	CURLMcode rv = CURLM_OK;
	do {
		rv = curl_multi_perform(fp->multi, &running_handles);
#ifdef DEBUG_CURL
		if (rv != 0 || running_handles != 1) {
			printf("curl read file returns: %d, running=%d\n", rv, running_handles);
		}
#endif
	} while (rv == CURLM_CALL_MULTI_PERFORM);

	// note that on R/W files EOF will not be set by the read side ending,
	// as the write side will keep running_handles>0, except of course this also
	// ends
	if (running_handles == 0) {
		// no more data will be available
		log_debug("pull_data sets EOF (running=0), rv=%d, datalen=%d\n", rv, fp->rdbufdatalen);
		*eof = READFLAG_EOF;
	}

	int msgs_in_queue = 0;
	CURLMsg *cmsg = NULL;
	while ((cmsg = curl_multi_info_read(fp->multi, &msgs_in_queue)) != NULL) {
#ifdef DEBUG_CURL	
		printf("cmsg=%p, in queue=%d\n", (void*)cmsg, msgs_in_queue);
#endif	
		// we only added one easy handle, so easy_handle is known
		CURLMSG msg = cmsg->msg;
		if (msg == CURLMSG_DONE) {
			CURLcode cc = cmsg->data.result;
			//printf("cc = %d\n", cc);
			if (cc != CURLE_OK) {
				log_error("errorbuffer = %s\n", cep->error_buffer);
			}
			log_debug("pull_data sets EOF (err msg)\n");
			*eof = READFLAG_EOF;
			break;
		}
	}
	return rv;
}

static int reply_with_data(curl_endpoint_t *cep, curl_file_t *fp, char *retbuf, int len, int *eof) {

	// we already have some data to give back
	int datalen = fp->rdbufdatalen - fp->bufrp;
	if (datalen > len) {
		// we have more data than requested
		datalen = len;
	}
	memcpy(retbuf, &(fp->rdbuffer[fp->bufrp]), datalen);
	fp->bufrp += datalen;

	if (fp->rdbufdatalen <= fp->bufrp) {
		// reached the end of the buffer
		CURLMcode rv = CURLM_OK;
		if (!fp->read_state) {		// set from previous pull_data
			// try to pull in more, so we see if eof should be sent
			rv = pull_data(cep, fp, &(fp->read_state));
			if (rv != CURLM_OK) {
				log_debug("reply_with_data sets EOF rv=%d, datalen=%d\n", rv, fp->rdbufdatalen);
				*eof = READFLAG_EOF;
			}
		}

		if (fp->read_state && fp->rdbufdatalen <= fp->bufrp) {
			log_debug("reply with data sets EOF (2)\n");
			*eof = READFLAG_EOF;
		}
	}
	return datalen;
}

// read file data
static int read_file(file_t *file, char *retbuf, int len, int *readflag, charset_t outcset) {

	(void) outcset;

	curl_file_t *fp = (curl_file_t*) file;

		curl_endpoint_t *cep = (curl_endpoint_t*) fp->file.endpoint;

		if (fp->read_converter != NULL) {
			// mostly used for directory
			// I'll never understand where I need a "struct" ...
			return fp->read_converter((struct curl_endpoint_t*)cep, fp, retbuf, len, readflag);
		}

		if (fp->rdbufdatalen > fp->bufrp) {
#ifdef DEBUG_CURL
			printf("Got some data already: datalen=%d\n", fp->rdbufdatalen);
#endif
			return reply_with_data(cep, fp, retbuf, len, readflag);
		}
		// we don't have enough data to send something
		CURLMcode rv = pull_data(cep, fp, &(fp->read_state));

		if (fp->rdbufdatalen > fp->bufrp) {
			// we should now have some data to give back
#ifdef DEBUG_CURL
			printf("Pulled some data: datalen=%d, eof=%d\n", fp->rdbufdatalen, *readflag);
#endif
			return reply_with_data(cep, fp, retbuf, len, readflag);
		}
		// no data left
			
		if (fp->read_state != 0) {
			log_warn("adding bogus zero byte, to make CBM noticing the EOF\n");
			*readflag = READFLAG_EOF;
			*retbuf = 0;
			return 1;
		}

		if (rv == CURLM_OK) {
			return 0;
		}
	
	return -CBM_ERROR_FAULT;
}

// ----------------------------------------------------------------------------------
// resolve and directory handling

static file_t *curl_root(endpoint_t *ep) {

        log_entry("curl_root");

        curl_file_t *fp = reserve_file(ep);

        return (file_t*) fp;
}


static int curl_declose(direntry_t *dirent) {

	mem_free(dirent->name);
	mem_free(dirent);

	return CBM_ERROR_OK;
}

static int curl_direntry2(file_t *dirfp, direntry_t **outentry, int isdirscan, int *readflag, const char *preview, charset_t cset) {

	(void) preview;
	(void) cset;
	(void) readflag;	// silence warning

	int err = CBM_ERROR_OK;
        *outentry = NULL;	// just in case

        log_debug("ENTER: curl_direntry2 '%s' fp=%p, dirstate=%d\n", dirfp->filename, dirfp, dirfp->dirstate);

        if (dirfp->handler != &curl_file_handler) {
                return CBM_ERROR_FAULT;
        }

	curl_file_t *fp = (curl_file_t*) dirfp;
	
	if (fp->session == NULL) {
		err = open_dr(fp, NULL);
	}

        curl_endpoint_t *cep = (curl_endpoint_t*) dirfp->endpoint;

	// read direntry
	*readflag = READFLAG_DENTRY;

	// create direntry to return
	curl_dirent_t *de = mem_alloc(&curl_dirent_type);
	de->de.parent = dirfp;

	int eof = 0;	
	int l = 0;
	int len = 64;
	char name[64];
	char *namep = name;

	switch(fp->read_state) {
	case 0:		// disk name
		if (isdirscan) {
			de->de.mode = FS_DIR_MOD_NAM;
			de->de.name = (uint8_t*)mem_alloc_str2(cep->path_buffer, "curl_de_diskname");
			de->de.cset = CHARSET_ASCII;
			fp->read_state++;
			break;
		}
		// falls through
	case 1:
		// file names
#ifdef DEBUG_CURL
		log_debug("get filename, bufdatalen=%d, bufrp=%d, eof=%d\n",
			fp->rdbufdatalen, fp->bufrp, *readflag);
#endif
		de->de.mode = FS_DIR_MOD_FIL;
		do {
			while ((fp->rdbufdatalen <= fp->bufrp) && (eof == 0)) {

				//log_debug("Trying to pull...\n");

				CURLMcode rv = pull_data((curl_endpoint_t*)cep, fp, &eof);

				if (rv != CURLM_OK) {
					log_error("Error retrieving directory data (%d)\n", rv);
					mem_free(de);
					return CBM_ERROR_DIR_ERROR;
				}
			}
			// find length of name
			
			while (fp->bufrp < fp->rdbufdatalen) {
				char c = fp->rdbuffer[fp->bufrp];
				fp->bufrp ++;
				if (c == 13) {
					// ignore
				} else
				if (c != 10) {
					*namep = c;
					namep++;
					l++;
				} else {
					// end of name
					*namep = 0;
					l++;
#ifdef DEBUG_CURL
					log_debug("bufrp=%d, datalen=%d\n", fp->bufrp, fp->rdbufdatalen);
#endif
					break;
				}
				if ((l + 2) >= len) {
					log_error("read buffer too small for dir name (is %d, need at least %d, concatenating)\n",
						len, l+2);
					*namep = 0;
					l++;
					break;
				}
			}


			if (eof != 0) {
				log_debug("end of dir read\n");
				fp->read_state++;
				*namep = 0;
			}
		}
		while ((*namep != 0) && (eof == 0));	// not null byte, then not done
		eof = 0;

		de->de.name = (uint8_t*)mem_alloc_str2(name, "curl_direntry");

		de->de.attr |= extension_to_filetype(name,
					FS_DIR_TYPE_PRG, FS_DIR_TYPE_SEQ);
		break;
	case 2:
		if (isdirscan) {
			log_debug("final dir entry\n");
			de->de.mode = FS_DIR_MOD_FRE;
			*readflag |= READFLAG_EOF;
			fp->read_state++;
			break;
		}
		// falls through
	case 3:
		err = CBM_ERROR_FAULT;
	}

	*outentry = (direntry_t*) de;

	return err;
}


// ----------------------------------------------------------------------------------

// open a file for reading, writing, or appending
static int open_file(curl_file_t *file, openpars_t *pars, int type) {

	(void)pars; // silence warning unused parameter

	int rv = CBM_ERROR_FAULT;

	curl_endpoint_t *cep = (curl_endpoint_t*) file->file.endpoint;
	curl_file_t *fp = (curl_file_t*) file;

		// create session	
		fp->multi = curl_multi_init();

		if (fp->multi == NULL) {
			log_error("multi session is NULL\n");
			return rv;
		}

		fp->session = curl_easy_init();

		if (fp->session == NULL) {
			log_error("easy session is NULL\n");
			curl_multi_cleanup(fp->multi);
			return rv;
		}

		// set options
		curl_easy_setopt(fp->session, CURLOPT_VERBOSE, (long)1);
		//curl_easy_setopt(fp->session, CURLOPT_WRITEFUNCTION, write_cb);
		curl_easy_setopt(fp->session, CURLOPT_WRITEDATA, fp);
		//curl_easy_setopt(fp->session, CURLOPT_READFUNCTION, read_cb);
		curl_easy_setopt(fp->session, CURLOPT_READDATA, fp);
		curl_easy_setopt(fp->session, CURLOPT_ERRORBUFFER, &(cep->error_buffer));

		mem_free(cep->name_buffer);
		cep->name_buffer = NULL;
		// prepare name
		mem_append_str5(&cep->name_buffer, 
			((provider_t*)(cep->base.ptype))->name,
			"://",
			cep->host_buffer,
			"/",
			cep->path_buffer);

		mem_append_str2(&cep->name_buffer, "/", fp->path);

		if (type == FS_OPEN_DR) {
			// end with a slash "/" to indicate a dir list
			mem_append_str2(&cep->name_buffer, "/", NULL);
		}

		log_info("curl URL: %s\n", cep->name_buffer);

		// set URL
		curl_easy_setopt(fp->session, CURLOPT_URL, cep->name_buffer);

		// debatable...
		curl_easy_setopt(fp->session, CURLOPT_BUFFERSIZE, (long) MAX_BUFFER_SIZE);

		// add to multi session
		//curl_multi_add_handle(fp->multi, fp->session);

		rv = CBM_ERROR_OK;
	
	return rv;
}

static int open_rd(curl_file_t *file, openpars_t *pars, int type) {

	int rv = open_file(file, pars, type);

	if (rv == CBM_ERROR_OK) {
		curl_file_t *fp = (curl_file_t*) file;

		// set for receiving
		curl_easy_setopt(fp->session, CURLOPT_WRITEFUNCTION, write_cb);

// HTTP
//  curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
		// protocol specific stuff	
		//if (fp->protocol == FTP) {
		// 	// FTP append
		// 	curl_easy_setopt(fp->session, CURLOPT_APPEND);
		//}	

		//add to multi session
		CURLMcode rv = curl_multi_add_handle(fp->multi, fp->session);

		printf("multi add returns %d\n", rv);

		return CBM_ERROR_OK;
	}
	return rv;
}


static int open_dr(curl_file_t *file, openpars_t *pars) {

	// curl_endpoint_t *cep = (curl_endpoint_t*) file->endpoint;
	curl_file_t *fp = (curl_file_t*) file;

	int rv = open_file(fp, pars, FS_OPEN_DR);
	if (rv == CBM_ERROR_OK) {

		// set for receiving
		curl_easy_setopt(fp->session, CURLOPT_WRITEFUNCTION, write_cb);

		// TODO: support for MLST/MLSD FTP commands,
		// only they are so new, I still need to find a running one for tests
		//
		// NLST is the "name list" without other data
		char *cmd = NULL;
		mem_append_str2(&cmd, "NLST ", "" /*file->filename*/);
		// custom FTP command to make info more grokable
		curl_easy_setopt(fp->session, CURLOPT_CUSTOMREQUEST, cmd);
		mem_free(cmd);

		//add to multi session
		CURLMcode rv = curl_multi_add_handle(fp->multi, fp->session);

		printf("multi add returns %d\n", rv);

		return CBM_ERROR_OK;
	}
	return CBM_ERROR_FAULT;
}


// write file data
#if 0

static size_t read_cb(char *ptr, size_t size, size_t nmemb, void *user) {

	curl_file_t *fp = (curl_file_t*) user;

	long rdlen = size * nmemb;
	long datalen = fp->wrbufdatalen - fp->bufwp;

#ifdef DEBUG_CURL
printf("read_cb-> %ld (buffer is %ld): ", rdlen, datalen);
printf("\n");
#endif
	if (datalen > rdlen) {
		datalen = rdlen;
	}

	memcpy(ptr, fp->wrbuffer+fp->bufwp, datalen);

	fp->bufwp += datalen;
	if (fp->bufwp >= fp->wrbufdatalen) {
		fp->bufwp = 0;
		fp->wrbufdatalen = 0;
	}
	return datalen;
}

// write file data
static int write_file(endpoint_t *ep, int tfd, char *buf, int len, int iseof) {

	curl_endpoint_t *cep = (curl_endpoint_t*) ep;
	curl_file_t *fp = find_file(ep, tfd);

	CURLMcode rv = CURLM_OK;

#ifdef DEBUG_CURL
	log_debug("write_file: tfd=%d, len=%d\n", tfd, len);
#endif
	
	if (len == 0) {
		return 0;
	}
	
	if (fp != NULL) {

		if (fp->wrbufdatalen + len > fp->wrbuflen) {
			// buffer would overflow, so flush it
			while (fp->wrbufdatalen > 0) {
				// we haven't sent it yet
				rv = pull_data(cep, fp, &(fp->read_state));

				if (rv != CURLM_OK) {
					log_error("Error writing data to curl\n");
					fp->wrbufdatalen = 0;
					fp->bufwp = 0;
					break;
				}
			}
		}

		memcpy(fp->wrbuffer + fp->wrbufdatalen, buf, len);
		fp->wrbufdatalen += len;

		CURLMcode rv = pull_data(cep, fp, &(fp->read_state));

		if (rv == CURLM_OK) {
			return 0;
		}
	} else {
		log_error("fp is NULL on read attempt\n");
	}
	return -CBM_ERROR_FAULT;
}

#endif

static int curl_open2(direntry_t *dirent, openpars_t *pars, int type, file_t **outfp) {

	int err = CBM_ERROR_OK;

	curl_file_t *cfp = reserve_file(dirent->parent->endpoint);
	cfp->file.filename = mem_alloc_str2((char*)dirent->name, "curl_open_filename");

	char *p = ((curl_file_t*)dirent->parent)->path;
	if (p) {
		cfp->path = p;
		mem_append_str2(&cfp->path, "/", (char*)dirent->name);
	} else {
		cfp->path = mem_alloc_str2((char*)dirent->name, "curl_open_path");
	}

        switch (type) {
                case FS_OPEN_RD:
                        err = open_rd(cfp, pars, FS_OPEN_RD);
			break;
                case FS_OPEN_DR:
                        err = open_dr(cfp, pars);
			break;
                default:
                        err = CBM_ERROR_FAULT;
        }

	if (err == CBM_ERROR_OK) {
		*outfp = (file_t*) cfp;
	} else {
		close_fd(cfp);
		*outfp = NULL;
	}
	return err;
}


// ----------------------------------------------------------------------------------

static void curl_dump_file(file_t *fp, int recurse, int indent) {

        curl_file_t *file = (curl_file_t*)fp;
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

static void curl_dump_ep(curl_endpoint_t *fsep, int indent) {

        const char *prefix = dump_indent(indent);
        int newind = indent + 1;
        const char *eppref = dump_indent(newind);

        log_debug("%sprovider='%s';\n", prefix, fsep->base.ptype->name);
        log_debug("%sis_temporary='%d';\n", prefix, fsep->base.is_temporary);
        log_debug("%sis_assigned='%d';\n", prefix, fsep->base.is_assigned);
        log_debug("%sfiles={;\n", prefix);
        for (int i = 0; ; i++) {
                curl_file_t *file = (curl_file_t*) reg_get(&fsep->base.files, i);
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

static void curl_dump(int indent) {

        const char *prefix = dump_indent(indent);
        int newind = indent + 1;
        const char *eppref = dump_indent(newind);

        log_debug("%s// tcp system provider\n", prefix);
        log_debug("%sendpoints={\n", prefix);
        for (int i = 0; ; i++) {
                curl_endpoint_t *fsep = (curl_endpoint_t*) reg_get(&endpoints, i);
                if (fsep != NULL) {
                        log_debug("%s// endpoint %p\n", eppref, fsep);
                        log_debug("%s{\n", eppref);
                        curl_dump_ep(fsep, newind+1);
                        log_debug("%s}\n", eppref);
                } else {
                        break;
                }
        }
        log_debug("%s}\n", prefix);
}

// ----------------------------------------------------------------------------------

provider_t ftp_provider = {
	"ftp",
	CHARSET_ASCII_NAME,
	curl_init,
	curl_end,
	ftp_temp,
	curl_to_endpoint,
	curl_pfree,
	curl_root,
	NULL, 	// direct
	NULL,	// format
	curl_dump 	// dump
};

provider_t http_provider = {
	"http",
	CHARSET_ASCII_NAME,
	curl_init,
	curl_end,
	http_temp,
	curl_to_endpoint,
	curl_pfree,
	curl_root,
	NULL, 	// direct
	NULL,	// format
	curl_dump 	// dump
};

static handler_t curl_file_handler = {
        "curl_file_handler",
        NULL, 		        // resolve2
        NULL,                   // wrap
	curl_fclose,		// fclose
	curl_declose,		// declose
        curl_open2, 	       	// open2
        handler_parent,         // default parent() implementation
        NULL,                   // fs_seek,                // seek
        read_file,              // readfile
        NULL,			// writefile unsupported for now
        NULL,                   // truncate
        curl_direntry2,         // direntry2
        NULL,                   // fs_create,              // create
        NULL,                   // fs_flush,               // flush data out to disk
        NULL,                   // fs_equals,              // check if two files (e.g. d64 files are the same)
        NULL,                   // fs_realsize2,            // real size of file (same as file->filesize here)
        NULL,                   // fs_delete2,              // delete file
        NULL,                   // fs_mkdir,               // create a directory
        NULL,                   // fs_rmdir2,               // remove a directory
        NULL,                   // fs_move2,                // move a file or directory
        curl_dump_file            // dump file
};

#endif

