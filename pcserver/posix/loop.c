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

#include <poll.h>
#include <unistd.h>

#include "mem.h"
#include "log.h"
#include "registry.h"
#include "loop.h"

static registry_t poll_list;
static struct pollfd *poll_pars = NULL;
static int update_needed = 0;

typedef struct {
	int 	fd;
	void 	*data;
	void	(*accept)(int fd, void *data);
	void	(*read)(int fd, void *data);
	void 	(*write)(int fd, void *data);
	void 	(*hup)(int fd, void *data);
} poll_info_t;

static void poll_info_init(const type_t *type, void *obj) {
	(void) type;
	poll_info_t *pinfo = (poll_info_t*) obj;

	pinfo->fd = -1;
	pinfo->data = NULL;

	pinfo->accept = NULL;
	pinfo->read = NULL;
	pinfo->write = NULL;
	pinfo->hup = NULL;
}

static type_t poll_info_type = {
	"poll_info",
	sizeof(poll_info_t),
	poll_info_init
};

static type_t poll_pars_type = {
	"pollfd",
	sizeof(struct pollfd),
	NULL
};

/**
 * init data structures
 */
void poll_init(void) {

	poll_pars = NULL;
	reg_init(&poll_list, "poll_list", 10);

	update_needed = 1;
}

static void update_poll_list() {

	if (!update_needed) {
		return;
	}
	update_needed = 0;

	// remove old poll() parameters
	if (poll_pars != NULL) {
		mem_free(poll_pars);
		poll_pars = NULL;
	}

	// purge closed entries from poll_list
	int len = reg_size(&poll_list);
	for (int i = 0; i < len; ) {
		poll_info_t *pinfo = reg_get(&poll_list, i);

		if (pinfo->fd < 0) {
			// purge
			reg_remove_pos(&poll_list, i);
			len--;
		} else {
			i++;
		}
	}

	// create new poll_list from remaining entries
	poll_pars = mem_alloc_n(len, &poll_pars_type);

	for (int i = 0; i < len; i++) {

		poll_info_t *pinfo = reg_get(&poll_list, i);
		if (pinfo != NULL) {
			short events = 0;
			if (pinfo->accept) {
				events |= POLLIN;
			}
			if (pinfo->read) {
				events |= POLLIN;
			}
			if (pinfo->write) {
				events |= POLLOUT;
			}
			poll_pars[i].events = events;
			poll_pars[i].fd = pinfo->fd;

		} else {
			//
			log_warn("Did not expect pinfo to be NULL!\n");
		}
	}
	log_info("Create poll list with %d entries\n", len);
}

/**
 * register a listen socket, and an action to call on accept
 * data is a void pointer to a data struct given to the function
 */
void poll_register_accept(int fd, void *data, 
				void (*accept)(int fd, void *data), 
				void (*hup)(int fd, void *data)
	) {

	log_debug("poll_register_accept(fd=%d, data=%p)\n", fd, data);

	poll_info_t *pinfo = mem_alloc(&poll_info_type);

	pinfo->fd = fd;
	pinfo->data = data;

	pinfo->accept = accept;
	pinfo->hup = hup;

	reg_append(&poll_list, pinfo);

	update_needed = 1;
}

/**
 * register a read/write socket, with actions to call when socket can be read/written
 */
void poll_register_readwrite(int fd, void *data, 
				void (*read)(int fd, void *data), 
				void (*write)(int fd, void *data), 
				void (*hup)(int fd, void *data)
	) {

	log_debug("poll_register_readwrite(fd=%d, data=%p)\n", fd, data);

	poll_info_t *pinfo = mem_alloc(&poll_info_type);

	pinfo->fd = fd;
	pinfo->data = data;

	pinfo->read = read;
	pinfo->write = write;
	pinfo->hup = hup;

	reg_append(&poll_list, pinfo);

	update_needed = 1;
}

/**
 * unregister socket
 */
void poll_unregister(int fd) {

        log_debug("poll_unregister: Removing entry for fd %d from registry %p (%s, size=%d)\n", fd, &poll_list, poll_list.name, poll_list.numentries);

	int n = reg_size(&poll_list);

        for (int i = 0; i < n; i++) {
		poll_info_t *pinfo = (poll_info_t*) reg_get(&poll_list, i);
                if (pinfo != NULL && pinfo->fd == fd) {
			pinfo->fd = -fd;
			update_needed = 1;
                        return;
                }
        }
        log_error("poll_unregister: Unable to remove entry for fd %d from registry %p (%s)\n", fd, &poll_list, poll_list.name);
}

/**
 * return 0 when timeout
 * return <0 when no file descriptor left
 */
int poll_loop(int timeoutMs) {
	
	int nfds = 0;
	int n = 0;
	int fd = 0;

	do {
		update_poll_list();

		nfds = reg_size(&poll_list);

		if (nfds == 0) {
			return -1;
		}
	
		n = poll(poll_pars, nfds, timeoutMs);

		for (int i = 0; i < nfds; i++) {

			if (poll_pars[i].revents) {
				poll_info_t *pinfo = reg_get(&poll_list, i);
				fd = poll_pars[i].fd;
	
				if (poll_pars[i].revents & POLLIN) {

					if (pinfo->accept) {
						pinfo->accept(fd, pinfo->data);
					} else
					if (pinfo->read) {
						pinfo->read(fd, pinfo->data);
					} else {
						log_error("unexpected POLLIN on fd %d\n", fd);
					}
				}
				if (poll_pars[i].revents & POLLOUT) {

					if (pinfo->write) {
						pinfo->write(fd, pinfo->data);
					} else {
						log_error("unexpected POLLOUT on fd %d\n", fd);
					}
				}
				if (poll_pars[i].revents & (POLLHUP | POLLERR | POLLNVAL)) {
			
					if (pinfo->hup) {
						pinfo->hup(fd, pinfo->data);
					} else {
						log_error("unexpected POLLERR/HUP/NVAL on fd %d\n", fd);
						close(fd);
						pinfo->fd = -fd;
					}
				}
				n--;
			}
		}
	} while (n > 0);

	return 0;	
}

void poll_shutdown() {

	log_info("poll_shutdown()\n");

	update_poll_list();

	int nfds = reg_size(&poll_list);

	for (int i = 0; i < nfds; i++) {
		poll_info_t *pinfo = reg_get(&poll_list, i);

		if (pinfo->hup) {
			pinfo->hup(pinfo->fd, pinfo->data);
		} else {
			close(pinfo->fd);
		}
		pinfo->fd = -pinfo->fd;
	}

	update_poll_list();
}

