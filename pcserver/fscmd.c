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

#include "wireformat.h"
#include "fscmd.h"
#include "dir.h"

#define	MAX_BUFFER_SIZE	64

//#define	min(a,b)	(((a)<(b))?(a):(b))

typedef struct {
	int		state;		/* note: currently not really used */
	FILE		*fp;
	DIR 		*dp;
	char		dirpattern[MAX_BUFFER_SIZE];
	struct dirent	*de;
	unsigned int	is_first :1;	// is first directory entry?
} File;

File files[MAXFILES];


void cmd_init() {
	int i;
        for(i=0;i<MAXFILES;i++) {
          files[i].state = F_FREE;
        }
}

void cmd_loop(int readfd, int writefd) {

        char buf[8192];
        int wrp,rdp,plen;
        int n;

            /* write and read pointers in the input buffer "buf" */
        wrp = rdp = 0;

        while((n=read(readfd, buf+wrp, 8192-wrp))!=0) {
//printf("read %d bytes: ",n);
//for(int i=0;i<n;i++) printf("%02x ",buf[wrp+i]); printf("\n");

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
//printf("wrp-rdp=%d, plen=%d\n",wrp-rdp,plen);
                // full packet received already?
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

/**
 * This function executes a packet from the device.
 * The return data is written into the file descriptor fd
 */
void do_cmd(char *buf, int fd) {
	int tfd, cmd;
	unsigned int len;
	char retbuf[200];
	char *nm;
	FILE *fp;
	DIR *dp;
	int n;

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
	fp = files[tfd].fp;
	dp = files[tfd].dp;

	buf[(unsigned int)buf[FSP_LEN]] = 0;	// 0-terminator
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

	switch(cmd) {
	case FS_OPEN_WR:
		/* no directory separators - security rules! */
		nm = (char*)buf+FSP_DATA;
		if(*nm=='/') nm++;
		if(strchr(nm, '/')) break;

		fp = open_first_match(nm, "wb");
printf("OPEN_WD(%s)=%p\n",buf+FSP_DATA,fp);
		if(fp) {
		  files[tfd].fp = fp;
		  files[tfd].dp = NULL;
		  retbuf[FSP_DATA] = 0;
		}
		break;
	case FS_OPEN_DR:
		// save pattern for later comparisons
		strcpy(files[tfd].dirpattern, buf+FSP_DATA);
		dp = opendir("." /*buf+FSP_DATA*/);
printf("OPEN_DR(%s)=%p\n",buf+FSP_DATA,dp);
		if(dp) {
		  files[tfd].fp = NULL;
		  files[tfd].dp = dp;
		  retbuf[FSP_DATA] = 0;
		  files[tfd].is_first = 1;
		}
		break;
	case FS_OPEN_RD:
		/* no directory separators - security rules! */
		if(strchr((char*)buf+FSP_DATA, '/')) break;

		fp = open_first_match((char*)buf+FSP_DATA, "rb");
printf("OPEN_RD(%s)=%p\n",buf+FSP_DATA,fp);
		if(fp) {
		  files[tfd].fp = fp;
		  files[tfd].dp = NULL;
		  retbuf[FSP_DATA] = 0;
		}
		break;
	case FS_READ:
		// ------------------------------------
		// read directory entries
		if(dp) {
		  if (files[tfd].is_first) {
		    files[tfd].is_first = 0;
		    int l = dir_fill_header(retbuf+FSP_DATA, 0, files[tfd].dirpattern);
		    retbuf[FSP_LEN] = FSP_DATA + l;
		    retbuf[FSP_CMD] = FS_WRITE;
		    files[tfd].de = dir_next(files[tfd].dp, files[tfd].dirpattern);
		    break;
		  }
		  if(!files[tfd].de) {
		    closedir(dp);
		    files[tfd].dp = NULL;
		    retbuf[FSP_CMD] = FS_EOF;
		    int l = dir_fill_disk(retbuf + FSP_DATA);
		    retbuf[FSP_LEN] = FSP_DATA + l;
		    break;
		  }
		  int l = dir_fill_entry(retbuf + FSP_DATA, files[tfd].de, MAX_BUFFER_SIZE-FSP_DATA);
		  retbuf[FSP_LEN] = FSP_DATA + l;
		  retbuf[FSP_CMD] = FS_WRITE;
		  // prepare for next read (so we know if we're done)
		  files[tfd].de = dir_next(files[tfd].dp, files[tfd].dirpattern);
		} else
		// ------------------------------------
		// read file data
		if(fp) {
		  n = fread(retbuf+FSP_DATA, 1, MAX_BUFFER_SIZE, fp);
		  retbuf[FSP_LEN] = n+FSP_DATA;
		  if(n<MAX_BUFFER_SIZE) {
		    retbuf[FSP_CMD] = FS_EOF;
		    fclose(fp);
		    files[tfd].fp = NULL;
		  } else {
		    // as feof() does not let us know if the file is EOF without
		    // having attempted to read it first, we need this kludge
		    int eofc = fgetc(fp);
		    if (eofc < 0) {
		      // EOF
		      retbuf[FSP_CMD] = FS_EOF;
		    } else {
		      // restore fp, so we can read it properly on the next request
		      ungetc(eofc, fp);
		      // do not send EOF
		      retbuf[FSP_CMD] = FS_WRITE;
		    }
		  }
		}
		break;
	case FS_WRITE:
	case FS_EOF:
		if(fp) {
		  if (len > FSP_DATA) {
		    // TODO: evaluate return value
		    n = fwrite(buf+FSP_DATA, 1, len-FSP_DATA, fp);
		  }
		  retbuf[FSP_DATA] = 0;
		  if(cmd == FS_EOF) {
		    fclose(fp);
		    files[tfd].fp = NULL;
		  }
		}
		break;
	case FS_CLOSE:
		if(fp) fclose(fp);
		if(dp) closedir(dp);
		files[tfd].fp = NULL;
		files[tfd].dp = NULL;
		retbuf[FSP_DATA] = 0;
		break;
	}


	write(fd, retbuf, retbuf[FSP_LEN]);
#if 0
	printf("write %02x %02x %02x:", retbuf[0], retbuf[1],
			retbuf[2] );
	for (int i = 3; i<retbuf[FSP_LEN];i++) printf(" %02x", retbuf[i]);
	printf("\n");
#endif

}


