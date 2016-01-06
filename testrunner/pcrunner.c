
/****************************************************************************

    xd2031 filesystem server - socket test runner
    Copyright (C) 2012,2014 Andre Fachat

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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>

#include "log.h"
#include "mem.h"
#include "terminal.h"
#include "registry.h"
#include "wireformat.h"
#include "script.h"
#include "connect.h"


static int trace = 0;

void usage(int rv) {
        printf("Usage: fsser [options] run_directory\n"
                " options=\n"
                "   -d <device> define serial device to use\n"
                "   -v          verbose\n"
                "   -t          trace send/received data\n"
                "   -?          gives you this help text\n"
        );
        exit(rv);
}

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



scriptlet_tab_t scriptlets[] = {
	{ "len", 3, 1, NULL, exec_len },
	{ "dsb", 3, 0, scr_dsb, NULL },
	{ "ign", 3, 1, NULL, exec_ign },
	{ NULL, 0, 0, NULL, NULL }
};




static int parse_init(line_t *line, const char *in, char **outbuf, int *outlen) {
	(void) in; // silence
	(void) line; // silence
	*outlen = 3;
	*outbuf = mem_alloc_c(3, "init_reset_response_buffer");
	(*outbuf)[0] = FS_RESET;
	(*outbuf)[1] = 0x03;
	(*outbuf)[2] = 0x7d;

	return 0;
}

cmd_tab_t cmds[] = {
	{ "expect", 6, parse_buf },
	{ "send", 4, parse_buf },
	{ "message", 7, parse_msg },
	{ "errmsg", 6, parse_msg },
	{ "init", 4, parse_init },
	{ NULL, 0, NULL }
};

#define CMD_EXPECT	0
#define CMD_SEND	1
#define CMD_MESSAGE	2
#define CMD_ERRMSG	3
#define CMD_INIT	4


 
// -----------------------------------------------------------------------


void send_sync(int fd) {
	log_debug("send sync\n");

	char buf[128];

	for (int i = sizeof(buf)-1; i >= 0; i--) {
		buf[i] = FS_SYNC;
	}

	write(fd, buf, sizeof(buf));
}


char buf[8192];
int wrp = 0;
int rdp = 0;

int read_packet(int fd, char *outbuf, int buflen) {

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

// -----------------------------------------------------------------------

int compare_packet(int fd, const char *inbuffer, const char *mask, const int inbuflen, int curpos) {
	char buffer[8192];
	ssize_t cnt = 0;
	int err = 0;

	cnt = read_packet(fd, buffer, sizeof(buffer));


	if (cnt < 0) {
		log_errno("Error reading from socket at line %d\n", curpos);
		err = 2;
	} else {
		if (inbuflen > 0) {
			// only check data when we actually expect something
			if (cnt == inbuflen) {
				// expected length
				if (mask == NULL) {
					if (memcmp(inbuffer, buffer, inbuflen)) {
						log_error("Detected mismatch at line %d\n", curpos);
						err = 1;
					}
				} else {
					for (int i = 0; i < inbuflen; i++) {
						if ((inbuffer[i] ^ buffer[i]) & mask[i]) {
							log_error("Detected mismatch at line %d\n", curpos);
							err = 1;
						}
					}
				}
			} else {
				// length mismatch
				err = 1;
			}
		}
		// print data lines
		if (trace || err) {
			log_hexdump2(buffer, cnt, 0, "Rxd   : ");
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

	// current "pc" pointer to script line
	int curpos = 0;	
	line_t *line = NULL;
	int lineno = 0;

	int err = 0;
	ssize_t size = 0;
	line_t *errmsg = NULL;
	scriptlet_t *scr = NULL;

	while ( (err == 0) && (line = reg_get(script, curpos)) != NULL) {

		lineno = line->num;

		switch (line->cmd) {
		case CMD_COMMENT:
			if (!trace) {
				break;
			}
			// fall-through
		case CMD_MESSAGE:
			log_info("> %s\n", line->buffer);
			break;
		case CMD_ERRMSG:
			errmsg = line;
			break;
		case CMD_SEND:
			for (int i = 0; (scr = reg_get(&line->scriptlets, i)) != NULL; i++) {
				if (scr->exec != NULL) {
					scr->exec(line, scr);
				}
			}
			if (trace) {
				log_hexdump2(line->buffer, line->length, 0, "Send  : ");
			}

			size = write(sockfd, line->buffer, line->length);
			if (size < 0) {
				log_errno("Error writing to socket at line %d\n", lineno);
				err = -1;
			}
			break;
		case CMD_EXPECT:
			line->mask = mem_alloc_c(line->length, "line_mask");
			memset(line->mask, 0xff, line->length);
			for (int i = 0; (scr = reg_get(&line->scriptlets, i)) != NULL; i++) {
				if (scr->exec != NULL) {
					scr->exec(line, scr);
				}
			}
			err = compare_packet(sockfd, line->buffer, line->mask, line->length, lineno);
			mem_free(line->mask);
			line->mask = NULL;

			if (err != 0) {
				if (errmsg != NULL) {
					log_error("> %d: %s -> %d\n", lineno, errmsg->buffer, err);
				}
				return err;
			}
			break;
		case CMD_INIT:
			send_sync(sockfd);
			err = compare_packet(sockfd, line->buffer, NULL, line->length, lineno);
			if (err != 0) {
				if (errmsg != NULL) {
					log_error("> %d: %s -> %d\n", lineno, errmsg->buffer, err);
				}
				return err;
			}
			break;
		}
		curpos++;
	}

	return 0;
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

			rv = execute_script(sockfd, script);
		}
	}

	
	return rv;
}

