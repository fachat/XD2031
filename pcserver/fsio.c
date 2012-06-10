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
 * usage: 
 *   fsser [options] exported_directory
 *
 *   options:
 * 	-ro	export read-only
 * 	-d <device>  determine device (if none, use stdin/stdout)
 */

#include <stdio.h>

#include "fscmd.h"
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>


/*
#include <sys/types.h>
#include <sys/socket.h>

#include <sys/stat.h>
#include <dirent.h>

#include <netinet/in.h>
#include <netdb.h>

#include <time.h>

*/

void usage(void) {
	printf("Usage: fsser [options] exported_directory\n"
		" options=\n"
		"   -ro		export read-only\n"
		"   -d <device>	define serial device to use\n"
	);
	exit(1);
}

int main(int argc, char *argv[]) {
	int writefd, readfd;
	FILE *fdesc;
	int i, ro=0;
	char *dir;
	char *device = NULL;	/* device name or NULL if stdin/out */

	i=1;
	while(i<argc && argv[i][0]=='-') {
	  switch(argv[i][1]) {
	    case '?':
		usage();	/* usage() exits already */
		break;
	    case 'r':
		if(argv[i][2]=='o') {
		  ro=1;
		}
	    case 'd':
		if (i < argc-2) {
		  i++;
		  device = argv[i];
		}
 	     	break;
	  }
	  i++;
	}

	if(i!=argc-2) {
	  usage();
	}

	dir = argv[i++];
	printf("dir=%s\n",dir);

	if(chdir(dir)<0) { 
	  fprintf(stderr, "Couldn't change to directory %s, errno=%d (%s)\n",
			dir, errno, strerror(errno));
	  exit(1);
	}

	if (device != NULL) {
		fdesc = fopen(device, "a+");
		if (fdesc == NULL) {
		  /* error */
		  fprintf(stderr, "Could not open device %s, errno=%d (%s)\n", 
			device, errno, strerror(errno));
		  exit(1);
		}
		readfd = fileno(fdesc);
		writefd = readfd;
	}

	cmd_init();

	cmd_loop(readfd, writefd);

	if (device != NULL) {
		fclose(fdesc);
	}

	return 0;	
}

