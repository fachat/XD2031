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


static int cmd_put_int(int sockfd, int type, int argc, const char *argv[]) {

	// TODO: unify with get
        int force = 0;
        int rv = CBM_ERROR_OK;

        int p = 0;
        // parse options
        while ((p < argc) && (argv[p][0] == '-')) {
                switch(argv[p][1]) {
                case 'f':
                        force = 1;
                        break;
                case '-':
                        // break options
                        p++;
                        goto endopts;
                default:
                        log_error("Unknown option '%c'\n", argv[p][1]);
                        return CBM_ERROR_SYNTAX_INVAL;
                }
                p++;
        }
endopts:

        // check filenames
        if ((argc - p) < 2) {
                log_error("Too few parameters!\n");
                return CBM_ERROR_SYNTAX_NONAME;
        }

        // take target name out of list
        argc--;
        // open target on host filesystem
        const char *trgname = argv[argc];

	// look at the target file

	const int BUFLEN = 255;	
	uint8_t *name = mem_alloc_c(BUFLEN+1, "parse buffer");
	uint8_t *buf = mem_alloc_c(BUFLEN+1, "msg buffer");
	uint8_t pkgfd = 0;
	nameinfo_t ninfo;

        // note that parse_filename parses in-place, nameinfo then points to name buffer
        strncpy((char*)name, trgname, BUFLEN);
        name[BUFLEN] = 0;

        parse_filename(name, strlen((const char*)name), BUFLEN, &ninfo, PARSEHINT_LOAD);

        if (send_longcmd(sockfd, force ? FS_OPEN_OW : FS_OPEN_WR, pkgfd, &ninfo)) {
	        if ((recv_packet(sockfd, buf, 256) > 0)
                	&& buf[FSP_CMD] == FS_REPLY) {
		       	
			if (buf[FSP_DATA] == CBM_ERROR_OK) {

		            for (; (rv == CBM_ERROR_OK) && (p < argc); p++) {
				
				int infd;

				const char *srcname = argv[p];
				infd = open(srcname, O_RDONLY);
				if (infd < 0) {
					rv = errno_to_error(errno);
					log_errno("Error opening file '%s'\n", srcname);
					break;
				}

                                // receive data packets until EOF
				ssize_t n = 0;
                                do {
                                        if ((n = read(infd, buf + FSP_DATA, MAX_BUFFER_SIZE-FSP_DATA)) < 0) {
                                                rv = errno_to_error(errno);
                                                log_errno("Error reading from file!\n");
                                                break;
                                        }

					// prepare send buffer
					buf[FSP_LEN] = FSP_DATA + n;
					buf[FSP_CMD] = (n == 0) ? FS_WRITE_EOF : FS_WRITE;
					buf[FSP_FD] = pkgfd;

					if (send_packet(sockfd, buf, buf[FSP_LEN]) < 0) {
                                                rv = errno_to_error(errno);
                                                log_errno("Unable to send read request!\n");
                                                break;
                                        }

                                        if (recv_packet(sockfd, buf, BUFLEN+1) < 0) {
                                                rv = errno_to_error(errno);
                                                log_errno("Could not receive packet!\n");
                                                break;
                                        }
                                        if (buf[FSP_CMD] != FS_REPLY) {
                                                log_error("Received unexpected packet of type %d!\n", buf[FSP_CMD]);
                                                rv = CBM_ERROR_FAULT;
                                                break;
                                        }
				} while (n > 0);

			        if (close(infd) < 0) {
			                log_errno("Could not close source file!");
			        }
			    }
			} else {
				log_error("Error opening file: %d\n", buf[FSP_DATA]);
			}
		} else {
			log_error("Problem receiving reply to open\n");
		}
	} else {
		log_error("Problem sending open\n");
	}
	mem_free(buf);
	mem_free(name);
	return rv;

}

int cmd_put(int sockfd, int argc, const char *argv[]) {
	return cmd_put_int(sockfd, 0, argc, argv);
}


