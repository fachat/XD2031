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

#include "os.h"

#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdbool.h>


#include "wireformat.h"
#include "provider.h"
#include "log.h"
#include "filetypes.h"
#include "dir.h"


#undef DEBUG_CURL

#define	MAX_BUFFER_SIZE	64

#define	MAX_PROTO_SIZE	11	// max length of "<proto>://", like "webdavs://", plus one "/" path separator

#define	MAX_SESSIONS	10

extern provider_t ftp_provider;
extern provider_t http_provider;
extern provider_t telnet_provider;


//#define	min(a,b)	(((a)<(b))?(a):(b))

typedef enum {
	PROTO_FTP,
	PROTO_FTPS,
	PROTO_HTTP,
	PROTO_HTTPS,
	PROTO_TELNET
} proto_t;

struct curl_endpoint_t;

typedef struct File {
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
	char	*name_buffer;		// malloc'd buffer for dir path
} File;

typedef struct curl_endpoint_t {
	// derived from endpoint_t
	endpoint_t 		base;
	// payload
	proto_t			protocol;	// type of provider
	char			error_buffer[CURL_ERROR_SIZE];
	char			name_buffer[2 * MAX_BUFFER_SIZE + MAX_PROTO_SIZE];
	char			host_buffer[MAX_BUFFER_SIZE];
	char			path_buffer[MAX_BUFFER_SIZE];
	File			files[MAX_SESSIONS];

} curl_endpoint_t;


//static size_t read_cb(char *ptr, size_t size, size_t nmemb, void *user);

static void curl_init() {
	// according to the curl man page, this init can be done multiple times
	CURLcode cc = curl_global_init(CURL_GLOBAL_ALL);
	if (cc != 0) {
		// error handling!
	}
}

static void init_fp(File *fp) {
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
	fp->name_buffer = NULL;
}


static curl_endpoint_t *new_endpoint(const char *path) {
	curl_endpoint_t *fsep = malloc(sizeof(curl_endpoint_t));

	for (int i = 0; i < MAX_SESSIONS; i++) {
		File *fp = &(fsep->files[i]);
		init_fp(fp);
	}

	fsep->error_buffer[0] = 0;
	fsep->name_buffer[0] = 0;
	fsep->path_buffer[0] = 0;
	fsep->host_buffer[0] = 0;
	// separate host from path
	// if hostend is NULL, then we only have the host
	char *hostend=strchr(path, '/');

	int hostlen = strlen(path);
	if (hostend != NULL) {
		hostlen = hostend - path;
	}
	strncpy(fsep->host_buffer, path, hostlen);
	fsep->host_buffer[hostlen] = 0;

	strncpy(fsep->path_buffer, path+hostlen+1, MAX_BUFFER_SIZE);
	fsep->path_buffer[MAX_BUFFER_SIZE-1] = 0;

	return fsep;
}


static curl_endpoint_t *_new(const char *path) {

	curl_endpoint_t *fsep = new_endpoint(path);

	return fsep;
}

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

static endpoint_t *ftp_new(endpoint_t *parent, const char *path) {

	(void) parent; // silence warning unused parameter

	curl_endpoint_t *fsep = _new(path);

	// not sure if this is needed...
	fsep->protocol = PROTO_FTP;

	fsep->base.ptype = &ftp_provider;
	
	return (endpoint_t*) fsep;
}

static endpoint_t *http_new(endpoint_t *parent, const char *path) {

	(void) parent; // silence warning unused parameter

	curl_endpoint_t *fsep = _new(path);

	// not sure if this is needed...
	fsep->protocol = PROTO_HTTP;

	fsep->base.ptype = &http_provider;
	
	return (endpoint_t*) fsep;
}

static File *find_file(endpoint_t *ep, int chan) {
	curl_endpoint_t *cep = (curl_endpoint_t*) ep;

	for (int i = 0; i < MAX_SESSIONS; i++) {
		if (cep->files[i].chan == chan) {
			return &(cep->files[i]);
		}
	}
	log_warn("Did not find curl session for channel=%d\n", chan);
	return NULL;
}

static void close_fd(File *fp) {
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
		init_fp(fp);
	}
	if (fp->name_buffer != NULL) {
		free(fp->name_buffer);
		fp->name_buffer = NULL;
	}
}

static File *reserve_file(endpoint_t *ep, int chan) {
	curl_endpoint_t *cep = (curl_endpoint_t*) ep;

	for (int i = 0; i < MAX_SESSIONS; i++) {
		if (cep->files[i].chan == chan) {
			log_warn("curl session still open for chan %d\n", chan);
			close_fd(&(cep->files[i]));
		} 
		if (cep->files[i].chan < 0) {
			File *fp = &(cep->files[i]);
			init_fp(fp);
			fp->chan = chan;
			return &(cep->files[i]);
		}
	}
	log_warn("Did not find free curl session for channel=%d\n", chan);
	return NULL;
}

void prov_free(endpoint_t *ep) {
	curl_endpoint_t *cep = (curl_endpoint_t*) ep;
	int i;
        for(i=0;i<MAX_SESSIONS;i++) {
		File *fp = &(cep->files[i]);
		if (fp->session != NULL) {
			log_warn("curl session %d is not NULL on free!\n", fp->chan);
		}
		close_fd(&(cep->files[i]));
        }
	free(ep);
}

// ----------------------------------------------------------------------------------
// commands as sent from the device

// close a file descriptor
static void close_fds(endpoint_t *ep, int tfd) {
	File *fp = find_file(ep, tfd);
	if (fp != NULL) {
		if (fp->session == NULL) {
			log_warn("curl sesion %d is NULL!", tfd);
		}
		close_fd(fp);
	}
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
static int read_file(endpoint_t *ep, int tfd, char *retbuf, int len, int *eof) {

	curl_endpoint_t *cep = (curl_endpoint_t*) ep;
	File *fp = find_file(ep, tfd);

	if (fp != NULL) {
		if (fp->read_converter != NULL) {
			// mostly used for directory
			// I'll never understand where I need a "struct" ...
			return fp->read_converter((struct curl_endpoint_t*)cep, fp, retbuf, len, eof);
		}

		if (fp->rdbufdatalen > fp->bufrp) {
#ifdef DEBUG_CURL
			printf("Got some data already: datalen=%d\n", fp->rdbufdatalen);
#endif
			return reply_with_data(cep, fp, retbuf, len, eof);
		}
		// we don't have enough data to send something
		CURLMcode rv = pull_data(cep, fp, &(fp->read_state));

		if (fp->rdbufdatalen > fp->bufrp) {
			// we should now have some data to give back
#ifdef DEBUG_CURL
			printf("Pulled some data: datalen=%d, eof=%d\n", fp->rdbufdatalen, *eof);
#endif
			return reply_with_data(cep, fp, retbuf, len, eof);
		}
		// no data left
			
		if (fp->read_state != 0) {
			log_warn("adding bogus zero byte, to make CBM noticing the EOF\n");
			*eof = READFLAG_EOF;
			*retbuf = 0;
			return 1;
		}

		if (rv == CURLM_OK) {
			return 0;
		}
	} else {
		log_error("fp is NULL on read attempt\n");
	}
	return -CBM_ERROR_FAULT;
}


// open a file for reading, writing, or appending
static File *open_file(endpoint_t *ep, int tfd, const char *buf, int is_dir) {

	curl_endpoint_t *cep = (curl_endpoint_t*) ep;
	File *fp = reserve_file(ep, tfd);

	if (fp != NULL) {
		// create session	
		fp->multi = curl_multi_init();

		if (fp->multi == NULL) {
			log_error("multi session is NULL\n");
			return NULL;
		}

		fp->session = curl_easy_init();

		if (fp->session == NULL) {
			log_error("easy session is NULL\n");
			curl_multi_cleanup(fp->multi);
			return NULL;
		}

		// set options
		curl_easy_setopt(fp->session, CURLOPT_VERBOSE, (long)1);
		//curl_easy_setopt(fp->session, CURLOPT_WRITEFUNCTION, write_cb);
		curl_easy_setopt(fp->session, CURLOPT_WRITEDATA, fp);
		//curl_easy_setopt(fp->session, CURLOPT_READFUNCTION, read_cb);
		curl_easy_setopt(fp->session, CURLOPT_READDATA, fp);
		curl_easy_setopt(fp->session, CURLOPT_ERRORBUFFER, &(cep->error_buffer));

		// prepare name
		strcpy(cep->name_buffer, ((provider_t*)(cep->base.ptype))->name);
		strcat(cep->name_buffer, "://");
		strcat(cep->name_buffer, cep->host_buffer);
		strcat(cep->name_buffer, "/");
		strcat(cep->name_buffer, cep->path_buffer);
		strcat(cep->name_buffer, "/");
		strcat(cep->name_buffer, buf);
		if (is_dir) {
			// end with a slash "/" to indicate a dir list
			strcpy(cep->name_buffer + strlen(cep->name_buffer), "/");
		}

		log_info("curl URL: %s\n", cep->name_buffer);

		// set URL
		curl_easy_setopt(fp->session, CURLOPT_URL, &(cep->name_buffer));

		// debatable...
		curl_easy_setopt(fp->session, CURLOPT_BUFFERSIZE, (long) MAX_BUFFER_SIZE);

		// add to multi session
		//curl_multi_add_handle(fp->multi, fp->session);
	}
	return fp;
}

static int open_rd(endpoint_t *ep, int tfd, const char *buf, const char *opts, int *reclen, int type) {
	(void)opts; // silence warning unused parameter
	(void) reclen;

	if (type != FS_OPEN_RD) {
		return CBM_ERROR_FAULT;
	}

	File *fp = open_file(ep, tfd, buf, 0);
	if (fp != NULL) {

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
	return CBM_ERROR_FAULT;
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


static int open_dr(endpoint_t *ep, int tfd, const char *buf, const char *opts) {

	(void)opts; // silence warning unused parameter

	curl_endpoint_t *cep = (curl_endpoint_t*) ep;
	File *fp = open_file(ep, tfd, buf, 1);
	if (fp != NULL) {
		int namelen = strlen(buf);
		fp->name_buffer = malloc(namelen + 1);
		strncpy(fp->name_buffer, buf, namelen + 1);

		fp->read_converter = &dir_nlst_read_converter;	// do DIR conversion
	
		// set for receiving
		curl_easy_setopt(fp->session, CURLOPT_WRITEFUNCTION, write_cb);

		// TODO: support for MLST/MLSD FTP commands,
		// only they are so new, I still need to find a running one for tests
		//
		// NLST is the "name list" without other data
		strcpy(cep->name_buffer, "NLST ");
		strcpy(cep->name_buffer + strlen(cep->name_buffer), buf);
		// custom FTP command to make info more grokable
		curl_easy_setopt(fp->session, CURLOPT_CUSTOMREQUEST, &(cep->name_buffer));

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

// ----------------------------------------------------------------------------------
// command channel

int do_chdir(endpoint_t *ep, char *name) {
	// change into new dir
	
	curl_endpoint_t *cep = (curl_endpoint_t*) ep;

	log_debug("CHDIR: Current path is '%s', name is '%s'\n", cep->path_buffer, name);

	if (name[0] == '/') {
		// absolute path - just copy over old path
		strncpy(cep->path_buffer, name, MAX_BUFFER_SIZE);
		cep->path_buffer[MAX_BUFFER_SIZE-1] = 0;
		return 0;
	}
	
	// pointer in name
	char *p = name;

	do {
	    int plen = strlen(p);
	    int l = strlen(cep->path_buffer);
	    if ( ((plen > 1) && p[0] == '.' && p[1] == '.')
		&& (plen == 2 || p[2] == '/')) {
		// one dir up

		// remove the last path element
		while (l > 0) {
			if (cep->path_buffer[l] == '/') {
				break;
			}
			l--;
		}
		cep->path_buffer[l] = 0;

		// advance to next path part
		plen = (plen > 2) ? 3 : 2;
	    } else {
		// append new path part
		// find end of next part
		for (plen = 0; p[plen] != 0; plen++) {
			if (p[plen] == '/') {
				break;
			}
		}
		if (plen + l + 1 > MAX_BUFFER_SIZE) {
			log_error("new path for chdir too long: %d\n", plen+l+1);
			return CBM_ERROR_FILE_NAME_TOO_LONG;
		}
		strcat(cep->path_buffer, "/");
		strncat(cep->path_buffer, p, plen);
		cep->path_buffer[MAX_BUFFER_SIZE-1] = 0;
		
		if(p[plen] == '/') plen++;
	    }
	    p = p+plen;
	} while(*p != 0);

	log_debug("CHDIR: New path is '%s'\n", cep->path_buffer);

	return CBM_ERROR_OK;
}


// ----------------------------------------------------------------------------------

provider_t ftp_provider = {
	"ftp",
	"ASCII",
	curl_init,
	ftp_new,
	ftp_temp,
	prov_free,
	NULL,	// wrap

	close_fds,
	open_rd,
	open_dr,
	read_file,
	NULL, // write_file,
	NULL, // do_scratch,
	NULL, // do_rename,
	do_chdir,
	NULL, // do_mkdir,
	NULL,  // do_rmdir
	NULL, 	// block
	NULL 	// direct
};

provider_t http_provider = {
	"http",
	"ASCII",
	curl_init,
	http_new,
	http_temp,
	prov_free,
	NULL,	// wrap

	close_fds,
	open_rd,
	NULL, //open_dr,
	read_file,
	NULL, // write_file,
	NULL, // do_scratch,
	NULL, // do_rename,
	do_chdir,
	NULL, // do_mkdir,
	NULL,  // do_rmdir
	NULL, 	// block
	NULL 	// direct
};



