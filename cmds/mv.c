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


// move command code


int cmd_move(int sockfd, int argc, const char *argv[]) {

	// TODO: unify with get
        int force = 0;
        int rv = CBM_ERROR_OK;

        // check filenames
        if ((argc) > 2) {
                log_error("Too many parameters!\n");
                return CBM_ERROR_SYNTAX_NONAME;
        }
        if ((argc) < 1) {
                log_error("Too few parameters!\n");
                return CBM_ERROR_SYNTAX_NONAME;
        }

	// look at the target file

	const int BUFLEN = 255;	
	uint8_t *buf = mem_alloc_c(BUFLEN+1, "msg buffer");
	uint8_t pkgfd = 0;

	uint8_t *name1 = mem_alloc_c(BUFLEN+1, "parse buffer");
	uint8_t *name2 = mem_alloc_c(BUFLEN+1, "parse buffer");
	nameinfo_t ninfo1;
	nameinfo_t ninfo2;

	nameinfo_init(&ninfo1);
	nameinfo_init(&ninfo2);

        // note that parse_filename parses in-place, nameinfo then points to name buffer
        strncpy((char*)name1, argv[0], BUFLEN);
        name1[BUFLEN] = 0;

	if (argc == 1) {
        	parse_cmd_pars(name1, strlen((const char*)name1), CMD_RENAME, &ninfo1);
		if (ninfo1.cmd != CMD_RENAME) {
			rv = CBM_ERROR_SYNTAX_NONAME;
		} else
		if (ninfo1.num_files != 1) {
                	log_error("Wrong number of parameters!\n");
			rv = CBM_ERROR_SYNTAX_NONAME;
		}	 
	} else {
		// argc is 2
        	parse_cmd_pars(name1, strlen((const char*)name1), FS_OPEN_RD, &ninfo1);

	        strncpy((char*)name2, argv[1], BUFLEN);
        	name2[BUFLEN] = 0;
        	parse_cmd_pars(name2, strlen((const char*)name2), FS_OPEN_RD, &ninfo2);

		if (ninfo1.num_files != 1 || ninfo2.num_files != 1) {
                	log_error("Wrong number of parameters!\n");
			rv = CBM_ERROR_SYNTAX_NONAME;
		} else {
			ninfo1.trg = ninfo2.file[0];
		}
	}


        if (rv == CBM_ERROR_OK 
			&& send_longcmd(sockfd, FS_MOVE, pkgfd, &ninfo1)) {
	        if ((recv_packet(sockfd, buf, 256) > 0)
                	&& buf[FSP_CMD] == FS_REPLY) {
		       	
			if (buf[FSP_DATA] != CBM_ERROR_OK) {

				log_error("Error moving file: %d\n", buf[FSP_DATA]);
				rv = buf[FSP_DATA];
			}
		} else {
			log_error("Problem receiving reply to open\n");
		}
	} else {
		log_error("Problem sending open\n");
	}
	mem_free(buf);
	mem_free(name1);
	mem_free(name2);
	return rv;

}


