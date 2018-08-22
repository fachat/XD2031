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


// put/save command code


static int cmd_get_int(int sockfd, int type, int argc, const char *argv[]) {

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


	// truncate the opened file?
	int trunc = 0;

	// take target name out of list
	argc--;
	// open target on host filesystem
	const char *trgname = argv[argc];
	// check if file exists and what type it is
	struct stat statbuf;

	if (stat(trgname, &statbuf) < 0) {
		if (errno != ENOENT) {
			// we do not overwrite, so NOENT is totally acceptable, if not wanted
			rv = errno_to_error(errno);
			log_errno("Error accessing '%s'\n", trgname);
			return rv;
		}
	} else {
		// no error - so file exists
		if (!force) {
			log_error("Target file exists!\n");
			return CBM_ERROR_FILE_EXISTS;
		}
		if (S_IFDIR == (statbuf.st_mode & S_IFMT)) {
			log_error("Target path is a directory!\n");
			return CBM_ERROR_FILE_TYPE_MISMATCH;
		}
		if (S_IFLNK == (statbuf.st_mode & S_IFMT)) {
			log_error("Target path is a symlink!\n");
			return CBM_ERROR_FILE_TYPE_MISMATCH;
		}
		if (S_IFREG == (statbuf.st_mode & S_IFMT)) {
			// only regular files are truncated, sockets, devices, ... are not
			trunc = 1;
		}
	}
	// open target file
	int outfd = open(trgname, O_CREAT | O_WRONLY | (trunc ? O_TRUNC : 0), S_IRUSR | S_IWUSR | S_IRGRP);
	if (outfd < 0) {
		rv = errno_to_error(errno);
		log_errno("Could not open target file for writing!\n");
		return rv;
	}

	// now look at the source files
	
	const int BUFLEN = 255;
	uint8_t *name = mem_alloc_c(BUFLEN+1, "parse buffer");
	uint8_t *buf = mem_alloc_c(BUFLEN+1, "msg buffer");
	uint8_t pkgfd = 0;
	nameinfo_t ninfo;

	rv = CBM_ERROR_OK;

	for (; (rv == CBM_ERROR_OK) && (p < argc); p++) {

		// note that parse_filename parses in-place, nameinfo then points to name buffer
		strncpy((char*)name, argv[p], BUFLEN);
		name[BUFLEN] = 0;

		parse_filename(name, strlen((const char*)name), BUFLEN, &ninfo, PARSEHINT_LOAD);

		if (send_longcmd(sockfd, FS_OPEN_RD, pkgfd, &ninfo)) {

			if ((recv_packet(sockfd, buf, 256) > 0) 
			 	&& buf[FSP_CMD] == FS_REPLY && buf[FSP_DATA] == CBM_ERROR_OK) {

		    		// receive data packets until EOF
		    		do {
					if (send_cmd(sockfd, FS_READ, pkgfd) < 0) {
						rv = errno_to_error(errno);
						log_errno("Unable to send read request!\n");
						break;
					}

					if (recv_packet(sockfd, buf, 256) < 0) {
						rv = errno_to_error(errno);
						log_errno("Could not receive packet!\n");
						break;
					}
					if (buf[FSP_CMD] != FS_DATA && buf[FSP_CMD] != FS_DATA_EOF) {
						log_error("Received unexpected packet of type %d!\n", buf[FSP_CMD]);
						rv = CBM_ERROR_FAULT;
						break;
					}

					if (write(outfd, buf + FSP_DATA, buf[FSP_LEN]-FSP_DATA) < 0) {
						rv = errno_to_error(errno);
						log_errno("Error writing to file!\n");
						break;
					}
					if (buf[FSP_CMD] == FS_DATA_EOF) {
						break;
					}
		    		} while(rv == 0); 

		    		rv = send_cmd(sockfd, FS_CLOSE, pkgfd);
			}
		}
	}

	if (close(outfd) < 0) {
		log_errno("Could not close target file!");
	}

	mem_free(buf);
	mem_free(name);
	return rv;

}

int cmd_get(int sockfd, int argc, const char *argv[]) {
	return cmd_get_int(sockfd, 0, argc, argv);
}


