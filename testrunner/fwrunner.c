
/****************************************************************************

    xd2031 filesystem server - socket test runner
    Copyright (C) 2014 Andre Fachat

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

/**
 * testrunner.c
 *
 * Part of the xd2031 file system server
 * 
 * this code connects to a Unix socket and uses a script to send
 * commands, resp. receive replies and check whether they are correct
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>

#include "terminal.h"
#include "log.h"
#include "registry.h"
#include "sock488.h"
#include "script.h"
#include "connect.h"

void usage(int rv) {
        printf("Usage: fsser [options] run_directory\n"
                " options=\n"
                "   -d <device> define serial device to use\n"
                "   -v          set verbose\n"
                "   -t          trace all send/receive data\n"
                "   -?          gives you this help text\n"
        );
        exit(rv);
}

static int trace = 0;

// Assert switch is a single character
// If somebody tries to combine options (e.g. -vD) or
// encloses the parameter in quotes (e.g. fsser "-d COM5")
// this function will throw an error
void assert_single_char(char *argv) {
        if (strlen(argv) != 2) {
                log_error("Unexpected trailing garbage character%s '%s' (%s)\n",
                                strlen(argv) > 3 ? "s" : "", argv + 2, argv);
                exit (1);
        }
}



int scr_unlisten(char *trg, int trglen, const char **parseptr, int *outparam) {
	(void) trglen;	// silence warning
	(void) parseptr;// silence warning
	(void) outparam; // silence warning

	trg[0] = 0x3f;
	return 1;
}
int scr_untalk(char *trg, int trglen, const char **parseptr, int *outparam) {
	(void) trglen;	// silence warning
	(void) parseptr;// silence warning
	(void) outparam; // silence warning

	trg[0] = 0x5f;
	return 1;
}


static int scr_atn(char *trg, int trglen, const char **parseptr, char cmd) {

	(void) trglen;	// silence warning

	char addr = 0;
	const char *p = *parseptr;

	while (isspace(*p)) { p++; }

	p = parse_hexbyte(p, &addr);
	if (p != NULL) {	

		while (isspace(*p)) { p++; }
	}
	*parseptr = p;

	if (addr > 0x1e) {
		log_warn("ATN byte out of range (>0x1e) %02x\n", addr);
	}

	trg[0] = cmd | addr;

	return 1;
}

int scr_listen(char *trg, int trglen, const char **parseptr, int *outparam) {
	(void) outparam; // silence warning
	return scr_atn(trg, trglen, parseptr, 0x20);
}
int scr_talk(char *trg, int trglen, const char **parseptr, int *outparam) {
	(void) outparam; // silence warning
	return scr_atn(trg, trglen, parseptr, 0x40);
}
int scr_secondary(char *trg, int trglen, const char **parseptr, int *outparam) {
	(void) outparam; // silence warning
	return scr_atn(trg, trglen, parseptr, 0x60);
}

scriptlet_tab_t scriptlets[] = {
	{ "len", 3, 1, NULL, exec_len },
	{ "dsb", 3, 0, scr_dsb, NULL },
	{ "talk", 4, 1, scr_talk, NULL },
	{ "listen", 6, 1, scr_listen, NULL },
	{ "secondary", 9, 1, scr_secondary, NULL },
	{ "untalk", 6, 1, scr_untalk, NULL },
	{ "unlisten", 8, 1, scr_unlisten, NULL },
	{ "ignore", 6, 0, scr_ignore, exec_ignore },
	{ NULL, 0, 0, NULL, NULL }
};

cmd_tab_t cmds[] = {
	{ "atn", 3, parse_buf },
	{ "send", 4, parse_buf },
	{ "recv", 4, parse_buf },
	{ "errmsg", 6, parse_msg },
	{ "message", 7, parse_msg },
	{ "expect", 6, parse_buf },
	{ "sendnoeof", 9, parse_buf },
	{ NULL, 0, NULL }
};


#define CMD_ATN		0
#define CMD_SEND	1
#define CMD_RECEIVE	2
#define CMD_ERRMSG	3
#define CMD_MESSAGE	4
#define CMD_EXPECT	5
#define CMD_SENDNOEOF	6	/* like send, but do not set EOF on last byte */


// -----------------------------------------------------------------------



char buf[8192];
int wrp = 0;
int rdp = 0;


int read_byte(int fd, char *data) {
	int n;
	while ((n = read(fd, data, 1)) <= 0) {
		if (n < 0) {
			if (errno != EAGAIN && errno != EWOULDBLOCK) {
			   	log_error("testrunner: read error %d (%s)\n",
                       			errno,strerror(errno));
                            		return -1;
			}
		}
	}
	//printf("%02x ", *data);
	return 0;
}

/*
 * buflen is the expected length of data
 */
int read_packet(int fd, char *outbuf, int buflen, int *outeof) {

	char incmd, outcmd;
        int n, eof, tout;

	*outeof = 0;
	wrp = 0;

	outcmd = S488_REQ;
	do {
		n = write(fd, &outcmd, 1);
		
		n = read_byte(fd, &incmd);
		eof = incmd & S488_EOF;
		tout = incmd & S488_TIMEOUT;
		incmd &= ~(S488_EOF | S488_TIMEOUT);
		if (tout == 0) {
			if (incmd == S488_OFFER) {
				n = read_byte(fd, outbuf + wrp);
				wrp++;
			}
			outcmd = S488_REQ | S488_ACK;
		} else {
			outcmd = S488_REQ;
		}

	} while((n == 0) && (wrp < buflen) && (!eof));

	if (n == 0) {
		outcmd = S488_ACK;
		n = write(fd, &outcmd, 1);
	}
		
	*outeof = eof;

	return (n < 0) ? -1 : wrp;
}

// -----------------------------------------------------------------------

int compare_packet(int fd, const char *inbuffer, const int inbuflen, int curpos, int waitforeof, 
		registry_t *scriptlets, line_t *line) {
	char buffer[4096];
	ssize_t cnt = 0;
	int err = 0;
	int eof = 0;
	int scrp = 0;

	if (waitforeof) {
		cnt = read_packet(fd, buffer, sizeof(buffer), &eof);
	} else {
		cnt = read_packet(fd, buffer, inbuflen, &eof);
	}

	//log_info("Rxd   : ");

	scriptlet_t *scr = reg_get(scriptlets, scrp);
	int curp = 0;
	int offset = 0;

	if (cnt < 0) {
		log_errno("Error reading from socket at line %d\n", curpos);
		err = 2;
	} else {
		do {
			offset = 0;
			if (scr != NULL && scr->pos == curp) {
				if (scr->exec != NULL) {
					offset = scr->exec(line, scr);
				}
				scrp++;
				scr = reg_get(scriptlets, scrp);
			}
			if (offset == 0) {
				if (inbuffer[curp] != buffer[curp]) {
					log_error("Detected mismatch at line %d\n", curpos);
					err = 1;
					break;
				}
				offset = 1;
			}
			curp += offset;
		} while (curp < inbuflen && curp < cnt);

		if (cnt != inbuflen) {
			log_error("Detected length mismatch: expected %d, received %d\n", inbuflen, cnt);
			err = 1;
		}
		// print lines
		if (trace || err) {
			log_hexdump2(buffer, cnt, 0, eof ? "Rx Eof: " : "Rxd   : ");
		}
		if (err) {
			log_hexdump2(inbuffer, inbuflen, 0, "Expect: ");
		}
	}
	return err;
}


/**
 * returns the status of the execution
 *  0 = normal end
 *  1 = expect mismatch
 */
int execute_script(int sockfd, registry_t *script) {

	char cmd = 0;

	int numerrs = 0;

	// current "pc" pointer to script line
	int curpos = 0;	
	int lineno = 0;
	line_t *line = NULL;

	int err = 0;
	ssize_t size = 0;
	line_t *errmsg = NULL;
	scriptlet_t *scr = NULL;

	while ( (err == 0) && (line = reg_get(script, curpos)) != NULL) {

		lineno = line->num;

		cmd = S488_SEND;

		switch (line->cmd) {
		case CMD_MESSAGE:
			log_info("> %s\n", line->buffer);
			curpos++;
			break;
		case CMD_ERRMSG:
			errmsg = line;
			curpos++;
			break;
		case CMD_ATN:
			cmd = S488_ATN;
			// fall-through
		case CMD_SEND:
		case CMD_SENDNOEOF:
			for (int i = 0; (scr = reg_get(&line->scriptlets, i)) != NULL; i++) {
				if (scr->exec != NULL) {
					scr->exec(line, scr);
				}
			}
			if (trace) {
				log_hexdump2(line->buffer, line->length, 0, (line->cmd == CMD_SEND) ? "Send  : " : "Send_A: ");
			}

			for (int i = 0; i < line->length; i++) {
				if (((i+1) == line->length) && (line->cmd == CMD_SEND)) {
					// TODO explicit EOF handling
					cmd |= S488_EOF;
				}
				size = write(sockfd, &cmd, 1);
				if (size < 0) {
					log_errno("Error writing to socket at line %d\n", lineno);
					err = -1;
				}
				size = write(sockfd, line->buffer+i, 1);
				if (size < 0) {
					log_errno("Error writing to socket at line %d\n", lineno);
					err = -1;
				}
			}
			curpos++;
			break;
		case CMD_RECEIVE:
		case CMD_EXPECT:
			//for (int i = 0; (scr = reg_get(&line->scriptlets, i)) != NULL; i++) {
			//	if (scr->exec != NULL) {
			//		scr->exec(line, scr);
			//	}
			//}
			err = compare_packet(sockfd, line->buffer, line->length, curpos, line->cmd == CMD_RECEIVE, &line->scriptlets, line);

			if (err != 0) {
				numerrs++;
				if (errmsg != NULL) {
					log_error("> %d: %s -> %d\n", lineno, errmsg->buffer, err);
				}
				if (err > 1) {
					// fatal
					return numerrs;
				}
				err = 0;
			}
			curpos++;
			break;
		}
	}

	return numerrs;
}

// -----------------------------------------------------------------------

int main(int argc, char *argv[]) {

	int rv = -1;

	const char *device = NULL;
	const char *scriptname = NULL;

	// wait for socket if not there right away?
	int dowait = 0;

	terminal_init();


        int i=1;
        while(i<argc && argv[i][0]=='-' && argv[i][1] != 0) {
          	switch(argv[i][1]) {
            	case '?':
                	assert_single_char(argv[i]);
                	usage(EXIT_SUCCESS);    /* usage() exits already */
                	break;
            	case 'd':
                	assert_single_char(argv[i]);
                	if (i < argc-1) {
                  		i++;
                  		device = argv[i];
                  		log_info("main: device = %s\n", device);
                	} else {
                  		log_error("-d requires <device> parameter\n");
                  		exit(1);
                	}
                	break;
		case 'v':
			set_verbose();
			break;
		case 't':
			trace = 1;
			break;
		case 'w':
			dowait = 1;
			break;
            	default:
                	log_error("Unknown command line option %s\n", argv[i]);
                	usage(1);
                	break;
          	}
          	i++;
        }

	// next parameter is the script name
	if (i >= argc) {
		log_error("Script name parameter missing!\n");
		return -1;
	}

	scriptname = argv[i];
	i++;

	registry_t *script = load_script_from_file(scriptname);

	if (script != NULL) {

		int sockfd = socket_open(device, dowait);
	
		if (sockfd >= 0) {

			// returns the number of errors in script
			rv = execute_script(sockfd, script);
		}
	}

	
	return rv;
}

