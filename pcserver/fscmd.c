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

#define DEBUG_READ
#define DEBUG_WRITE

#define	MAX_BUFFER_SIZE	64

extern provider_t fs_provider;


void cmd_init() {
//	int i;
//        for(i=0;i<MAXFILES;i++) {
//          files[i].state = F_FREE;
//        }

	fs_provider.init();
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

	// not on FS_TERM
	tfd = buf[FSP_FD];
	if (tfd < 0 || tfd >= MAXFILES) {
		printf("Illegal file descriptor: %d\n", tfd);
		return;
	}

	buf[(unsigned int)buf[FSP_LEN]] = 0;	// 0-terminator

	provider_t *prov = &fs_provider;

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
		rv = prov->open(tfd, buf + FSP_DATA, "wb");
		retbuf[FSP_DATA] = rv;
		break;
	case FS_OPEN_DR:
		rv = prov->opendir(tfd, buf + FSP_DATA);
		retbuf[FSP_DATA] = rv;
		break;
	case FS_OPEN_RD:
		rv = prov->open(tfd, buf + FSP_DATA, "rb");
		retbuf[FSP_DATA] = rv;
		break;
	case FS_OPEN_AP:
		rv = prov->open(tfd, buf + FSP_DATA, "ab");
		retbuf[FSP_DATA] = rv;
		break;
	case FS_READ:
		rv = prov->readfile(tfd, retbuf + FSP_DATA, MAX_BUFFER_SIZE, &eof);
		retbuf[FSP_LEN] = FSP_DATA + rv;
		if (eof) {
			retbuf[FSP_CMD] = FS_EOF;
		}
		break;
	case FS_WRITE:
	case FS_EOF:
		rv = prov->writefile(tfd, buf, len, cmd == FS_EOF);
		retbuf[FSP_DATA] = rv;
		break;
	case FS_CLOSE:
		prov->close(tfd);
		retbuf[FSP_DATA] = 0;
		break;

		// command operations
	case FS_DELETE:
		rv = prov->scratch(buf, &outdeleted);
		if (rv == 1) {
			retbuf[FSP_DATA + 1] = outdeleted > 99 ? 99 :outdeleted;
			retbuf[FSP_LEN] = FSP_DATA + 2;
		}
		retbuf[FSP_DATA] = rv;
		break;
	}


	int e = write(fd, retbuf, retbuf[FSP_LEN]);
	if (e < 0) {
		printf("Error on write: %d\n", errno);
	}
#ifdef DEBUG_WRITE
	printf("write %02x %02x %02x:", retbuf[0], retbuf[1],
			retbuf[2] );
	for (int i = 3; i<retbuf[FSP_LEN];i++) printf(" %02x", retbuf[i]);
	printf("\n");
#endif

}


