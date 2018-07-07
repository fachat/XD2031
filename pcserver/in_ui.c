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
#include "list.h"
#include "cmd.h"
#include "errors.h"
#include "cmdline.h"

static int user_interface_enabled = true;
static int user_interface_aborted = false;

static err_t ui_cmd_quit(int flag, void *param) {
	(void)flag;
	(void)param;
	log_info("Aborted by user request.\n");
	user_interface_aborted = true;
        return CBM_ERROR_OK;
}

static err_t ui_cmd_dump(int flag, void *param) {
       	(void)flag;
	(void)param;
 	// dump memory structures for analysis

        // dump open endpoints, files etc
        // maybe later compare with dump from mem to find memory leaks
        provider_dump();

        return CBM_ERROR_OK;
}

static cmdline_t ui_options[] = {
        { "quit",    "Q",    CMDL_UI,      PARTYPE_FLAG,   NULL, ui_cmd_quit, NULL,
                "Quit the server", NULL },
        { "dump",    NULL,   CMDL_UI,      PARTYPE_FLAG,   NULL, ui_cmd_dump, NULL, 
                "Dump internal memory structures", NULL },
};

void in_ui_init() {
        cmdline_register_mult(ui_options, sizeof(ui_options)/sizeof(cmdline_t));
}

//------------------------------------------------------------------------------------
// helpers

void enable_user_interface(void) {
	user_interface_enabled = true;
}

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

        err_t rv = cmdline_parse_cfg(buf, CMDL_INIT+CMDL_PARAM+CMDL_CMD+CMDL_UI);
	if (rv) {
		log_error("Syntax error: '%s'\n", buf);
	}

	return user_interface_aborted;
}


