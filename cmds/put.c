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

#include "os.h"
#include "mem.h"
#include "terminal.h"
#include "wireformat.h"
#include "name.h"
#include "xdcmd.h"


// put/save command code


static int cmd_put_int(int sockfd, int type, int argc, const char *argv[]) {

	uint8_t *name = mem_alloc_c(256, "parse buffer");
	uint8_t *buf = mem_alloc_c(256, "msg buffer");
	uint8_t pkgfd = 0;

	nameinfo_t ninfo;

	if (argc > 2) {
		log_error("Too many parameters!\n");
		mem_free(name);
		return -1;
	}

	if (argc == 1) {
		// note that parse_filename parses in-place, nameinfo then points to name buffer
		strncpy((char*)name, argv[0], 255);
		name[255] = 0;
	} else {
		name[0] = 0;
	}
	parse_filename(name, strlen((const char*)name), 255, &ninfo, PARSEHINT_LOAD);

	int rv = send_longcmd(sockfd, FS_OPEN_WR, pkgfd, &ninfo);

	if (rv >= 0) {
		rv = recv_packet(sockfd, buf, 256);

		if (buf[FSP_CMD] == FS_REPLY && buf[FSP_DATA] == CBM_ERROR_OK) {

#if 0
		    // receive data packets until EOF
		    do {
			rv = send_cmd(sockfd, FS_READ, pkgfd);
			if (rv < 0) {
				log_errno("Unable to send read request!\n");
				break;
			}

			rv = recv_packet(sockfd, buf, 256);

			if (rv < 0) {
				log_errno("Could not receive packet!\n");
				break;
			}
			if (buf[FSP_CMD] != FS_DATA && buf[FSP_CMD] != FS_DATA_EOF) {
				log_error("Received unexpected packet of type %d!\n", buf[FSP_CMD]);
				break;
			}

			rv = parse_dir_packet(buf + FSP_DATA, buf[FSP_LEN] - FSP_DATA, &dir);

			if (rv < 0) {
				break;
			}

			switch (type) {
			case 2:
				print_long_packet(&dir);
				break;
			case 1:
				print_ls_packet(&dir);
				break;
			default:
				print_dir_packet(&dir);
				break;
			}
			
			if (buf[FSP_CMD] == FS_DATA_EOF) {
				break;
			}
		    } while(rv == 0); 
#endif
		    rv = send_cmd(sockfd, FS_CLOSE, pkgfd);
		}
	}
	mem_free(buf);
	mem_free(name);
	return rv;

}

int cmd_put(int sockfd, int argc, const char *argv[]) {
	return cmd_put_int(sockfd, 0, argc, argv);
}


