/****************************************************************************

    Async poll 
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

/**
 * init data structures
 */
void poll_init(void);

/**
 * register a listen socket, and an action to call on accept
 * data is a void pointer to a data struct given to the function
 */
void poll_register_accept(int fd, void *data, 
				void (*accept)(int fd, void *data), 
				void (*hup)(int fd, void *data)
);

/**
 * register a read/write socket, with actions to call when socket can be read/written
 */
void poll_register_readwrite(int fd, void *data, 
				void (*read)(int fd, void *data), 
				void (*write)(int fd, void *data), 
				void (*hup)(int fd, void *data)
);

/**
 * unregister socket
 */
void poll_unregister(int fd);


/**
 * loop until timeout without activity
 * return 0 when timeout
 * return <0 when no file descriptor left
 */
int poll_loop(int timeoutMs);

/**
 * close everything
 */
void poll_shutdown();

