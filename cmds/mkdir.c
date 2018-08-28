/****************************************************************************

    xd2031 filesystem server - command frontend
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "os.h"
#include "mem.h"
#include "terminal.h"
#include "wireformat.h"
#include "name.h"
#include "xdcmd.h"
#include "cerrno.h"


#define MAX_BUFFER_SIZE                 64

// put/save command code

static int cmd_mkdir_single(int sockfd, const char *trgname) {

	int rv = CBM_ERROR_OK;

	// look at the target file

	const int BUFLEN = 255;	
	uint8_t *name = mem_alloc_c(BUFLEN+1, "parse buffer");
	uint8_t *buf = mem_alloc_c(BUFLEN+1, "msg buffer");
	uint8_t pkgfd = 0;
	nameinfo_t ninfo;

        // note that parse_filename parses in-place, nameinfo then points to name buffer
        strncpy((char*)name, trgname, BUFLEN);
        name[BUFLEN] = 0;

	nameinfo_init(&ninfo);
        parse_cmd_pars(name, strlen((const char*)name), &ninfo);

        if (send_longcmd(sockfd, FS_MKDIR, pkgfd, &ninfo)) {
	        if ((recv_packet(sockfd, buf, 256) > 0)
                	&& buf[FSP_CMD] == FS_REPLY) {
		       	
			if (buf[FSP_DATA] != CBM_ERROR_OK) {

				log_error("Error opening file: %d\n", buf[FSP_DATA]);
				rv = buf[FSP_DATA];
			}
		} else {
			log_error("Problem receiving reply to open\n");
			rv = CBM_ERROR_FAULT;
		}
	} else {
		log_error("Problem sending open\n");
	}
	mem_free(buf);
	mem_free(name);
	return rv;

}


int cmd_mkdir(int sockfd, int argc, const char *argv[]) {

	// TODO: unify with get
        int force = 0;
        int rv = CBM_ERROR_OK;

        int p = 0;

        // check filenames
        if ((argc - p) < 1) {
                log_error("Too few parameters!\n");
                return CBM_ERROR_SYNTAX_NONAME;
        }

	if ((argc - p) == 1) {
		// single name parameter, use name for both source and target
	} else {
        	// take target name out of list
        	argc--;
	}

	for (; rv == CBM_ERROR_OK && p < argc; p++) {

		rv = cmd_mkdir_single(sockfd, argv[p]);
	}

	return rv;
}

