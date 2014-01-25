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

#include "fscmd.h"
#include "privs.h"
#include "log.h"
#include "charconvert.h"
#include "provider.h"
#include "mem.h"
#include "serial.h"
#include "terminal.h"

#define FALSE 0
#define TRUE 1

void usage(int rv) {
	printf("Usage: fsser [options] run_directory\n"
		" options=\n"
                "   -A<drv>:<provider-string>\n"
                "               assign a provider to a drive\n"
                "               e.g. use '-A0:fs=.' to assign the current directory\n"
                "               to drive 0. Dirs are relative to the run_directory param\n"
		"   -X<bus>:<cmd>\n"
		"               send an 'X'-command to the specified bus, e.g. to set\n"
		"               the IEC bus to device number 9 use:\n"
		"               -Xiec:U=9\n"
		"   -d <device>	define serial device to use\n"
		"   -d auto     auto-detect serial device\n"
		"   -D          run as daemon, disable user interface\n"
		"   -v          enable debug log output\n"
		"   -?          gives you this help text\n"
	);
	exit(rv);
}


int main(int argc, char *argv[]) {
	serial_port_t writefd=0, readfd=0;
	serial_port_t fdesc;
	int i;
	char *dir=NULL;
	char *device = NULL;	/* device name or NULL if stdin/out */
	char parameter_d_given = FALSE;

	mem_init();

	// Check -v (verbose) first to enable log_debug()
	// when processing other options
	for (i=1; i < argc; i++) if (!strcmp("-v", argv[i])) set_verbose();

	terminal_init();

	i=1;
	while(i<argc && argv[i][0]=='-') {
	  switch(argv[i][1]) {
	    case '?':
		usage(EXIT_SUCCESS);	/* usage() exits already */
		break;
	    case 'd':
	    	parameter_d_given = TRUE;
		if (i < argc-2) {
		  i++;
		  device = argv[i];
		  if(!strcmp(device,"auto")) {
		    guess_device(&device);
		    /* exits on more or less than a single possibility */
		  }
		  if(!strcmp(device,"-")) device = NULL; 	// use stdin/out
		  log_info("main: device = %s\n", device);
		}
 	     	break;
	    case 'A':
	    case 'X':
		// ignore these, as those will be evaluated later by cmd_...
		break;
	    case 'v':
		break;
	    case 'D':
		disable_user_interface();
		break;
	    default:
		log_error("Unknown command line option %s\n", argv[i]);
		usage(EXIT_RESPAWN_NEVER);
		break;
	  }
	  i++;
	}
	if(!parameter_d_given) guess_device(&device);

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
		readfd = fdesc;
		writefd = readfd;
	}

	// we have the serial device open, now we can drop privileges
	drop_privileges();

	cmd_init();

	if(argc == 1) {
		// Default assigns
		log_info("Using built-in default assigns\n");
		provider_assign(0, "fs",   os_get_home_dir());
		provider_assign(1, "fs",   "/usr/local/xd2031/sample");
		provider_assign(2, "fs",   "/usr/local/xd2031/tools");
		provider_assign(3, "ftp",  "ftp.zimmers.net/pub/cbm");
		provider_assign(7, "http", "www.zimmers.net/anonftp/pub/cbm/");
	} else cmd_assign_from_cmdline(argc, argv);

	int res = cmd_loop(readfd, writefd);

	if (device != NULL) {
		device_close(fdesc);
	}

	// If the device is lost, the daemon should always restart the server
	// when it is available again
	if(res) {
		exit(EXIT_RESPAWN_ALWAYS);
	} else {
		exit(EXIT_SUCCESS);
	}
}

