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
 *   fstcp [options] exported_directory
 *
 *   options:
 * 	-ro	export read-only
 */

#define	PORT	8090

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <netdb.h>

/*
#include <sys/types.h>
#include <sys/socket.h>

#include <sys/stat.h>
#include <dirent.h>

#include <netinet/in.h>

#include <time.h>
*/

#include "fscmd.h"
#include "privs.h"

void usage(void) {
	printf("Usage: fsmux [options] exported_directory hostname_to_export_to\n"
		" options=\n"
		"   -ro		export read-only\n"
		"   -ro		export read-only\n"
	);
	exit(1);
}

int main(int argc, char *argv[]) {
	int sock, err;
	struct sockaddr_in serv_addr, client_addr, host_addr;
	socklen_t client_addr_len;
	int port=PORT;
	int fd;
	int i, ro=0;
	char *dir, *hname;
	struct hostent *he;

	i=1;
	while(i<argc && argv[i][0]=='-') {
	  switch(argv[i][1]) {
	    case '?':
		usage();
		break;
	    case 'r':
		if(argv[i][2]=='o') {
		  ro=1;
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

	hname= argv[i++];
	printf("hostname=%s\n",hname);

	he = gethostbyname(hname);
	if(!he) {
	  fprintf(stderr, "Could not get hostinfo for %s, h_errno=%d\n",
			hname, h_errno);
	  exit(2);
	}
	printf("official name is %s\n",he->h_name);
	if(he->h_addrtype != AF_INET) {
	  fprintf(stderr, "Address type for %s not Internet!\n", hname);
	  exit(2);
	}

	memcpy((char*)&host_addr.sin_addr.s_addr, he->h_addr_list[0], 
							he->h_length);

/*	host_addr.sin_addr.s_addr = ntohl(*(long*)(he->h_addr_list[0]));*/

	printf("ok, want connection to %08x\n", host_addr.sin_addr.s_addr);


	printf("port=%d\n",port);
	
	sock = socket(AF_INET, SOCK_STREAM, 0);

	printf("sock=%d\n",sock);

	serv_addr.sin_family=AF_INET;
	serv_addr.sin_port=htons(port);
	serv_addr.sin_addr.s_addr=htonl(INADDR_ANY);

	err = bind(sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	if(err<0) {
	  fprintf(stderr, "Could not bind (errno=%d, %s)!\n", errno,
							strerror(errno));
	  return 2;
	}

	err = listen(sock, 1);
	if(err<0) {
	  fprintf(stderr, "Could not listen!\n");
	  return 2;
	}

	// now we have enough set up to drop privileges
	drop_privileges();

	/* note that this may probably belong into the accept branch (e.g.
	 * if we would clone() after accept */
	cmd_init();

	while(1) {
	  client_addr_len = sizeof(client_addr);
	  fd = accept(sock,(struct sockaddr*)&client_addr, &client_addr_len);
	  if(fd<0) {
	    fprintf(stderr, "Could not accept, errno=%d (%s)!\n",
						errno, strerror(errno));
	    return 2;
	  }
	  printf("accept request from %08x, port %d, clen=%d, inal=%d\n", 
			client_addr.sin_addr.s_addr, client_addr.sin_port,
			client_addr_len, sizeof(struct in_addr));

	  if(!memcmp(&client_addr.sin_addr.s_addr, 
			&host_addr.sin_addr.s_addr, sizeof(struct in_addr))) {

	    close(sock);

	    printf("ok, got connection to %04x, port %d\n", 
			client_addr.sin_addr.s_addr, client_addr.sin_port);

	    cmd_loop(fd, fd);

	    exit(0);
	  } else {
	    printf("connect trial rejected!\n");
	    close(fd);
	  }
	}
	
	close(sock);
	return 0;	
}

