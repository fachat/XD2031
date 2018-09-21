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

/*
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
*/

#include <stdbool.h>

#include "charconvert.h"
#include "types.h"
#include "mem.h"
#include "serial.h"
#ifndef _WIN32
#  include "socket.h"
#endif
#include "cmd.h"
#include "xcmd.h"
#include "privs.h"
#include "terminal.h"
#include "in_device.h"
#include "in_ui.h"
#include "loop.h"
#include "cmdline.h"

#include "provider.h"
#include "dir.h"


// --------------------------------------------------------------------------------------


static char *device_name = NULL;	/* device name or NULL if not given */
static char *socket_name = NULL;	/* socket name or NULL if not given */
static char *tsocket_name = NULL;	/* tools socket name or NULL if not given */

static char *cfg_name = NULL;		/* name of the config file if non-standard */

static err_t main_assign(const char *param, void *extra, int ival) {
	(void) extra;
	(void) ival;
	
	int err = cmd_assign_cmdline(param, CHARSET_ASCII);
        if (err != CBM_ERROR_OK) {
                log_error("%d Error assigning %s\n", err, param);
        }
	return err;
}

static err_t main_set_param(const char *param, void *extra, int ival) {
	(void) ival;
	char **x = (char**)extra;
	*x = mem_alloc_str(param);

	return E_OK;
}

static err_t main_xcmd(const char *param, void *extra, int ival) {
	(void) extra;
	(void) ival;
	
	xcmd_register(param);

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
        { "verbose",    "v",	CMDL_INIT,	PARTYPE_FLAG,   NULL, main_set_verbose, NULL,
                "Set verbose mode", NULL },
	{ "config",	"c",	CMDL_CFG,	PARTYPE_PARAM,	cmdline_set_param, NULL, &cfg_name,
		"Set name of config file instead of default ~/.xdconfig", NULL },
        { "daemon", 	"D",	CMDL_RUN,	PARTYPE_FLAG,   NULL, main_set_daemon, NULL,
		"Run as daemon, disable cli user interface.", NULL },
	{ "device",	"d",	CMDL_RUN,	PARTYPE_PARAM,	cmdline_set_param, NULL, &device_name,
		"Set name of device to use. Use 'auto' for autodetection (default)", NULL },
#ifndef _WIN32
	{ "socket",	"s",	CMDL_RUN,	PARTYPE_PARAM,	main_set_param, NULL, &socket_name,
		"Set name of socket to use instead of device", NULL },
	{ "tools",	"T",	CMDL_RUN,	PARTYPE_PARAM,	main_set_param, NULL, &tsocket_name,
		"Set name of tools socket to use instead of ~/.xdtools", NULL },
#endif
        { "wildcards", 	"w",	CMDL_PARAM,	PARTYPE_FLAG,   NULL, cmdline_set_flag, &advanced_wildcards,
		"Use advanced wildcards", NULL },
        { "assign", 	"A",	CMDL_CMD,	PARTYPE_PARAM,  main_assign, NULL, NULL,
		"Assign a provider to a drive\n"
                "               e.g. use '-A0:fs=.' to assign the current directory\n"
                "               to drive 0. Dirs are relative to the run_directory param\n"
                "               Note: do not use a trailing '/' on a path.\n"
		, NULL },
        { "xcmd", 	"X",	CMDL_CMD,	PARTYPE_PARAM,  main_xcmd, NULL, NULL,
                "Send an 'X'-command to the specified bus\n"
		"               e.g. to set the IEC bus to device number 9 use:\n"
                "               -Xiec:U=9\n"
		, NULL },
};

#define	BUFFER_SIZE	8192
#define	ARGP_SIZE	10

static err_t cfg_load(void) {

	const char *filename = NULL;
	err_t rv = E_OK;

	if (cfg_name == NULL) {
		const char *home = os_get_home_dir();
		filename = malloc_path(home, ".xdconfig");
	} else {
		filename = mem_alloc_str(cfg_name);
		// default when not found
		rv = E_ABORT;
	}

	FILE *fd = fopen(filename, "r");
	if (fd == NULL) {
		log_errno("Could not open file '%s'", filename);
	} else {
		// file was opened ok
		rv = E_OK;

		char *line = mem_alloc_c(BUFFER_SIZE, "cfg-file-buffer");
		int lineno = 0;

		while ((line = fgets(line, BUFFER_SIZE, fd)) != NULL) {

			log_debug("Parsing line % 3d: %s", lineno, line);

			rv = cmdline_parse_cfg(line, CMDL_INIT+CMDL_RUN+CMDL_PARAM+CMDL_CMD, 0);

			if (rv) {
				break;
			}
			lineno++;
		}

		mem_free(line);	
		fclose(fd);
	}
	mem_free(filename);

	if (rv) {
		log_error("Error parsing configuration options in %s\n", filename);
	}
	return rv;
}

// --------------------------------------------------------------------------------------

void end(int rv) {

	poll_free();

	cmdline_module_free();

	mem_free(tsocket_name);
	mem_free(socket_name);
	mem_free(device_name);

	cmd_free();

	exit(rv);
}

void printusage(int isoptions) {

	printf("Usage: fsser [options] run_directory\n"
		"\n"
		"Typical option examples are:\n"
		"   fsser -A0:fs=/home/user/8bitdir .\n"
		"   fsser -d /dev/ttyUSB0 -A0:=/home/user/8bitdir/somegame.d64 /tmp\n"
		"Ui entry commands are same options, examples:\n"
		"   assign 0:fs=/home/user/8bitdir\n"
	);

	cmdline_usage(isoptions);
}

static err_t mainusage(int rv) {

	printusage(1);

	if (rv) {
		end(rv);
	}

	return rv;
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

	// -----------------------------
	// command line

	cmdline_module_init();
	cmdline_register_mult(main_options, sizeof(main_options)/sizeof(cmdline_t));

	in_ui_init();

	poll_init();

	terminal_init();


	// parse command line, phase 0 (verbose, cfg file)
	int p = argc;
	if (cmdline_parse(&p, argv, CMDL_INIT+CMDL_CFG)) {
		mainusage(EXIT_RESPAWN_NEVER);
	}
	
	// set working directory before we actually parse any relevant option for it (like assign)
	if(argc == 1) {
		// Use default configuration if no parameters were given
		// Default assigns are made later
		dir = ".";
	} else if (p == argc) {
		log_error("Missing run_directory\n");
		mainusage(EXIT_RESPAWN_NEVER);
	} else if (argc > p+1) {
		log_error("Multiple run_directories or missing option sign '-'\n");
		mainusage(EXIT_RESPAWN_NEVER);
	} else {
		dir = argv[p];
	}

	log_info("dir=%s\n", dir);

	if(chdir(dir)<0) { 
		log_error("Couldn't change to directory %s, errno=%d (%s)\n",
			dir, os_errno(), os_strerror(os_errno()));
	  	end(EXIT_RESPAWN_NEVER);
	}

	// only now can we init the cmds, as this reads the cwd()
	cmd_init();

	// load config file
	if (cfg_load()) {
	  	end(EXIT_RESPAWN_NEVER);
	}

	// parse command line, phase 1, (other options overriding the config file)
	p = argc;
	if (cmdline_parse(&p, argv, CMDL_RUN+CMDL_PARAM)) {
		mainusage(EXIT_RESPAWN_NEVER);
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


	if(argc == 1) {
		// Default assigns
		log_info("Using only information from configuration file\n");
	} else {
		// parse cmdline, phase 2 (assign and xcmd options)
		p = argc;
		cmdline_parse(&p, argv, CMDL_CMD);
	}


	while (poll_loop(1000) == 0) { 
		// TODO: move UI input into poll_loop()
		// UI input
		int rv = in_ui_loop();
		if (rv) {
			break;
		}

		if (poll_num_sockets() < min_num_socks) {
			log_debug("number of sockets %d below minimum %d - terminating\n", poll_num_sockets, min_num_socks);
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

