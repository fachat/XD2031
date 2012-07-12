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

#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


#include "provider.h"
#include "log.h"

#undef DEBUG_CURL

#define	MAX_BUFFER_SIZE	64

#define	MAX_PROTO_SIZE	11	// max length of "<proto>://", like "webdavs://", plus one "/" path separator

#define	MAX_SESSIONS	10

extern provider_t ftp_provider;
extern provider_t http_provider;


//#define	min(a,b)	(((a)<(b))?(a):(b))

typedef enum {
	PROTO_FTP,
	PROTO_FTPS,
	PROTO_HTTP,
	PROTO_HTTPS
} proto_t;

typedef struct {
	int	chan;			// channel
	CURL 	*session;		// curl session info
	CURLM 	*multi;			// curl session info

	char	*buffer;		// transfer buffer (for callback)
	int	buflen;			// transfer buffer length (for callback)
	int	bufdatalen;		// transfer buffer content length (for callback)
	int	bufrp;			// buffer read pointer
} File;

typedef struct {
	// derived from endpoint_t
	struct provider_t 	*ptype;
	// payload
	proto_t			protocol;	// type of provider
	char			error_buffer[CURL_ERROR_SIZE];
	char			name_buffer[2 * MAX_BUFFER_SIZE + MAX_PROTO_SIZE];
	char			path_buffer[MAX_BUFFER_SIZE];
	File			files[MAX_SESSIONS];

} curl_endpoint_t;

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
	fp->buffer = NULL;
	fp->buflen = 0;
	fp->bufdatalen = 0;
	fp->bufrp = 0;
}


static curl_endpoint_t *new_endpoint(const char *path) {
	curl_endpoint_t *fsep = malloc(sizeof(curl_endpoint_t));

	for (int i = 0; i < MAX_SESSIONS; i++) {
		File *fp = &(fsep->files[i]);
		init_fp(fp);
	}

	fsep->error_buffer[0] = 0;
	fsep->name_buffer[0] = 0;
	strncpy(fsep->path_buffer, path, MAX_BUFFER_SIZE);
	fsep->path_buffer[MAX_BUFFER_SIZE-1] = 0;

	return fsep;
}


static curl_endpoint_t *_new(const char *path) {

	curl_endpoint_t *fsep = new_endpoint(path);

	return fsep;
}

static endpoint_t *ftp_new(const char *path) {

	curl_endpoint_t *fsep = _new(path);

	// not sure if this is needed...
	fsep->protocol = PROTO_FTP;

	fsep->ptype = (struct provider_t *) &ftp_provider;
	
	return (endpoint_t*) fsep;
}

static endpoint_t *http_new(const char *path) {

	curl_endpoint_t *fsep = _new(path);

	// not sure if this is needed...
	fsep->protocol = PROTO_HTTP;

	fsep->ptype = (struct provider_t *) &http_provider;
	
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
		if (fp->buffer != NULL) {
			free(fp->buffer);
		}
		init_fp(fp);
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
			log_warn("curl session %d is not NULL on free!", fp->chan);
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
		} else {
			close_fd(fp);
		}
	}
}

static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *user) {

	File *fp = (File*) user;

	size_t inlen = size * nmemb;

//printf("write_cb-> %d (buffer is %d)\n", inlen, fp->buflen);

	if (inlen > fp->buflen) {
		free(fp->buffer);	// fp->buffer NULL is ok, nothing is done
		int alloclen = inlen < 8192 ? 8192 : inlen;
		fp->buffer = malloc(alloclen);
		if (fp->buffer == NULL) {
			// malloc failed
			printf("malloc failed!\n");
			return 0;
		}
		fp->buflen = alloclen;
	}
	memcpy(fp->buffer, ptr, inlen);
	
	fp->bufdatalen = inlen;
	fp->bufrp = 0;

	return inlen;
}

static size_t read_cb(char *ptr, size_t size, size_t nmemb, void *user) {

	return 0;
}

// read file data
static int read_file(endpoint_t *ep, int tfd, char *retbuf, int len, int *eof) {


	curl_endpoint_t *cep = (curl_endpoint_t*) ep;
	File *fp = find_file(ep, tfd);


	if (fp != NULL) {
		if (fp->bufdatalen > fp->bufrp) {
#ifdef DEBUG_CURL
			printf("Got some data already: datalen=%d\n", fp->bufdatalen);
#endif
			// we already have some data to give back
			int datalen = fp->bufdatalen - fp->bufrp;
			if (datalen > len) {
				// we have more data than requested
				datalen = len;
			}
			memcpy(retbuf, &(fp->buffer[fp->bufrp]), datalen);
			fp->bufrp += datalen;
			return datalen;
		}
		// we don't have enough data to send something
		int running_handles = 0;

		CURLMcode rv = CURLM_OK;
		do {
			rv = curl_multi_perform(fp->multi, &running_handles);
#ifdef DEBUG_CURL
			printf("curl read file returns: %d, running=%d\n", rv, running_handles);
#endif
		} while (rv == CURLM_CALL_MULTI_PERFORM);

		int msgs_in_queue = 0;
		CURLMsg *cmsg = NULL;
		while ((cmsg = curl_multi_info_read(fp->multi, &msgs_in_queue)) != NULL) {
		
			printf("cmsg=%p, in queue=%d\n", cmsg, msgs_in_queue);
	
			// we only added one easy handle, so easy_handle is known
			CURLMSG msg = cmsg->msg;
			if (msg == CURLMSG_DONE) {
				CURLcode cc = cmsg->data.result;
				printf("cc = %d\n", cc);
				printf("errorbuffer = %s\n", cep->error_buffer);
				*eof = 1;
				break;
			}
		}

		if (fp->bufdatalen > fp->bufrp) {
			// we should now have some data to give back
			int datalen = fp->bufdatalen - fp->bufrp;
			if (datalen > len) {
				// we have more data than requested
				datalen = len;
			}
			memcpy(retbuf, &(fp->buffer[fp->bufrp]), datalen);
			fp->bufrp += datalen;
			return datalen;
		}

		if (rv == CURLM_OK) {
			return 0;
		}
	} else {
		log_error("fp is NULL on read attempt\n");
	}
	return -22;
}


// open a file for reading, writing, or appending
static File *open_file(endpoint_t *ep, int tfd, const char *buf) {

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
		strcpy(cep->name_buffer, ((provider_t*)(cep->ptype))->name);
		strcpy(cep->name_buffer + strlen(cep->name_buffer), "://");
		strcpy(cep->name_buffer + strlen(cep->name_buffer), &(cep->path_buffer));
		strcpy(cep->name_buffer + strlen(cep->name_buffer), "/");
		strcpy(cep->name_buffer + strlen(cep->name_buffer), buf);

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

static int open_rd(endpoint_t *ep, int tfd, const char *buf) {

	File *fp = open_file(ep, tfd, buf);
	if (fp != NULL) {

		// set for receiving
		curl_easy_setopt(fp->session, CURLOPT_WRITEFUNCTION, write_cb);

		// protocol specific stuff	
		//if (fp->protocol == FTP) {
		// 	// FTP append
		// 	curl_easy_setopt(fp->session, CURLOPT_APPEND);
		//}	

		//add to multi session
		CURLMcode rv = curl_multi_add_handle(fp->multi, fp->session);

		printf("multi add returns %d\n", rv);

		return 0;
	}
	return -22;
}

// open a directory read
static int open_dr(endpoint_t *ep, int tfd, const char *buf) {
#if 0
	File *files = ((fs_endpoint_t*)ep)->files;
		// close the currently open files
		// so we don't loose references to open files
		close_fds(ep, tfd);

		// save pattern for later comparisons
		strcpy(files[tfd].dirpattern, buf);
		DIR *dp = opendir("." /*buf+FSP_DATA*/);
printf("OPEN_DR(%s)=%p\n",buf,(void*)dp);
		if(dp) {
		  files[tfd].fp = NULL;
		  files[tfd].dp = dp;
		  files[tfd].is_first = 1;
		  return 0;
		}
		// TODO: open error (maybe depending on errno?)
		return -1;
#endif
}

// read directory
static int read_dir(endpoint_t *ep, int tfd, char *retbuf, int *eof) {
#if 0
	File *files = ((fs_endpoint_t*)ep)->files;
		int rv = 0;
		  if (files[tfd].is_first) {
		    files[tfd].is_first = 0;
		    int l = dir_fill_header(retbuf, 0, files[tfd].dirpattern);
		    rv = l;
		    files[tfd].de = dir_next(files[tfd].dp, files[tfd].dirpattern);
		    return rv;
		  }
		  if(!files[tfd].de) {
		    close_fds(ep, tfd);
		    *eof = 1;
		    int l = dir_fill_disk(retbuf);
		    rv = l;
		    return rv;
		  }
		  int l = dir_fill_entry(retbuf, files[tfd].de, MAX_BUFFER_SIZE-FSP_DATA);
		  rv = l;
		  // prepare for next read (so we know if we're done)
		  files[tfd].de = dir_next(files[tfd].dp, files[tfd].dirpattern);
		  return rv;
#endif
}


// write file data
static int write_file(endpoint_t *ep, int tfd, char *buf, int len, int is_eof) {
#if 0
	File *files = ((fs_endpoint_t*)ep)->files;
		FILE *fp = files[tfd].fp;
		if(fp) {
		  if (len > FSP_DATA) {
		    // TODO: evaluate return value
		    int n = fwrite(buf+FSP_DATA, 1, len-FSP_DATA, fp);
		  }
		  if(is_eof) {
		    close_fds(ep, tfd);
		  }
		  return 0;
		}
		return -1;
#endif
}

// ----------------------------------------------------------------------------------
// command channel



// ----------------------------------------------------------------------------------

static int readfile(endpoint_t *ep, int chan, char *retbuf, int len, int *eof) {
	return read_file(ep, chan, retbuf, len, eof);

#if 0
	File *files = ((fs_endpoint_t*)ep)->files;
	int rv = 0;
	File *f = &files[chan];
	if (f->dp) {
		rv = read_dir(ep, chan, retbuf, eof);
	} else
	if (f->fp) {
		// read a file
		rv = read_file(ep, chan, retbuf, len, eof);
	}
	return rv;
#endif
}


provider_t ftp_provider = {
	"ftp",
	curl_init,
	ftp_new,
	prov_free,

	close_fds,
	open_rd,
	NULL, //open_ftp_wr,
	NULL, //open_ftp_ap,
	NULL, //open_ftp_rw,
	NULL, //open_ftp_dr,
	readfile,
	write_file,
	NULL
};

provider_t http_provider = {
	"http",
	curl_init,
	http_new,
	prov_free,

	close_fds,
	open_rd,
	NULL, //open_ftp_wr,
	NULL, //open_ftp_ap,
	NULL, //open_ftp_rw,
	NULL, //open_ftp_dr,
	readfile,
	write_file,
	NULL
};


