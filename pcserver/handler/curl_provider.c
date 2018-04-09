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


#undef DEBUG_CURL

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

static int curl_close(file_t *fp, int recurse, char *outbuf, int *outlen);

typedef enum {
	PROTO_FTP,
	PROTO_FTPS,
	PROTO_HTTP,
	PROTO_HTTPS,
	PROTO_TELNET
} proto_t;

struct curl_endpoint_t;

typedef struct File {
	file_t	file;			// embedded
	int	chan;			// channel
	CURL 	*session;		// curl session info
	CURLM 	*multi;			// curl session info
	int	(*read_converter)(struct curl_endpoint_t *cep, struct File *fp, char *retbuf, int len, int *eof);

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
} File;


static void file_init(const type_t *t, void *obj) {
        (void) t;       // silence unused warning
        File *fp = (File*) obj;

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
}

static type_t file_type = {
        "curl_file",
        sizeof(File),
        file_init
};




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


static curl_endpoint_t *new_endpoint(const char *path) {

	curl_endpoint_t *fsep = mem_alloc(&endpoint_type);

	// separate host from path
	// if hostend is NULL, then we only have the host
	char *hostend=strchr(path, '/');

	fsep->host_buffer = conv_to_alloc(path, &ftp_provider);
	char *p = strchr(fsep->host_buffer, '/');
	if (p != NULL) {
		*p = 0;
	}

	if (hostend != NULL) {
		fsep->path_buffer = conv_to_alloc(hostend+1, &ftp_provider);
	}

	return fsep;
}


static curl_endpoint_t *_new(const char *path) {

	curl_endpoint_t *fsep = new_endpoint(path);

	return fsep;
}

//-----------------------------------------------------
// protocol specific endpoint handling

static endpoint_t *ftp_temp(char **name) {

	log_debug("trying to create temporary drive for '%s'\n", *name);

	// path ends with last '/'
	char *end = strrchr(*name, '/');
	if (end != NULL) {
		*end = 0;
	}

	curl_endpoint_t *fsep = _new(*name);

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

static endpoint_t *http_temp(char **name) {

	log_debug("trying to create temporary drive for '%s'\n", *name);

	// path ends with last '/'
	char *end = strrchr(*name, '/');
	if (end != NULL) {
		*end = 0;
	}

	curl_endpoint_t *fsep = _new(*name);

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

static endpoint_t *ftp_new(endpoint_t *parent, const char *path, int from_cmdline) {

	(void) parent; // silence warning unused parameter
        (void) from_cmdline;    // silence unused parameter warning

	curl_endpoint_t *fsep = _new(path);

	// not sure if this is needed...
	fsep->protocol = PROTO_FTP;

	fsep->base.ptype = &ftp_provider;
	
	return (endpoint_t*) fsep;
}

static endpoint_t *http_new(endpoint_t *parent, const char *path, int from_cmdline) {

	(void) parent; // silence warning unused parameter
        (void) from_cmdline;    // silence unused parameter warning

	curl_endpoint_t *fsep = _new(path);

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

		char *p = conv_to_alloc(file->filename, &ftp_provider);
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

        File *fp = (File*) file;
        curl_endpoint_t *parentep = (curl_endpoint_t*) file->endpoint;

        if (parentep == NULL) {
		return CBM_ERROR_FAULT;
	}

        // alloc and init a new endpoint struct
        curl_endpoint_t *newep = mem_alloc(&endpoint_type);


	newep->host_buffer = mem_alloc_str(parentep->host_buffer);
	if (parentep->path_buffer != NULL) {
		newep->path_buffer = mem_alloc_str(parentep->path_buffer);
	}
	
	newep->path_buffer = add_parent_path(newep->path_buffer, file);

        // free resources
        curl_close((file_t*)fp, 1, NULL, NULL);

        *outep = (endpoint_t*)newep;
        return CBM_ERROR_OK;

}


//-----------------------------------------------------


static void close_fd(File *fp) {

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

	mem_free(fp);
}

static File *reserve_file(endpoint_t *ep) {

	File *file = mem_alloc(&file_type);

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
static int curl_close(file_t *fp, int recurse, char *outbuf, int *outlen) {
	(void) outbuf;

        close_fd((File*)fp);

        if (recurse) {
                if (fp->parent != NULL) {
                        fp->parent->handler->close(fp->parent, 1, NULL, NULL);
                }
        }
	if (outlen != NULL) 
		*outlen = 0;
	return CBM_ERROR_OK;
}

static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *user) {

	File *fp = (File*) user;

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

static CURLMcode pull_data(curl_endpoint_t *cep, File *fp, int *eof) {
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

static int reply_with_data(curl_endpoint_t *cep, File *fp, char *retbuf, int len, int *eof) {

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
static int read_file(file_t *file, char *retbuf, int len, int *readflag) {

	File *fp = (File*) file;

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

        File *fp = reserve_file(ep);

        return (file_t*) fp;
}


static int curl_direntry(file_t *fp, file_t **outentry, int isresolve, int *readflag, const char **outpattern) {
	(void) readflag;	// silence warning
        *outentry = NULL;	// just in case

	file_t *wrapfile = NULL;

        log_debug("ENTER: curl_direntry fp=%p, dirstate=%d\n", fp, fp->dirstate);

        if (fp->handler != &curl_file_handler) {
                return CBM_ERROR_FAULT;
        }

        curl_endpoint_t *tnep = (curl_endpoint_t*) fp->endpoint;

        if (!isresolve) {
		// TODO ftp dir
                // escape, we don't show a dir
                return CBM_ERROR_OK;
        }


        File *retfile = reserve_file((endpoint_t*) tnep);

	retfile->file.parent = fp;

	// compute file name (filename is stored in the external charset, e.g. PETSCII)
        char *name = mem_alloc_str(fp->pattern);
	char *p = strchr(name, '/');
	if (p != NULL) {
		// shorten file/dir name to next dir separator if exists
		*p = 0;
	}
        retfile->file.filename = name;
	retfile->file.isdir = 1;	// just in case

        if ( handler_next((file_t*)retfile, fp->pattern, outpattern, &wrapfile)
                                == CBM_ERROR_OK) {
	        *outentry = wrapfile;
                int rv = CBM_ERROR_OK;
		return rv;
	}

	curl_close((file_t*)retfile, 0, NULL, NULL);

        return CBM_ERROR_OK;
}

// ----------------------------------------------------------------------------------

// open a file for reading, writing, or appending
static int open_file(file_t *file, openpars_t *pars, int type) {

	(void)pars; // silence warning unused parameter

	int rv = CBM_ERROR_FAULT;

	curl_endpoint_t *cep = (curl_endpoint_t*) file->endpoint;
	File *fp = (File*) file;

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

		// prepare name
		mem_append_str5(&cep->name_buffer, 
			((provider_t*)(cep->base.ptype))->name,
			"://",
			cep->host_buffer,
			"/",
			cep->path_buffer);

		cep->name_buffer = add_parent_path(cep->name_buffer, (file_t*)fp);

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

static int open_rd(file_t *file, openpars_t *pars, int type) {

	int rv = open_file(file, pars, type);

	if (rv == CBM_ERROR_OK) {
		File *fp = (File*) file;

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

/**
 * converts the list of file names from an FTP NLST command
 * to a wireformat dir entry.
 *
 * Because of the FS_DIR_* macros used here, the wireformat.h include
 * is required, which I would like to have avoided...
 */
int dir_nlst_read_converter(struct curl_endpoint_t *cep, File *fp, char *retbuf, int len, int *readflag) {

	if (len < FS_DIR_NAME + 1) {
		log_error("read buffer too small for dir entry (is %d, need at least %d)\n",
				len, FS_DIR_NAME+1);
		return -CBM_ERROR_FAULT;
	}

	// prepare dir entry
	memset(retbuf, 0, FS_DIR_NAME+1);	

	*readflag = READFLAG_DENTRY;

	int eof = 0;	
	int l = 0;
	char *namep = retbuf + FS_DIR_NAME;
	switch(fp->read_state) {
	case 0:		// disk name
		l = dir_fill_header(retbuf, 0, cep->path_buffer);
		fp->read_state++;
		break;
	case 1:
		// file names
#ifdef DEBUG_CURL
		log_debug("get filename, bufdatalen=%d, bufrp=%d, eof=%d\n",
			fp->rdbufdatalen, fp->bufrp, *readflag);
#endif

		l = FS_DIR_NAME;
		retbuf[FS_DIR_MODE] = FS_DIR_MOD_FIL;
		do {
			while ((fp->rdbufdatalen <= fp->bufrp) && (eof == 0)) {

				//log_debug("Trying to pull...\n");

				CURLMcode rv = pull_data((curl_endpoint_t*)cep, fp, &eof);

				if (rv != CURLM_OK) {
					log_error("Error retrieving directory data (%d)\n", rv);
					return -CBM_ERROR_DIR_ERROR;
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
		if (l > FS_DIR_NAME) {
			// Check if filename has a known extension, e.g. PRG USR SEQ
			// Default to PRG for files that have no extension
			// Default to SEQ to prevent LOADing unknown extensions
			retbuf[FS_DIR_ATTR] |= extension_to_filetype(retbuf + FS_DIR_NAME,
					FS_DIR_TYPE_PRG, FS_DIR_TYPE_SEQ);
			break;
		}
		// otherwise fall through
	case 2:
		log_debug("final dir entry\n");
		retbuf[FS_DIR_MODE] = FS_DIR_MOD_FRE;
		retbuf[FS_DIR_NAME] = 0;
		l = FS_DIR_NAME + 1;
		*readflag |= READFLAG_EOF;
		fp->read_state++;
		break;
	}
#ifdef DEBUG_CURL
	printf("Got some dir data : datalen=%d\n", l);
        for (int i = 0; i < FS_DIR_NAME; i++) {
	        printf("%02x ", retbuf[i]);
        }
        printf(": %s\n", retbuf+FS_DIR_NAME);
#endif
	return l;
}


static int open_dr(file_t *file, openpars_t *pars) {

	(void)pars; // silence warning unused parameter

	// curl_endpoint_t *cep = (curl_endpoint_t*) file->endpoint;
	File *fp = (File*) file;

	int rv = open_file(file, pars, FS_OPEN_DR);
	if (rv == CBM_ERROR_OK) {

		fp->read_converter = &dir_nlst_read_converter;	// do DIR conversion
	
		// set for receiving
		curl_easy_setopt(fp->session, CURLOPT_WRITEFUNCTION, write_cb);

		// TODO: support for MLST/MLSD FTP commands,
		// only they are so new, I still need to find a running one for tests
		//
		// NLST is the "name list" without other data
		char *cmd = NULL;
		mem_append_str2(&cmd, "NLST ", file->filename);
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

	File *fp = (File*) user;

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
	File *fp = find_file(ep, tfd);

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

static int curl_open(file_t *fp, openpars_t *pars, int type) {

        switch (type) {
                case FS_OPEN_RD:
                        return open_rd(fp, pars, FS_OPEN_RD);
                case FS_OPEN_DR:
                        return open_dr(fp, pars);
                default:
                        return CBM_ERROR_FAULT;
        }
}


// ----------------------------------------------------------------------------------

static void curl_dump_file(file_t *fp, int recurse, int indent) {

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
        log_debug("%spattern='%s';\n", prefix, file->file.pattern);
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
	ftp_new,
	ftp_temp,
	curl_to_endpoint,
	curl_pfree,
	curl_root,
	NULL,	// wrap
	NULL, 	// direct
	NULL,	// format
	curl_dump 	// dump
};

provider_t http_provider = {
	"http",
	CHARSET_ASCII_NAME,
	curl_init,
	http_new,
	http_temp,
	curl_to_endpoint,
	curl_pfree,
	curl_root,
	NULL,	// wrap
	NULL, 	// direct
	NULL,	// format
	curl_dump 	// dump
};

static handler_t curl_file_handler = {
        "curl_file_handler",
        NULL,                   // resolve
        curl_close,             // close
        curl_open,              // open
        handler_parent,         // default parent() implementation
        NULL,                   // fs_seek,                // seek
        read_file,              // readfile
        NULL,			// writefile unsupported for now
        NULL,                   // truncate
        curl_direntry,          // direntry
        NULL,                   // fs_create,              // create
        NULL,                   // fs_flush,               // flush data out to disk
        NULL,                   // fs_equals,              // check if two files (e.g. d64 files are the same)
        NULL,                   // fs_realsize,            // real size of file (same as file->filesize here)
        NULL,                   // fs_delete,              // delete file
        NULL,                   // fs_mkdir,               // create a directory
        NULL,                   // fs_rmdir,               // remove a directory
        NULL,                   // fs_move,                // move a file or directory
        curl_dump_file            // dump file
};


