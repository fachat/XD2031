/****************************************************************************

    Socket filesystem server
    Copyright (C) 2014 Andre Fachat

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



#include "os.h"

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>


/**
 * open a named unix socket, listen on it and return the first connection
 */
int socket_open(const char *socketname) {

	log_info("Opening socket %s for requests\n", socketname);

	int sockfd, clientfd;
	socklen_t servlen, clientlen;
	struct sockaddr_un client_addr, server_addr;
	
	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd < 0) {
		log_error("Error opening socket\n");
		return -1;
	}

	memset((char *) &server_addr, 0, sizeof(server_addr));
   	server_addr.sun_family = AF_UNIX;
   	strcpy(server_addr.sun_path, socketname);
   	servlen=strlen(server_addr.sun_path) + 
                     sizeof(server_addr.sun_family);
   	if(bind(sockfd,(struct sockaddr *)&server_addr,servlen)<0) {
       		log_error("Error binding socket"); 
		close(sockfd);
		return -1;
	}

   	listen(sockfd,0);
   
	clientlen = sizeof(client_addr);
   	clientfd = accept(sockfd,(struct sockaddr *)&client_addr,&clientlen);
   	if (clientfd < 0) {
       		log_error("Error accepting socket"); 
		close(sockfd);
		return -1;
	}

	return clientfd;
}



