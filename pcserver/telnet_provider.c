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
 * This file is a filesystem provider implementation, to be
 * used with the FSTCP program on an OS/A65 computer. 
 *
 * In this file the actual command work is done for the 
 * local filesystem.
 */

#include "os.h"

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "fscmd.h"

#include "provider.h"
#include "errors.h"
#include "mem.h"

#include "log.h"

#undef DEBUG_READ

#define	MAX_BUFFER_SIZE	64

//#define	min(a,b)	(((a)<(b))?(a):(b))

typedef struct {
	int		chan;		// channel for which the File is

	char		has_lastbyte;	// lastbyte is valid when set
	char		lastbyte;	// last byte read, to handle EOF correctly
	int		sockfd;		// socket file descriptor
} File;

typedef struct {
	// derived from endpoint_t
	struct provider_t 	*ptype;
	// payload
	char			*hostname;	// from assign
	File 			files[MAXFILES];
} tn_endpoint_t;

void tnp_init() {
}

extern provider_t telnet_provider;

static void init_fp(File *fp) {

	//log_debug("initializing fp=%p (used to be chan %d)\n", fp, fp == NULL ? -1 : fp->chan);

	fp->has_lastbyte = 0;
        fp->chan = -1;
        fp->sockfd = -1;
}

// close a file descriptor
static int close_fd(File *file) {
	int er = 0;

	if (file->sockfd >= 0) {
		close(file->sockfd);
		file->sockfd = -1;
	}
	init_fp(file);
	return er;
}

// free the endpoint descriptor
static void tnp_free(endpoint_t *ep) {
        tn_endpoint_t *cep = (tn_endpoint_t*) ep;
        int i;
        for(i=0;i<MAXFILES;i++) {
                close_fd(&(cep->files[i]));
        }

	mem_free(cep->hostname);
        mem_free(ep);
}

// allocate a new endpoint, where the path is giving the 
// hostname for the connections. Not much to do here, as the
// port comes with the file name in the open, so we can get
// the address on the open only.
static endpoint_t *tnp_new(endpoint_t *parent, const char *path) {

	// alloc and init a new endpoint struct
	tn_endpoint_t *tnep = malloc(sizeof(tn_endpoint_t));

        tnep->ptype = (struct provider_t *) &telnet_provider;

	tnep->hostname = NULL;
        for(int i=0;i<MAXFILES;i++) {
		init_fp(&(tnep->files[i]));
        }

	char *hostname = mem_alloc_str(path);
	tnep->hostname = hostname;
	
	log_info("Telnet provider set to hostname '%s'\n", tnep->hostname);

	return (endpoint_t*) tnep;
}

// ----------------------------------------------------------------------------------
// error translation

static int errno_to_error(int err) {

	switch(err) {
	case EEXIST:
		return ERROR_FILE_EXISTS;
	case EACCES:
		return ERROR_NO_PERMISSION;
	case ENAMETOOLONG:
		return ERROR_FILE_NAME_TOO_LONG;
	case ENOENT:
		return ERROR_FILE_NOT_FOUND;
	case ENOSPC:
		return ERROR_DISK_FULL;
	case EROFS:
		return ERROR_WRITE_PROTECT;
	case ENOTDIR:	// mkdir, rmdir
	case EISDIR:	// open, rename
		return ERROR_FILE_TYPE_MISMATCH;
	case ENOTEMPTY:
		return ERROR_DIR_NOT_EMPTY;
	case EMFILE:
		return ERROR_NO_CHANNEL;
	case EINVAL:
		return ERROR_SYNTAX_INVAL;
	default:
		return ERROR_FAULT;
	}
}


// ----------------------------------------------------------------------------------
// if we had closures, we could reuse instead of copy this code across endpoint types...
//

static File *reserve_file(endpoint_t *ep, int chan) {
        tn_endpoint_t *cep = (tn_endpoint_t*) ep;

        for (int i = 0; i < MAXFILES; i++) {
                if (cep->files[i].chan == chan) {
                        close_fd(&(cep->files[i]));
                }
                if (cep->files[i].chan < 0) {
                        File *fp = &(cep->files[i]);
                        init_fp(fp);
                        fp->chan = chan;

			log_debug("reserving file %p for chan %d\n", fp, chan);

                        return &(cep->files[i]);
                }
        }
        log_warn("Did not find free fs session for channel=%d\n", chan);
        return NULL;
}

static File *find_file(endpoint_t *ep, int chan) {
        tn_endpoint_t *cep = (tn_endpoint_t*) ep;

        for (int i = 0; i < MAXFILES; i++) {
                if (cep->files[i].chan == chan) {
                        return &(cep->files[i]);
                }
        }
        log_warn("Did not find fs session for channel=%d\n", chan);
        return NULL;
}

// ----------------------------------------------------------------------------------
// commands as sent from the device

// close a file descriptor
static void close_fds(endpoint_t *ep, int tfd) {
	File *file = find_file(ep, tfd); // ((fs_endpoint_t*)ep)->files;
	if (file != NULL) {
		close_fd(file);
		init_fp(file);
	}
}


// open a socket for reading, writing, or appending
static int open_file(endpoint_t *ep, int tfd, const char *buf, const char *mode) {
	int ern;
	int er = ERROR_FAULT;
	File *file;
	struct addrinfo *addr, *ap;
	struct addrinfo hints;
	int sockfd;

	tn_endpoint_t *tnep = (tn_endpoint_t*) ep;

	log_info("open file for fd=%d on host %s with service/port %s\n", tfd, tnep->hostname, buf);

	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = (AI_V4MAPPED | AI_ADDRCONFIG);

	// 1. get the internet address for it via getaddrinfo
	ern = getaddrinfo(tnep->hostname, buf, &hints, &addr);
	if (ern != 0) {
		log_errno("Did not get address info for %s:%s\n", tnep->hostname, buf);
		return er;
	}

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
		er = ERROR_FAULT;
		ap = NULL;
	}

        if (ap == NULL) {               /* No address succeeded */
            log_error("Could not connect to %s:%s\n", tnep->hostname, buf);
        } else {

		log_debug("Connected with fd=%d\n", sockfd);

		file = reserve_file(ep, tfd);

		if (file) {
			file->sockfd = sockfd;
			er = ERROR_OK;
		} else {
			close(sockfd);
			log_error("Could not reserve file\n");
			er = ERROR_FAULT;
		}
	}

	log_info("OPEN_RD/AP/WR(%s: %s:%s =%p (fd=%d)\n",mode, tnep->hostname, buf, (void*)file, sockfd);

	return er;
}


// read file data
//
// returns positive number of bytes read, or negative error number
//
static int read_file(endpoint_t *ep, int tfd, char *retbuf, int len, int *eof) {
	File *file = find_file(ep, tfd);

	if (file != NULL) {
		int sockfd = file->sockfd;

		int rv = ERROR_OK;

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
			*eof = 1;
			len = file->has_lastbyte ? 1 : 0;
			retbuf[0] = file->lastbyte;
		}
		return len;
	}
	return -ERROR_FAULT;
}

// write file data
static int write_file(endpoint_t *ep, int tfd, char *buf, int len, int is_eof) {
	File *file = find_file(ep, tfd);

#ifdef DEBUG_WRITE
	log_debug("Write_file (telnet): fd=%p\n", file);
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
		return 0;
	}
	return -ERROR_FAULT;
}

// ----------------------------------------------------------------------------------
// command channel


// ----------------------------------------------------------------------------------

static int open_file_rd(endpoint_t *ep, int tfd, const char *buf) {
       return open_file(ep, tfd, buf, "rb");
}

static int open_file_wr(endpoint_t *ep, int tfd, const char *buf) {
       return open_file(ep, tfd, buf, "wb");
}

static int open_file_ap(endpoint_t *ep, int tfd, const char *buf) {
       return open_file(ep, tfd, buf, "ab");
}

static int open_file_rw(endpoint_t *ep, int tfd, const char *buf) {
       return open_file(ep, tfd, buf, "rwb");
}


provider_t telnet_provider = {
	"telnet",
	tnp_init,
	tnp_new,
	tnp_free,
	close_fds,
	open_file_rd,
	open_file_wr,
	open_file_ap,
	open_file_rw,
	NULL,	//open_dr,
	read_file,
	write_file,
	NULL,	//fs_delete,
	NULL,	//fs_rename,
	NULL,	//fs_cd,
	NULL,	//fs_mkdir,
	NULL	//fs_rmdir
};


