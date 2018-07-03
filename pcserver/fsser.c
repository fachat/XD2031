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
#include "list.h"
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
#include "cmdline.h"
#include "array_list.h"

// --------------------------------------------------------------------------------------

static list_t *assign_list = NULL;
static list_t *xcmd_list = NULL;

static char *device_name = NULL;	/* device name or NULL if not given */
static char *socket_name = NULL;	/* socket name or NULL if not given */
static char *tsocket_name = NULL;	/* tools socket name or NULL if not given */

static err_t main_add_list(const char *param, void *extra, int ival) {
	(void) ival;

	list_t **list = (list_t**) extra;

	log_debug("Add parameter '%s' to '%s' list", param, (extra == &assign_list)?"assign":"xcmd");

	list_add(*list, (char*)param);

	return CBM_ERROR_OK;
}

static err_t main_set_daemon(int flag, void *param) {
	(void) param;
	if (flag) {
		disable_user_interface();
	} else {
		enable_user_interface();
	}
	return CBM_ERROR_OK;
}

static err_t main_set_verbose(int flag, void *param) {
        (void) param;
	set_verbose(flag);
        return E_OK;
}

static cmdline_t main_options[] = {
        { "verbose",    "v",	PARTYPE_FLAG,   NULL, main_set_verbose, NULL,
                "Set verbose mode", NULL },
	{ "device",	"d",	PARTYPE_PARAM,	cmdline_set_param, NULL, &device_name,
		"Set name of device to use. Use 'auto' for autodetection (default)", NULL },
#ifndef _WIN32
	{ "socket",	"s",	PARTYPE_PARAM,	cmdline_set_param, NULL, &socket_name,
		"Set name of socket to use instead of device", NULL },
	{ "tools",	"T",	PARTYPE_PARAM,	cmdline_set_param, NULL, &tsocket_name,
		"Set name of tools socket to use instead of ~/.xdtools", NULL },
#endif
        { "wildcards", 	"w",	PARTYPE_FLAG,   NULL, cmdline_set_flag, &advanced_wildcards,
		"Use advanced wildcards", NULL },
        { "daemon", 	"D",	PARTYPE_FLAG,   NULL, main_set_daemon, NULL,
		"Run as daemon, disable cli user interface.", NULL },
        { "assign", 	"A",	PARTYPE_PARAM,  main_add_list, NULL, &assign_list,
		"Assign a provider to a drive\n"
                "               e.g. use '-A0:fs=.' to assign the current directory\n"
                "               to drive 0. Dirs are relative to the run_directory param\n"
                "               Note: do not use a trailing '/' on a path.\n"
		, NULL },
        { "xcmd", 	"X",	PARTYPE_PARAM,  main_add_list, NULL, &xcmd_list,
                "Send an 'X'-command to the specified bus\n"
		"               e.g. to set the IEC bus to device number 9 use:\n"
                "               -Xiec:U=9\n"
		, NULL },
};

// --------------------------------------------------------------------------------------

void end(int rv) {

	poll_free();

	cmdline_module_free();

	mem_free(tsocket_name);
	mem_free(socket_name);
	mem_free(device_name);

	list_free(assign_list, NULL);
	list_free(xcmd_list, NULL);


	exit(rv);
}


err_t usage(int rv, void* extra) {
	(void) extra;

	printf("Usage: fsser [options] run_directory\n"
		"\n"
		"Typical examples are:\n"
		"   fsser -A0:fs=/home/user/8bitdir .\n"
		"   fsser -d /dev/ttyUSB0 -A0:=/home/user/8bitdir/somegame.d64 /tmp\n"
	);

	cmdline_usage();

	end(rv);
	return rv;
}


// Assert switch is a single character
// If somebody tries to combine options (e.g. -vD) or
// encloses the parameter in quotes (e.g. fsser "-d COM5")
// this function will throw an error
void assert_single_char(char *argv) {
	if (strlen(argv) != 2) {
		log_error("Unexpected trailing garbage character%s '%s' (%s)\n",
				strlen(argv) > 3 ? "s" : "", argv + 2, argv);
		end (EXIT_RESPAWN_NEVER);
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
	}

	if (data) {
		mem_free(data);
	}

	poll_unregister(fd);
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

	mem_init();
	atexit(mem_exit);

	// -----------------------------
	// config init

	// stop server when number of registered sockets falls below this
	int min_num_socks = 0;

	char *dir=NULL;

	char use_stdio = false;
	assign_list = array_list_init(10);
	xcmd_list = array_list_init(10);

	// -----------------------------
	// command line

	cmdline_module_init();
	cmdline_register_mult(main_options, sizeof(main_options)/sizeof(cmdline_t));

	poll_init();

	terminal_init();

	int p = argc;
	if (cmdline_parse(&p, argv)) {
		usage(EXIT_RESPAWN_NEVER, NULL);
	}

	if (socket_name != NULL && device_name != NULL) {
		log_error("both -s and -d given!\n");
		end(EXIT_RESPAWN_NEVER);
	}

	if(socket_name == NULL && (device_name == NULL || !strcmp("auto", device_name))) {
		int rv = guess_device(&device_name);
		if (rv) {
			end(rv);
		}
	}
	if(device_name != NULL && !strcmp("-", device_name)) {
		device_name = NULL; 	// use stdin/out
		use_stdio = true;
	}
	log_info("main: device = %s\n", use_stdio ? "<stdio>" : device_name);

	if(argc == 1) {
		// Use default configuration if no parameters were given
		// Default assigns are made later
		dir = ".";
	} else if (p == argc) {
		log_error("Missing run_directory\n");
		usage(EXIT_RESPAWN_NEVER, NULL);
	} else if (argc > p+1) {
		log_error("Multiple run_directories or missing option sign '-'\n");
		usage(EXIT_RESPAWN_NEVER, NULL);
	} else {
		dir = argv[p];
	}

	log_info("dir=%s\n", dir);

	if(chdir(dir)<0) { 
		log_error("Couldn't change to directory %s, errno=%d (%s)\n",
			dir, os_errno(), os_strerror(os_errno()));
	  	end(EXIT_RESPAWN_NEVER);
	}

	if (device_name != NULL) {
		serial_port_t fdesc;
		fdesc = device_open(device_name);
		if (os_open_failed(fdesc)) {
		  /* error */
		  log_error("Could not open device %s, errno=%d (%s)\n",
			device_name, os_errno(), os_strerror(os_errno()));
		  end(EXIT_RESPAWN_NEVER);
		}
		if(config_ser(fdesc)) {
		  log_error("Unable to configure serial port %s, errno=%d (%s)\n",
			device_name, os_errno(), os_strerror(os_errno()));
		  end(EXIT_RESPAWN_NEVER);
		}

		in_device_t *fdp = in_device_init(fdesc, fdesc, 1);
		poll_register_readwrite(fdesc, fdp, fd_read, NULL, fd_hup);
		min_num_socks ++;
	}

	// we have the serial device open, now we can drop privileges
	drop_privileges();

#ifndef _WIN32
	if (tsocket_name == NULL) {
		const char *home = os_get_home_dir();
		tsocket_name = malloc_path(home, ".xdtools");
	}
	fd_listen(tsocket_name, 0);
	min_num_socks ++;


	if (socket_name != NULL) {
		if (strcmp(socket_name, "-")) {
	
			//fd_listen(socket_name, 1);
			int data_fd = socket_open(socket_name);
			if (data_fd < 0) {
				log_errno("Could not open listen socket at %s\n", socket_name);
				end(EXIT_RESPAWN_NEVER);
			}
			in_device_t *fdp = in_device_init(data_fd, data_fd, 1);
			poll_register_readwrite(data_fd, fdp, fd_read, NULL, fd_hup);
			min_num_socks ++;
		}
        } else 
	if (device_name == NULL && !use_stdio) {

                log_error("No socket or device name given!\n");
                end(EXIT_RESPAWN_NEVER);
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
		if (cmd_assign_from_cmdline(assign_list)) {
			log_error("Error assigning drives! Aborting!\n");
			usage(EXIT_RESPAWN_NEVER, NULL);
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
		end(EXIT_SUCCESS);
	//}
}

