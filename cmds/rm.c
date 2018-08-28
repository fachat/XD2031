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


// rm/scratch command code


int cmd_rm_int(int sockfd, int argc, const char *argv[], int isrmdir) {

	int rv = CBM_ERROR_OK;
	int scratched = 0;

	int p = 0;

	// check filenames
	if ((argc - p) < 1) {
		log_error("Too few parameters!\n");
		return CBM_ERROR_SYNTAX_NONAME;
	}

	// now look at the files
	const int BUFLEN = 255;
	uint8_t *name = mem_alloc_c(BUFLEN+1, "parse buffer");
	uint8_t *buf = mem_alloc_c(BUFLEN+1, "rx buffer");
	uint8_t pkgfd = 15;
	nameinfo_t ninfo;

	rv = CBM_ERROR_OK;

	for (; (rv == CBM_ERROR_OK) && (p < argc); p++) {

		// note that parse_filename parses in-place, nameinfo then points to name buffer
		strncpy((char*)name, argv[p], BUFLEN);
		name[BUFLEN] = 0;

		nameinfo_init(&ninfo);
		parse_cmd_pars(name, strlen((const char*)name), &ninfo);

		if (send_longcmd(sockfd, isrmdir ? FS_RMDIR : FS_DELETE, pkgfd, &ninfo)) {

			if ((recv_packet(sockfd, buf, 256) > 0) 
			 	&& buf[FSP_CMD] == FS_REPLY) {

			       	if (buf[FSP_DATA] == CBM_ERROR_SCRATCHED) {
					log_cbmerr(buf[FSP_DATA], buf[FSP_DATA+1], 0);
				} else
				if (buf[FSP_DATA] != CBM_ERROR_OK) {
					log_cbmerr(buf[FSP_DATA], 0, 0);
				}
			}
		}
	}

	mem_free(name);
	mem_free(buf);
	return rv;

}

int cmd_rm(int sockfd, int argc, const char *argv[]) {
	return cmd_rm_int(sockfd, argc, argv, 0);
}

int cmd_rmdir(int sockfd, int argc, const char *argv[]) {
	return cmd_rm_int(sockfd, argc, argv, 1);
}
