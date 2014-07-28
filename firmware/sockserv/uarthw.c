
/***************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2012 Andre Fachat

    Inspired by uart.c from XS-1541, but rewritten in the end.

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

****************************************************************************/

#include <inttypes.h>
#include <stdbool.h>

#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>

#include "debug.h"
#include "uarthw.h"

#define LOG_PREFIX 	"s488_uart "

static const char *socket_name = NULL;
static int socket_fd = -1;

void uarthw_set_socket(const char *socketname) {
	socket_name = socketname;
}

static int client_socket_open(const char *socketname, int dowait) {

        int sockfd, servlen;
        struct sockaddr_un  server_addr;
        struct timespec sleeptime;

        printf(LOG_PREFIX "Connecting to socket %s\n", socketname);

        memset((char *)&server_addr, 0, sizeof(server_addr));
        server_addr.sun_family = AF_UNIX;
        strcpy(server_addr.sun_path, socketname);
        servlen = strlen(server_addr.sun_path) +
                 sizeof(server_addr.sun_family);
        if ((sockfd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK,0)) < 0) {
                printf(LOG_PREFIX "Creating socket");
                return -1;
        }
        while (connect(sockfd, (struct sockaddr *)
                         &server_addr, servlen) < 0) {
                if (errno != ENOENT || !dowait) {
                        printf(LOG_PREFIX "Connecting: %d (%s)", errno, strerror(errno));
                        return -1;
                }
                sleeptime.tv_sec = 0;
                sleeptime.tv_nsec = 100000000l; // 100 ms
                nanosleep(&sleeptime, NULL);
        }
        return sockfd;
}


/**
 * interface from serial code to uart low level interrupt driver
 */


/**
 * initialize the UART 
 */
void uarthw_init() {

	if (socket_name != NULL) {

		int fd = client_socket_open(socket_name, 1);

		if (fd >= 0) {
			socket_fd = fd;
		} else {
			printf(LOG_PREFIX "Could not open socket!\n");
		}
	} else {
		printf(LOG_PREFIX "No socket name given!\n");
	}
	if (socket_fd < 0) {
		printf(LOG_PREFIX "Terminating!\n");
		exit (-3);
	}
}

/**
 * when returns true, there is space in the uart send buffer,
 * so uarthw_send(...) can be called with a byte to send
 */
int8_t uarthw_can_send() {
	return true;
}

/**
 * submit a byte to the send buffer
 */
void uarthw_send(int8_t data) {
        ssize_t wsize = write(socket_fd, &data, 1);
        while (wsize < 0 && errno == EAGAIN) {
                // wait 10ms
                struct timespec sleeptime = { 0, 10000000l };
                nanosleep(&sleeptime, NULL);
                wsize = write(socket_fd, &data, 1);
        }

	if (wsize == 0) {
		printf(LOG_PREFIX "Could not write to fd=%d, data=%02x\n", socket_fd, data);
	} else
	if (wsize < 0) { 
		printf(LOG_PREFIX "Error writing to server on fd %d: errno=%d (%s)\n", socket_fd, errno, strerror(errno));
	}
}

/**
 * receive a byte from the receive buffer
 * Returns -1 when no byte available
 */
int16_t uarthw_receive() {

	uint8_t data;

	if (socket_fd < 0) {
		return -1;
	}

	ssize_t rsize = read(socket_fd, &data, 1);
	if (rsize == 0) {
		printf(LOG_PREFIX "End of file on socket!\n");
		exit(-1);
	} else
	if (rsize < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			// ok, just no data currently available
			return -1;
		}
		printf(LOG_PREFIX "Unrecoverable error on read ->%d (%s)\n", errno, strerror(errno));
		exit(-2);
	}

	return ((int16_t)data) & 0xff;
}


