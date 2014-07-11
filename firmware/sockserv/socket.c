/*
    Titel:	 Socket server 
    Copyright (C) 2014  Andre Fachat <afachat@gmx.de>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
    MA  02110-1301, USA.
*/


#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "socket.h"

/**
 * open a named unix socket, listen on it and return the first connection
 *
 * TODO: unify with pcserver/socket.c
 */
int socket_open(const char *socketname) {

        printf("Opening socket %s for requests\n", socketname);

        int sockfd, clientfd;
        socklen_t servlen, clientlen;
        struct sockaddr_un client_addr, server_addr;

        sockfd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (sockfd < 0) {
                printf("Error opening socket\n");
                return -1;
        }

        memset((char *) &server_addr, 0, sizeof(server_addr));
        server_addr.sun_family = AF_UNIX;
        strcpy(server_addr.sun_path, socketname);
        servlen=strlen(server_addr.sun_path) +
                     sizeof(server_addr.sun_family);
        if(bind(sockfd,(struct sockaddr *)&server_addr,servlen)<0) {
                printf("Error binding socket\n");
                close(sockfd);
                return -1;
        }

        listen(sockfd,0);

        clientlen = sizeof(client_addr);
        clientfd = accept(sockfd,(struct sockaddr *)&client_addr,&clientlen);
        while (clientfd < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			struct timespec waittime;
			waittime.tv_sec = 0;
			waittime.tv_nsec = 1000l * 1000l * 100;	// 100ms
			nanosleep(&waittime, NULL);
        		clientfd = accept(sockfd,(struct sockaddr *)&client_addr,&clientlen);
		} else {
                	printf("Error accepting socket, errno=%d (%s)\n", errno, strerror(errno));
                	close(sockfd);
                	return -1;
		}
        }

        return clientfd;
}



