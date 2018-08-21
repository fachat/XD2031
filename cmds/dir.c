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


// dir command code

// TODO: date/time
typedef struct {
	const char 	*name;
	int		len;
	int		attr;
	int		estimate;
	int		etype;		// FS_DIR_MOD_*
	int		splat;
	int		ftype;		// DIR/PRG/SEQ/...
} dirinfo_t;

const char *ftypes[9] = {
	"DEL", "SEQ", "PRG", "USR", "REL", "??5", "??6", "??7", "DIR"
};

static int parse_dir_packet(const uint8_t *buf, int len, dirinfo_t *dir) {
	
	memset(dir, 0, sizeof(dirinfo_t));

	if (len < FS_DIR_NAME + 1) {
		log_error("received short directory data packet at length %d\n", len);
		return -1;
	}

	dir->len = buf[FS_DIR_LEN]
			+ (buf[FS_DIR_LEN+1]<<8)
			+ (buf[FS_DIR_LEN+2]<<8)
			+ (buf[FS_DIR_LEN+3]<<8);

	dir->attr = buf[FS_DIR_ATTR];
	dir->ftype = buf[FS_DIR_ATTR] & FS_DIR_ATTR_TYPEMASK;

	// TODO: date/time
	
	dir->etype = buf[FS_DIR_MODE];

	dir->name = (const char*) buf+FS_DIR_NAME;

	return 0;
}

static void print_long_packet(dirinfo_t *dir) {

	switch (dir->etype) {
	case FS_DIR_MOD_DIR:
		printf("%10d ", dir->len);
		// TODO: date/time
		printf("%s/\n", dir->name);
		break;
	case FS_DIR_MOD_FIL:
		printf("%10d ", dir->len);
		// TODO: date/time
		printf("%s\n", dir->name);
		break;
	default:
		break;
	}
}

static void print_ls_packet(dirinfo_t *dir) {

	switch (dir->etype) {
	case FS_DIR_MOD_DIR:
		printf("%s/\n", dir->name);
		break;
	case FS_DIR_MOD_FIL:
		printf("%s\n", dir->name);
		break;
	default:
		break;
	}
}

static void print_dir_packet(dirinfo_t *dir) {

	int len = 0;
	int l = 0;

	switch (dir->etype) {
	case FS_DIR_MOD_NAM:
	case FS_DIR_MOD_NAS:
		len = (dir->len + 255) / 256;
		printf("%c%d ", (dir->attr & FS_DIR_ATTR_ESTIMATE) ? '~':' ', len);
		color_reverse();
		l = strlen(dir->name);
		l = (l > 16) ? 16 : l;
		printf("\"%s%s\" \n", 
			dir->name,
			"                "+l
			);
		color_default();
		break;
	case FS_DIR_MOD_DIR:
		dir->ftype = 8;	// hack to get "DIR"
	case FS_DIR_MOD_FIL:
		len = (dir->len + 253) / 254;
		printf("%c%d ", (dir->attr & FS_DIR_ATTR_ESTIMATE) ? '~':' ', len);
		l = len;
		l = (l<10)?0:(l<100)?1:(l<1000)?2:(l<10000)?3:(l<100000)?4:5;
		printf("%s ", "      "+l); 
		l = strlen(dir->name);
		l = (l > 16) ? 16 : l;
		printf("\"%s\"%s %c%s%c\n", 
			dir->name,
			"                "+l,
			(dir->attr & FS_DIR_ATTR_SPLAT) ? '*':' ',
			ftypes[dir->ftype],
			(dir->attr & FS_DIR_ATTR_LOCKED) ? '<':' '
			);
		break;
	case FS_DIR_MOD_FRE:
	case FS_DIR_MOD_FRS:
		len = (dir->len + 255) / 256;
		printf("%c%d ", (dir->attr & FS_DIR_ATTR_ESTIMATE) ? '~':' ', len);
		printf("BLOCKS FREE\n");
		break;
	}
}

static int cmd_dir_int(int sockfd, int type, int argc, const char *argv[]) {

	uint8_t *name = mem_alloc_c(256, "parse buffer");
	uint8_t *buf = mem_alloc_c(256, "msg buffer");
	uint8_t pkgfd = 0;

	nameinfo_t ninfo;
	dirinfo_t dir;

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

	int rv = send_longcmd(sockfd, FS_OPEN_DR, pkgfd, &ninfo);

	if (rv >= 0) {
		rv = recv_packet(sockfd, buf, 256);

		if (buf[FSP_CMD] == FS_REPLY && buf[FSP_DATA] == CBM_ERROR_OK) {

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
		}
	}
	mem_free(buf);
	mem_free(name);
	return rv;

}

int cmd_dir(int sockfd, int argc, const char *argv[]) {
	return cmd_dir_int(sockfd, 0, argc, argv);
}

int cmd_ls(int sockfd, int argc, const char *argv[]) {

	if (argc>0 && !strcmp("-l", argv[0])) {
		return cmd_dir_int(sockfd, 2, argc, argv);
	} else {
		return cmd_dir_int(sockfd, 1, argc, argv);
	}
}


