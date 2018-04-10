/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2018 Andre Fachat

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

#ifndef IN_DEVICE_H
#define IN_DEVICE_H

typedef struct {
	serial_port_t readfd;
	serial_port_t writefd;
	int wrp;
	int rdp;
	char buf[8192];
} in_device_t;

in_device_t *in_device_init(int readfd, int writefd, int do_reset);

/**
 *
 * Here the data is read from the given readfd, put into a packet buffer,
 * then given to cmd_dispatch() for the actual execution, and the reply is 
 * again packeted and written to the writefd
 *
 * returns
 *   2 if read fails (errno gives more information)
 *   1 if no data has been read
 *   0 if processing succeeded
 */
int in_device_loop(in_device_t *tp);


#endif
