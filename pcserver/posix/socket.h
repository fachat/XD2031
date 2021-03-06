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

/**
 * open a named unix socket, listen on it and return the first connection
 */
int socket_open(const char *socketname);

/**
 * open a named socket and listen on it; do not accept (yet)
 */
int socket_listen(const char *socketname);

/**
 * try to accept a connection on a socket
 * return -1 on error or the socket fd
 */
int socket_accept(int sockfd);


