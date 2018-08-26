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
#include "log.h"
#include "terminal.h"
#include "connect.h"
#include "wireformat.h"
#include "name.h"
#include "xdcmd.h"


// --------------------------------------------------------------------------
// send/receive packets


int send_packet(int sockfd, const uint8_t *buf, int len) {

	int p = 0;
	
	do {

		ssize_t r = write(sockfd, buf+p, len-p);
		if (r < 0) {
			return -1;
		}

		p += r;	
		len -= r;

	} while (len > 0);

	return p;
}

// convenience
int send_cmd(int sockfd, uint8_t cmd, uint8_t fd) {

	uint8_t buf[FSP_DATA];

	buf[FSP_CMD] = cmd;
	buf[FSP_FD] = fd;
	buf[FSP_LEN] = FSP_DATA; 

	return send_packet(sockfd, buf, FSP_DATA);
}

// convenience
int send_longcmd(int sockfd, uint8_t cmd, uint8_t fd, nameinfo_t *ninfo) {

	uint8_t *buf = mem_alloc_c(256, "longcmd buffer");

	if (ninfo->trg.drive == NAMEINFO_UNUSED_DRIVE) {
		// default drive
		ninfo->trg.drive = 0;
	}

	uint8_t len = assemble_filename_packet(buf+FSP_DATA, ninfo);
	len += FSP_DATA;

	buf[FSP_CMD] = cmd;
	buf[FSP_FD] = fd;
	buf[FSP_LEN] = len; 

	int rv = send_packet(sockfd, buf, len);

	mem_free(buf);

	return rv;
}

// TODO: copy from pcrunner.c, need to put in common code
// Note: also similarly in the server (with async reads)
// besides: I think this actually fails for partial reads as wrp=rdp=0 at the start

char buf[8192];
int wrp = 0;
int rdp = 0;

int recv_packet(int fd, uint8_t *outbuf, int buflen) {

	(void) buflen; // TODO

        int plen, cmd;
        int n;

        wrp = 0;
        rdp = 0;

        for(;;) {

              // as long as we have more than FSP_LEN bytes in the buffer
              // i.e. 2 or more, we loop and process packets
              // FSP_LEN is the position of the packet length
              while(wrp-rdp > FSP_LEN) {
                // first byte in packet is command, second is length of packet
                plen = 255 & buf[rdp+FSP_LEN];  //  AND with 255 to fix sign
                cmd = 255 & buf[rdp+FSP_CMD];   //  AND with 255 to fix sign
                // full packet received already?
                if (cmd == FS_SYNC) {
                  // the byte is the FS_SYNC command
                  // mirror it back
                  write(fd, buf+rdp+FSP_CMD, 1);
                  rdp++;
                } else
                if (plen < FSP_DATA) {
                  // a packet is at least 3 bytes (when with zero data length)
                  // so ignore byte and shift one in buffer position
                  rdp++;
                } else
                if(wrp-rdp >= plen) {
                  // did we already receive the full packet?
                  // yes, then execute
                  //printf("dispatch @rdp=%d [%02x %02x ... ]\n", rdp, buf[rdp], buf[rdp+1]);
                  memcpy(outbuf, buf+rdp, plen);
                  rdp +=plen;
                  return plen;
                } else {
                  // no, then break out of the while, to read more data
                  break;
                }
              }

              n = read(fd, buf+wrp, 8192-wrp);
              //log_debug("read->%d\n", n);
              if (n == 0) {
                return 0;
              }

              if(n < 0) {
                log_error("testrunner: read error %d (%s)\n",
                        errno,strerror(errno));
                return -1;
              }
              wrp+=n;
              if(rdp && (wrp==8192 || rdp==wrp)) {
                if(rdp!=wrp) {
                  memmove(buf, buf+rdp, wrp-rdp);
                }
                wrp -= rdp;
                rdp = 0;
              }

            }
}

// --------------------------------------------------------------------------

static int cmd_info(int sockfd, int argc, const char *argv[]) {

	log_debug("cmd_info(sockfd=%d, argc=%d, argv[]=%s\n",
		sockfd, argc, argc>0 ? argv[0] : "-");

	// send command
	uint8_t *buf = mem_alloc_c(256, "info buffer");

	int rv = send_cmd(sockfd, FS_INFO, FSFD_CMD);
	if (rv >= 0) {
		rv = recv_packet(sockfd, buf, 256);

		if (rv < 0) {
			log_errno("Could not receive packet!\n");
			exit(1);
		}

		log_info("Info: %s\n", buf+3);
	}

	mem_free(buf);	
	return 0;
}

static int cmd_assign(int sockfd, int argc, const char *argv[]) {

	log_error("cmd_assign(sockfd=%d, argc=%d, argv[]=%s) - not yet implemented!\n",
		sockfd, argc, argc>0 ? argv[0] : "-");

	char *buf = mem_alloc_c(4, "assign buffer");


	mem_free(buf);	
	return 0;
}

// --------------------------------------------------------------------------
// command dispatch code

typedef struct {
	const char *name;
	int (*func)(int sockfd, int argc, const char *argv[]);
	const char *usage;
	const char *usageopts[10];
} cmdtab_t;

static const cmdtab_t cmdtab[] = {
	{	"info",		cmd_info,
			"Get info from the server",
			{ NULL } },
	{	"assign",	cmd_assign,
			"Assign a drive to a new drivespec with parameters:",
			{ 	"<drivespec>    '<drive>:[<prov>]=<path>'", 
			  	"               E.g. '0:fs=/home/user/.xdsamples'",
				"               For more details see the server documentation.",
			NULL }},
	{	"dir",	cmd_dir,
			"Show the directory of the given drive in CBM format, optionally filtered by a pattern:",
			{	"[[<drive:>]<pattern>]",
			NULL }},
	{	"ls",	cmd_ls,
			"Show the directory of the given drive in Unix format, optionally filtered by a pattern:",
			{	"[options] [[<drive:>]<pattern>]",
				"Options:",
				"-l                  long format",
			NULL }},
	{	"put",	cmd_put,
			"Put file(s) from the local filesystem to the CBM server filesystem. Multiple source files are concatenated.",
			{	"[options] <local_file> <drive:>[<target_file>]",
				"[options] <local_file_1> [ <local_file_2> ... ] <drive:>[<target_file>]",
				"[options] <local_and_target_file>",
				"Options:",
				"-f                  overwrite existing file (default is not to overwrite)",
			NULL }},
	{	"get",	cmd_get,
			"Get file(s) from the CBM server filesystem. Multiple source files are concatenated.",
			{	"[options] <drive:><src_file_1> [ <drive:><src_file_2> ... ] <local_target_file>",
				"[options] <source_and_local_file>",
				"Options:",
				"-f                  overwrite existing files (default is not to overwrite)",
			NULL }},
	{	"rm",	cmd_rm,
			"Delete file(s) from the CBM server filesystem. Multiple files or patterns can be specified.",
			{	"<drive:><file_pattern_1> [ <drive:><file_pattern_2> ... ]",
			NULL }},
};

const int numcmds = sizeof(cmdtab) / sizeof(cmdtab_t);

// --------------------------------------------------------------------------

// TODO: move cmdline handling into common code

void usage(int rv) {
        printf("Usage: xdcmd [options] <command> [command options]\n"
                " options=\n"
                "   -T <socket> define socket device to use (instead of device)\n"
                "   -?          gives you this help text\n"
		" Commands have own options and usually include '--' to end options list\n"
		" commands:\n"
        );
	for (int i = 0; i < numcmds; i++) {
		printf(" %-8s   %s\n", cmdtab[i].name, cmdtab[i].usage);
		for (int j = 0; cmdtab[i].usageopts[j]; j++) {
			printf("            %s\n", cmdtab[i].usageopts[j]);
		}
	}
        exit(rv);
}


// Assert switch is a single character
// If somebody tries to combine options (e.g. -vD) or
// encloses the parameter in quotes (e.g. fsser "-d COM5")
// this function will throw an error
void assert_single_char(const char *argv) {
        if (strlen(argv) != 2) {
                log_error("Unexpected trailing garbage character%s '%s' (%s)\n",
                                strlen(argv) > 3 ? "s" : "", argv + 2, argv);
                exit (EXIT_RESPAWN_NEVER);
        }
}

// --------------------------------------------------------------------------

int main(int argc, const char *argv[]) {

	const char *socket = NULL;

	mem_init();
	atexit(mem_exit);

	terminal_init();

	// --------------------------------------------
	// parse the parameters

        // Check -v (verbose) first to enable log_debug()
        // when processing other options
        for (int i=1; i < argc; i++) if (!strcmp("-v", argv[i])) set_verbose(1);

	int p = 1;
	while (p < argc && argv[p][0]=='-') {

		switch(argv[p][1]) {
		case 0:
			// single '-' option ends parameter processing
			p++;
			goto endpars;
		case 'v':
                	assert_single_char(argv[p]);
			// verbose is already checked above
			set_verbose(1);
			break;
            	case '?':
                	assert_single_char(argv[p]);
                	usage(EXIT_SUCCESS);    /* usage() exits already */
                	break;
		case 'T':
			assert_single_char(argv[p]);
                	if (p < argc-2) {
                  		p++;
                  		socket = argv[p];
                  		log_info("main: tools socket = %s\n", socket);
                	} else {
                  		log_error("-T requires <socket name> parameter\n");
                  		exit(EXIT_RESPAWN_NEVER);
                	}
                	break;
            	default:
                	log_error("Unknown command line option %s\n", argv[p]);
                	usage(EXIT_RESPAWN_NEVER);
                	break;
		}
		p++;
	}
endpars:
	// --------------------------------------------
	// open the socket

        if (socket == NULL) {
                const char *home = os_get_home_dir();
                socket = malloc_path(home, ".xdtools");
        }

	int sockfd = socket_open(socket, 0);
	if (sockfd < 0) {
		log_errno("Could not open socket %s\n", socket);
		mem_free(socket);
		exit(1);
	}
	mem_free(socket);

	// --------------------------------------------
	// find our command either as ending part of the name of the binary ...
	

	const cmdtab_t *cmd = NULL;
	int l = strlen(argv[0]);

	for (int i = 0; i < numcmds; i++) {
		int cl = strlen(cmdtab[i].name);

		if ((cl <= l)
			&& !strcmp(cmdtab[i].name, &argv[0][l-cl])) {
			cmd = &cmdtab[i];
			break;
		}
	}
	// ... or as command line parameter
	if (p < argc) {
		l = strlen(argv[p]);
		if (cmd == NULL) {
			for (int i = 0; i < numcmds; i++) {
				int cl = strlen(cmdtab[i].name);

				if ((cl <= l)
					&& !strcmp(cmdtab[i].name, argv[p])) {
					cmd = &cmdtab[i];
					p++;
					break;
				}
			}
		}
	}

	if (cmd == NULL) {
		log_error("Could not identify any of the commands!\n");
		usage(1);
	}	

	int rv = cmd->func(sockfd, argc-p, argv+p);


	close(sockfd);

	return rv;
}

