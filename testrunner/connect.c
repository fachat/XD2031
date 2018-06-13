
/****************************************************************************

    xd2031 filesystem server - socket test runner
    Copyright (C) 2012,2014 Andre Fachat

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

#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>

#include "log.h"


int socket_open(const char *socketname, int dowait) {

   	int sockfd, servlen;
   	struct sockaddr_un  server_addr;
	struct timespec sleeptime;

   	log_info("Connecting to socket %s\n", socketname);

   	memset((char *)&server_addr, 0, sizeof(server_addr));
   	server_addr.sun_family = AF_UNIX;
   	strcpy(server_addr.sun_path, socketname);
   	servlen = strlen(server_addr.sun_path) + 
                 sizeof(server_addr.sun_family);
   	if ((sockfd = socket(AF_UNIX, SOCK_STREAM,0)) < 0) {
       		log_error("Creating socket");
		return -1;
   	}
   	while (connect(sockfd, (struct sockaddr *) 
                         &server_addr, servlen) < 0) {
		if (errno != ENOENT || !dowait) {
       			log_errno("Connecting: ");
			log_error("Terminating\n");
   			return -1;
		}
		sleeptime.tv_sec = 0;
		sleeptime.tv_nsec = 100000000l;	// 100 ms
		nanosleep(&sleeptime, NULL);
   	}
   	return sockfd;
}



