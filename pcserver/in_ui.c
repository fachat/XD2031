/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2012,2014 Andre Fachat, Nils Eilers

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
 * In this file the actual command work is done
 */

#include "os.h"

#include <stdbool.h>
#include <strings.h>

#include "provider.h"
#include "dir.h"
#include "cmd.h"
#include "errors.h"

static int user_interface_enabled = true;


//------------------------------------------------------------------------------------
// helpers


void disable_user_interface(void) {
	user_interface_enabled = false;
	log_warn("User interface disabled. Abort with \"service fsser stop\".\n");
}

#define INBUF_SIZE 1024
// reads stdin and returns true, if the main loop should abort
int in_ui_loop(void) {

	int err;
	char buf[INBUF_SIZE + 1];

	// are we enabled?
        if(!user_interface_enabled) {
		return 0;
	}
	
	// do we have data to process?
	if(!os_stdin_has_data()) {
		return 0;
	}

	//log_debug("cmd_process_stdin()\n");

	if (fgets(buf, INBUF_SIZE, stdin) == NULL) {
		return false;
	}
	drop_crlf(buf);

	// ignore empty line
	if (buf[0] == 0) {
		return false;
	}

	log_debug("stdin: %s\n", buf);

	// Q / QUIT
	if((!strcasecmp(buf, "Q")) || (!strcasecmp(buf, "QUIT"))) {
		log_info("Aborted by user request.\n");
		return true;
	}

	// Enable *=+ / disable *=- advanced wildcards
	if ((buf[0] == '*') && (buf[1] == '=')) {
		if (buf[2] == '+') {
			advanced_wildcards = true;
			log_info("Advanced wildcards enabled.\n");
			return false;
		}
		if (buf[2] == '-') {
			advanced_wildcards = false;
			log_info("Advanced wildcards disabled.\n");
			return false;
		}
	}

	if (!strcmp(buf, "D")) {
		// dump memory structures for analysis

		// dump open endpoints, files etc
		// maybe later compare with dump from mem to find memory leaks
		provider_dump();
		return false;
	}

	if (buf[0] == 'A' || buf[0] == 'a') {
		// assign from stdin control
		err = cmd_assign(buf+1, CHARSET_ASCII, 1);
                if (err != CBM_ERROR_OK) {
                        log_error("%d Error assigning %s\n", err, buf+1);
                }
		return false;
	}

	log_error("Syntax error: '%s'\n", buf);
	return false;
}


