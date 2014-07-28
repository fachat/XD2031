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
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "bus.h"
#include "sock488.h"
#include "socket.h"


#undef	S488_DEBUG


static bus_t sock488_bus;

static const char *socket_name = NULL;
static int socket_fd = -1;

void sock488_set_socket(const char *socketname) {

	socket_name = socketname;
}

void sock488_init() {

	bus_init_bus("sock488", &sock488_bus);
	sock488_bus.active = 1;

	if (socket_name == NULL) {
		printf("No client socket name given - terminating!\n");
		exit(-3);
	}

	socket_fd = socket_open(socket_name);

	if (socket_fd < 0) {
		printf("Error opening sock488 socket\n");
		exit(-1);
	}
}

/**
 * submit a byte to the send buffer
 */
static void send_byte(int8_t data) {
 	ssize_t wsize = write(socket_fd, &data, 1);
	while (wsize < 0 && errno == EAGAIN) {
		// wait 10ms
		struct timespec sleeptime = { 0, 10000000l };
		nanosleep(&sleeptime, NULL);
 	       	wsize = write(socket_fd, &data, 1);
	}

       	if (wsize == 0) {
               printf("Could not write to fd=%d, data=%02x\n", socket_fd, data);
       	} else
       	if (wsize < 0) {
               printf("Error writing %02x to fd %d: errno=%d (%s)\n", data, socket_fd, errno, strerror(errno));
        }
}

static char read_byte() {
	char data;

	ssize_t rsize = read(socket_fd, &data, 1);
	if (rsize == 0) {
                printf("End of file on s488 socket (fd=%d)!\n", socket_fd);
                exit(-1);
        } else
        if (rsize < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // ok, just no data currently available
                        return -1;
                }
                printf("Unrecoverable error on read ->%d (%s)\n", errno, strerror(errno));
                exit(-2);
        }

	return data;
}

/**
 * this is called from the main loop. It has to check whether we get some
 * data from the socket and feeds it to the bus_* methods:
 *	bus_attention()
 * 	bus_sendbyte()
 * It also offers bytes from
 *	bus_receivebyte()
 * to the server and acknowledges it
 */
void sock488_mainloop_iteration() {

	int16_t par_status = 0;

	uint8_t tmp = 0;
	uint8_t indata = read_byte();
	uint8_t data = 0;

	uint8_t eof = indata & S488_EOF;
	uint8_t tout = indata & S488_TIMEOUT;
	uint8_t ack = indata & S488_ACK;
	indata &= ~(S488_EOF | S488_ACK);

	switch (indata) {

		case S488_ATN:
			data = read_byte();
			bus_attention(&sock488_bus, data);
			break;
		case S488_SEND:
			data = read_byte();
			bus_sendbyte(&sock488_bus, data, eof ? BUS_FLUSH : 0);
			break;
		case S488_REQ:
			if (ack) {
				// acknowledge old byte
				bus_receivebyte(&sock488_bus, &tmp, 0);
			}

			eof = 0;
			tout = 0;

			par_status = bus_receivebyte(&sock488_bus, &data, BUS_PRELOAD);
#ifdef S488_DEBUG
			printf("got: %02x, e=%02x, a=%02x, t=%02x -> par_status=%04x, data=%02x\n", indata, eof, ack, tout, par_status, data);
#endif
			if (par_status & STAT_RDTIMEOUT) {
				tout = S488_TIMEOUT;
				send_byte(S488_OFFER | tout);
			} else {
				if (par_status & STAT_EOF) {
					eof = S488_EOF;
				}
				send_byte(S488_OFFER | eof );
				send_byte(data);
			}
			break;
		case 0:
			if (ack) {
				// acknowledge old byte
				bus_receivebyte(&sock488_bus, &tmp, 0);
			}
			break;
		default:
			break;
	}

}





