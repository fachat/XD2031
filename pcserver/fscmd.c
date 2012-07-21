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
 * This file is a server FSTCP filesystem implementation, to be
 * used with the FSTCP program on an OS/A65 computer. 
 *
 * In this file the actual command work is done
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <strings.h>

#include "wireformat.h"
#include "fscmd.h"
#include "dir.h"
#include "provider.h"
#include "log.h"

#define DEBUG_CMD
#undef DEBUG_READ
#undef DEBUG_WRITE

#define	MAX_BUFFER_SIZE			64

//------------------------------------------------------------------------------------
// Mapping from channel number for open files to endpoint providers
// These are set when the channel is opened

typedef struct {
       int             channo;
       endpoint_t      *ep;
} chan_t;

chan_t chantable[MAX_NUMBER_OF_ENDPOINTS];


void chan_init() {
       int i;
        for(i=0;i<MAX_NUMBER_OF_ENDPOINTS;i++) {
          chantable[i].channo = -1;
        }
}

endpoint_t *chan_to_endpoint(int chan) {

       int i;
        for(i=0;i<MAX_NUMBER_OF_ENDPOINTS;i++) {
               if (chantable[i].channo == chan) {
                       return chantable[i].ep;
               }
        }
       log_info("Did not find ep for channel %d\n", chan);
       return NULL;
}

void free_chan(int channo) {
       int i;
        for(i=0;i<MAX_NUMBER_OF_ENDPOINTS;i++) {
               if (chantable[i].channo == channo) {
                       chantable[i].channo = -1;
                       chantable[i].ep = NULL;
               }
        }
}

void set_chan(int channo, endpoint_t *ep) {
       int i;
        for(i=0;i<MAX_NUMBER_OF_ENDPOINTS;i++) {
               // we overwrite existing entries, to "heal" leftover cruft
               // just in case...
               if ((chantable[i].channo == -1) || (chantable[i].channo == channo)) {
                       chantable[i].channo = channo;
                       chantable[i].ep = ep;
                       return;
               }
        }
       log_error("Did not find free ep slot for channel %d\n", channo);
}

//------------------------------------------------------------------------------------
//

void cmd_init() {
	provider_init();
	chan_init();
}


void cmd_loop(int readfd, int writefd) {

        char buf[8192];
        int wrp,rdp,plen, cmd;
        int n;

            /* write and read pointers in the input buffer "buf" */
        wrp = rdp = 0;

        while((n=read(readfd, buf+wrp, 8192-wrp))!=0) {
#ifdef DEBUG_READ
	      printf("read %d bytes: ",n);
	      for(int i=0;i<n;i++) printf("%02x ",buf[wrp+i]); printf("\n");
#endif

              if(n<0) {
                fprintf(stderr,"fstcp: read error %d (%s)\n",errno,strerror(errno));
                break;
              }
              wrp+=n;
              if(rdp && (wrp==8192 || rdp==wrp)) {
                if(rdp!=wrp) {
                  memmove(buf, buf+rdp, wrp-rdp);
                }
                wrp -= rdp;
                rdp = 0;
              }
//printf("wrp=%d, rdp=%d, FSP_LEN=%d\n",wrp,rdp,FSP_LEN);
              // as long as we have more than FSP_LEN bytes in the buffer
              // i.e. 2 or more, we loop and process packets
              // FSP_LEN is the position of the packet length
              while(wrp-rdp > FSP_LEN) {
                // first byte in packet is command, second is length of packet
                plen = 255 & buf[rdp+FSP_LEN];	//  AND with 255 to fix sign
                cmd = 255 & buf[rdp+FSP_CMD];	//  AND with 255 to fix sign
//printf("wrp-rdp=%d, plen=%d\n",wrp-rdp,plen);
                // full packet received already?
                if (cmd == FS_SYNC || plen < 3) {
		  // a packet is at least 3 bytes
		  // or the byte is the FS_SYNC command
		  // so ignore byte and shift one in buffer position
		  rdp++;
		} else 
                if(wrp-rdp >= plen) {
                  // yes, then execute
                  do_cmd(buf+rdp, writefd);
                  rdp +=plen;
                } else {
                  break;
                }
              }
            }

}


// ----------------------------------------------------------------------------------

/**
 * This function executes a packet from the device.
 * The return data is written into the file descriptor fd
 */
void do_cmd(char *buf, int fd) {
	int tfd, cmd;
	unsigned int len;
	char retbuf[200];
	int rv;

	cmd = buf[FSP_CMD];		// 0
	len = 255 & buf[FSP_LEN];	// 1

	if (cmd == FS_TERM) {
		if (len > 200) {
			printf("TERM length exceeds 200! Is %d\n", len);
			len = 200;
		}
		memcpy(retbuf, buf+3, len-3);
		retbuf[len-3] = 0;
		printf("%s",retbuf);
		return;
	}

#ifdef DEBUG_CMD
	{
		int n = buf[FSP_LEN];
		printf("cmd: %d bytes @%p: ",n, buf);
		for(int i=0;i<n;i++) printf("%02x ",buf[i]); printf("\n");
	}
#endif

	// not on FS_TERM
	tfd = buf[FSP_FD];
	if (tfd < 0) {
		printf("Illegal file descriptor: %d\n", tfd);
		return;
	}

	buf[(unsigned int)buf[FSP_LEN]] = 0;	// 0-terminator

	provider_t *prov = NULL; //&fs_provider;
	endpoint_t *ep = NULL;

#if 0
	printf("got cmd=%d, fd=%d, name=%s",cmd,tfd,
			(cmd<FS_ASSIGN)?((char*)buf+FSP_DATA):"null");
	for (int i = 3; i < buf[FSP_LEN]; i++) printf(" %02x", (unsigned int)buf[i]);
	printf("\n");
#endif
	retbuf[FSP_CMD] = FS_REPLY;
	retbuf[FSP_LEN] = 4;
	retbuf[FSP_FD] = tfd;
	retbuf[FSP_DATA] = -22;

	int eof = 0;
	int outdeleted = 0;

	switch(cmd) {
		// file-oriented commands
	case FS_OPEN_WR:
		ep = provider_lookup(buf[FSP_DATA]);
		if (ep != NULL) {
			prov = (provider_t*) ep->ptype;
			rv = prov->open_wr(ep, tfd, buf + FSP_DATA + 1);
			retbuf[FSP_DATA] = rv;
			if (rv == 0) {
				set_chan(tfd, ep);
			}
		}
		break;
	case FS_OPEN_DR:
		ep = provider_lookup(buf[FSP_DATA]);
		if (ep != NULL) {
			prov = (provider_t*) ep->ptype;
			// not all providers support directory operation
			if (prov->opendir != NULL) {
				rv = prov->opendir(ep, tfd, buf + FSP_DATA + 1);
				retbuf[FSP_DATA] = rv;
				if (rv == 0) {
					set_chan(tfd, ep);
				}
			}
		}
		break;
	case FS_OPEN_RD:
		ep = provider_lookup(buf[FSP_DATA]);
		if (ep != NULL) {
			prov = (provider_t*) ep->ptype;
			rv = prov->open_rd(ep, tfd, buf + FSP_DATA + 1);
			retbuf[FSP_DATA] = rv;
			if (rv == 0) {
				set_chan(tfd, ep);
			}
		}
		break;
	case FS_OPEN_AP:
		ep = provider_lookup(buf[FSP_DATA]);
		if (ep != NULL) {
			prov = (provider_t*) ep->ptype;
			rv = prov->open_ap(ep, tfd, buf + FSP_DATA + 1);
			retbuf[FSP_DATA] = rv;
			if (rv == 0) {
				set_chan(tfd, ep);
			}
		}
		break;
	case FS_READ:
		ep = chan_to_endpoint(tfd);
		if (ep != NULL) {
			eof = 0;	// default just in case
			prov = (provider_t*) ep->ptype;
			rv = prov->readfile(ep, tfd, retbuf + FSP_DATA, MAX_BUFFER_SIZE-FSP_DATA, &eof);
			retbuf[FSP_LEN] = FSP_DATA + rv;
			if (eof) {
				//printf("fscmd: setting EOF\n");
				retbuf[FSP_CMD] = FS_EOF;
			}
		}
		break;
	case FS_WRITE:
	case FS_EOF:
		ep = chan_to_endpoint(tfd);
		//printf("WRITE: chan=%d, ep=%p\n", tfd, ep);
		if (ep != NULL) {
			prov = (provider_t*) ep->ptype;
			rv = prov->writefile(ep, tfd, buf+FSP_DATA, len-FSP_DATA, cmd == FS_EOF);
			retbuf[FSP_DATA] = rv;
		}
		break;
	case FS_CLOSE:
		ep = chan_to_endpoint(tfd);
		if (ep != NULL) {
			prov = (provider_t*) ep->ptype;
			prov->close(ep, tfd);
			free_chan(tfd);
			retbuf[FSP_DATA] = 0;
		}
		break;

		// command operations
	case FS_DELETE:
		ep = provider_lookup(buf[FSP_DATA]);
		if (ep != NULL) {
			prov = (provider_t*) ep->ptype;
			if (prov->scratch != NULL) {
				rv = prov->scratch(ep, buf+FSP_DATA+1, &outdeleted);
				if (rv == 1) {
					retbuf[FSP_DATA + 1] = outdeleted > 99 ? 99 :outdeleted;
					retbuf[FSP_LEN] = FSP_DATA + 2;
				}
				retbuf[FSP_DATA] = rv;
			}
		}
		break;
	case FS_RENAME:
		ep = provider_lookup(buf[FSP_DATA]);
		if (ep != NULL) {
			prov = (provider_t*) ep->ptype;
			if (prov->rename != NULL) {
				rv = prov->rename(ep, buf+FSP_DATA+1);
				retbuf[FSP_DATA] = rv;
			}
		}
		break;
	case FS_CHDIR:
		ep = provider_lookup(buf[FSP_DATA]);
		if (ep != NULL) {
			prov = (provider_t*) ep->ptype;
			if (prov->cd != NULL) {
				rv = prov->cd(ep, buf+FSP_DATA+1);
				retbuf[FSP_DATA] = rv;
			}
		}
		break;
	case FS_MKDIR:
		ep = provider_lookup(buf[FSP_DATA]);
		if (ep != NULL) {
			prov = (provider_t*) ep->ptype;
			if (prov->mkdir != NULL) {
				rv = prov->mkdir(ep, buf+FSP_DATA+1);
				retbuf[FSP_DATA] = rv;
			}
		}
		break;
	case FS_RMDIR:
		ep = provider_lookup(buf[FSP_DATA]);
		if (ep != NULL) {
			prov = (provider_t*) ep->ptype;
			if (prov->rmdir != NULL) {
				rv = prov->rmdir(ep, buf+FSP_DATA+1);
				retbuf[FSP_DATA] = rv;
			}
		}
		break;
	case FS_ASSIGN:
		// assign an endpoint number (i.e. a drive number for the PET)
		// to a filesystem provider and path
		// The drive number in buf[FSP_DATA] is the one to assign,
		// while the rest of the name determines which provider to use
		//
		// A provider can be determines relative to an existing one. In 
		// this case the provider name is the endpoint (drive) number,
		// and the path is interpreted as relative to an existing endpoint.
		// If the provider name is a real name, the path is absolute.
		//
		rv = provider_assign(buf[FSP_DATA], buf+FSP_DATA+1);
		retbuf[FSP_DATA] = rv;
		break;
	}


	int e = write(fd, retbuf, retbuf[FSP_LEN]);
	if (e < 0) {
		printf("Error on write: %d\n", errno);
	}
#if defined DEBUG_WRITE || defined DEBUG_CMD
	printf("write %02x %02x %02x:", retbuf[0], retbuf[1],
			retbuf[2] );
	for (int i = 3; i<retbuf[FSP_LEN];i++) printf(" %02x", retbuf[i]);
	printf("\n");
#endif

}


