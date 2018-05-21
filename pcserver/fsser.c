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
 *   fsser [options] exported_directory
 *
 *   options see the usage string below
 */

#include "os.h"

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>

#include "charconvert.h"
#include "in_device.h"
#include "in_ui.h"
#include "cmd.h"
#include "privs.h"
#include "log.h"
#include "provider.h"
#include "mem.h"
#include "serial.h"
#ifndef _WIN32
#  include "socket.h"
#endif
#include "terminal.h"
#include "dir.h"
#include "loop.h"

// --------------------------------------------------------------------------------------

void usage(int rv) {
	printf("Usage: fsser [options] run_directory\n"
		" options=\n"
                "   -A<drv>:<provider-string>\n"
                "               assign a provider to a drive\n"
                "               e.g. use '-A0:fs=.' to assign the current directory\n"
                "               to drive 0. Dirs are relative to the run_directory param\n"
		"               Note: do not use a trailing '/' on a path.\n"
		"   -X<bus>:<cmd>\n"
		"               send an 'X'-command to the specified bus, e.g. to set\n"
		"               the IEC bus to device number 9 use:\n"
		"               -Xiec:U=9\n"
		"   -d <device>	define serial device to use\n"
#ifndef _WIN32
		"   -s <socket>	define socket device to use (instead of device)\n"
#endif
		"   -d auto     auto-detect serial device\n"
		"   -w          Use advanced wildcards (anything following '*' must also match)\n"
		"   -D          run as daemon, disable user interface\n"
		"   -v          enable debug log output\n"
		"   -?          gives you this help text\n"
		"\n"
		"Typical examples are:\n"
		"   fsser -A0:fs=/home/user/8bitdir .\n"
		"   fsser -d /dev/ttyUSB0 -A0:=/home/user/8bitdir/somegame.d64 /tmp\n"
	);
	exit(rv);
}


// Assert switch is a single character
// If somebody tries to combine options (e.g. -vD) or
// encloses the parameter in quotes (e.g. fsser "-d COM5")
// this function will throw an error
void assert_single_char(char *argv) {
	if (strlen(argv) != 2) {
		log_error("Unexpected trailing garbage character%s '%s' (%s)\n",
				strlen(argv) > 3 ? "s" : "", argv + 2, argv);
		exit (EXIT_RESPAWN_NEVER);
	}
}

// --------------------------------------------------------------------------------------
// poll loop callback functions
//

typedef struct {
	int do_reset;
} accept_data_t;

static type_t accept_data_type = {
	"accept_data_t",
	sizeof(accept_data_t),
	NULL
};

static void fd_hup(int fd, void *data) {

	log_debug("fd_hup for fd=%d (%p)\n", fd, data);

	if (fd >= 0) {
		close(fd);
		poll_unregister(fd);
	}

	if (data) {
		mem_free(data);
	}

}

static void fd_read(int fd, void *data) {

	log_debug("fd_read for fd=%d (%p)\n", fd, data);

	in_device_t *fddata = (in_device_t*) data;

	int rv = in_device_loop(fddata);

	if (rv == 2) {
		poll_unregister(fd);
	}
}

static void fd_accept(int fd, void *data) {

	log_debug("fd_accept for fd=%d (%p)\n", fd, data);

	accept_data_t *adata = (accept_data_t*) data;

	int data_fd = socket_accept(fd);

	in_device_t *td = in_device_init(data_fd, data_fd, adata->do_reset);

	poll_register_readwrite(data_fd, td, fd_read, NULL, fd_hup);
}

static void fd_listen(const char *socketname, int do_reset) {

	log_debug("fd_listen for socket=%s\n", socketname);

	int listen_fd = socket_listen(socketname);

	accept_data_t *data = mem_alloc(&accept_data_type);

	data->do_reset = do_reset;

	poll_register_accept(listen_fd, data, fd_accept, fd_hup);
}

// --------------------------------------------------------------------------------------

int main(int argc, char *argv[]) {

	// stop server when number of registered sockets falls below this
	int min_num_socks = 0;

	int i;
	char *dir=NULL;

	char *device = NULL;	/* device name or NULL if not given */
	char *socket = NULL;	/* socket name or NULL if not given */
	char *tsocket = NULL;	/* tools socket name or NULL if not given */
	char parameter_d_given = false;
	char use_stdio = false;

	mem_init();

	poll_init();

	// Check -v (verbose) first to enable log_debug()
	// when processing other options
	for (i=1; i < argc; i++) if (!strcmp("-v", argv[i])) set_verbose();

	terminal_init();

	i=1;
	while(i<argc && argv[i][0]=='-') {
	  switch(argv[i][1]) {
	    case '?':
		assert_single_char(argv[i]);
		usage(EXIT_SUCCESS);	/* usage() exits already */
		break;
	    case 'd':
		if (socket != NULL) {
		  log_error("both -s and -d given!\n");
		  exit(EXIT_RESPAWN_NEVER);
		}
		// device name
		assert_single_char(argv[i]);
		parameter_d_given = true;
		if (i < argc-2) {
		  i++;
		  device = argv[i];
		  if(!strcmp(device,"auto")) {
		    guess_device(&device);
		    /* exits on more or less than a single possibility */
		  }
		  if(!strcmp(device,"-")) {
			device = NULL; 	// use stdin/out
			use_stdio = true;
		  }
		  log_info("main: device = %s\n", use_stdio ? "<stdio>" : device);
		} else {
		  log_error("-d requires <device> parameter\n");
		  exit(EXIT_RESPAWN_NEVER);
		}
 	     	break;
#ifndef _WIN32
	    case 's':
		// socket name
		if (device != NULL || use_stdio) {
		  log_error("both -s and -d given!\n");
		  exit(EXIT_RESPAWN_NEVER);
		}
		assert_single_char(argv[i]);
		parameter_d_given = true;
		if (i < argc-2) {
		  i++;
		  socket = argv[i];
		  log_info("main: socket = %s\n", socket);
		} else {
		  log_error("-s requires <socket name> parameter\n");
		  exit(EXIT_RESPAWN_NEVER);
		}
 	     	break;
	    case 'T':
		// tools socket name
		assert_single_char(argv[i]);
		parameter_d_given = true;
		if (i < argc-2) {
		  i++;
		  tsocket = argv[i];
		  log_info("main: tools socket = %s\n", tsocket);
		} else {
		  log_error("-T requires <socket name> parameter\n");
		  exit(EXIT_RESPAWN_NEVER);
		}
 	     	break;
#endif
	    case 'A':
	    case 'X':
		// ignore these, as those will be evaluated later by cmd_...
		break;
	    case 'v':
		assert_single_char(argv[i]);
		break;
	    case 'D':
		assert_single_char(argv[i]);
		disable_user_interface();
		break;
            case 'w':
		assert_single_char(argv[i]);
                advanced_wildcards = true;
                break;
	    default:
		log_error("Unknown command line option %s\n", argv[i]);
		usage(EXIT_RESPAWN_NEVER);
		break;
	  }
	  i++;
	}
	if(!parameter_d_given) {
		guess_device(&device);
	}

	if(argc == 1) {
		// Use default configuration if no parameters were given
		// Default assigns are made later
		dir = ".";
	} else if (i == argc) {
		log_error("Missing run_directory\n");
		usage(EXIT_RESPAWN_NEVER);
	} else if (argc > i+1) {
		log_error("Multiple run_directories or missing option sign '-'\n");
		usage(EXIT_RESPAWN_NEVER);
	} else dir = argv[i];

	log_info("dir=%s\n", dir);

	if(chdir(dir)<0) { 
		log_error("Couldn't change to directory %s, errno=%d (%s)\n",
			dir, os_errno(), os_strerror(os_errno()));
	  exit(EXIT_RESPAWN_NEVER);
	}

	if (device != NULL) {
		serial_port_t fdesc;
		fdesc = device_open(device);
		if (os_open_failed(fdesc)) {
		  /* error */
		  log_error("Could not open device %s, errno=%d (%s)\n",
			device, os_errno(), os_strerror(os_errno()));
		  exit(EXIT_RESPAWN_NEVER);
		}
		if(config_ser(fdesc)) {
		  log_error("Unable to configure serial port %s, errno=%d (%s)\n",
			device, os_errno(), os_strerror(os_errno()));
		  exit(EXIT_RESPAWN_NEVER);
		}

		in_device_t *fdp = in_device_init(fdesc, fdesc, 1);
		poll_register_readwrite(fdesc, fdp, fd_read, NULL, fd_hup);
		min_num_socks ++;
	}

	// we have the serial device open, now we can drop privileges
	drop_privileges();

#ifndef _WIN32
	if (tsocket == NULL) {
		const char *home = os_get_home_dir();
		tsocket = malloc_path(home, ".xdtools");
	}
	fd_listen(tsocket, 0);
	min_num_socks ++;


	if (socket != NULL) {
	
		//fd_listen(socket, 1);
		int data_fd = socket_open(socket);
		if (data_fd < 0) {
			log_errno("Could not open listen socket at %s\n", socket);
			exit(EXIT_RESPAWN_NEVER);
		}
		in_device_t *fdp = in_device_init(data_fd, data_fd, 1);
		poll_register_readwrite(data_fd, fdp, fd_read, NULL, fd_hup);
		min_num_socks ++;
        } else 
	if (device == NULL && !use_stdio) {

                log_error("No socket or device name given!\n");
                exit(EXIT_RESPAWN_NEVER);
	}
#endif

	cmd_init();

	if(argc == 1) {
		// Default assigns
		log_info("Using built-in default assigns\n");
		provider_assign(0, "fs",   os_get_home_dir(), CHARSET_ASCII, 1);
		provider_assign(1, "fs",   "/usr/local/xd2031/sample", CHARSET_ASCII, 1);
		provider_assign(2, "fs",   "/usr/local/xd2031/tools", CHARSET_ASCII, 1);
		provider_assign(3, "ftp",  "ftp.zimmers.net/pub/cbm", CHARSET_ASCII, 1);
		provider_assign(7, "http", "www.zimmers.net/anonftp/pub/cbm/", CHARSET_ASCII, 1);
	} else {
		if (cmd_assign_from_cmdline(argc, argv)) {
			log_error("Error assigning drives! Aborting!\n");
			usage(EXIT_RESPAWN_NEVER);
		}
	}


	while (poll_loop(1000) == 0) { 
		// TODO: move UI input into poll_loop()
		// UI input
		int rv = in_ui_loop();
		if (rv) {
			break;
		}

		if (poll_num_sockets() < min_num_socks) {
			break;
		}
	}

	poll_shutdown();
 
	// If the device is lost, the daemon should always restart the server
	// when it is available again
	//if(res) {
	//	exit(EXIT_RESPAWN_ALWAYS);
	//} else {
		exit(EXIT_SUCCESS);
	//}
}

