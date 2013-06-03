/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2012 Andre Fachat

    Derived from:
    OS/A65 Version 1.3.12
    Multitasking Operating System for 6502 Computers
    Copyright (C) 1989-1997 Andre Fachat

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

/*
 * This file is a server FSTCP filesystem implementation, to be
 * used with the FSTCP program on an OS/A65 computer. 
 *
 * In this file the actual command work is done
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#include "wireformat.h"
#include "fscmd.h"
#include "dir.h"
#include "charconvert.h"
#include "provider.h"
#include "log.h"
#include "xcmd.h"
#include "channel.h"

#define DEBUG_CMD
#undef DEBUG_CMD_TERM
#undef DEBUG_READ
#undef DEBUG_WRITE

#define	MAX_BUFFER_SIZE			64

static void cmd_dispatch(char *buf, int fs);
static void write_packet(int fd, char *retbuf);


//------------------------------------------------------------------------------------
// debug log helper

#if defined DEBUG_CMD || defined DEBUG_WRITE

const char *nameofcmd(int cmdno) {
	switch (cmdno) {
	case FS_TERM:		return "TERM";
	case FS_OPEN_RD:	return "OPEN_RD";
	case FS_OPEN_WR:	return "OPEN_WR";
	case FS_OPEN_RW:	return "OPEN_RW";
	case FS_OPEN_AP:	return "OPEN_AP";
	case FS_OPEN_OW:	return "OPEN_OW";
	case FS_OPEN_DR:	return "OPEN_DR";
	case FS_READ:		return "READ";
	case FS_WRITE:		return "WRITE";
	case FS_REPLY:		return "REPLY";
	case FS_EOF:		return "EOF";
	case FS_SEEK:		return "SEEK";
	case FS_CLOSE:		return "CLOSE";
	case FS_MOVE:		return "MOVE";
	case FS_DELETE:		return "DELETE";
	case FS_FORMAT:		return "FORMAT";
	case FS_CHKDSK:		return "CHKDSK";
	case FS_RMDIR:		return "RMDIR";
	case FS_MKDIR:		return "MKDIR";
	case FS_CHDIR:		return "CHDIR";
	case FS_ASSIGN:		return "ASSIGN";
	case FS_SETOPT:		return "SETOPT";
	case FS_RESET:		return "RESET";
	case FS_BLOCK:		return "BLOCK";
	case FS_DIRECT:		return "DIRECT";
	case FS_GETDATIM:	return "GETDATIM";
	case FS_CHARSET:	return "CHARSET";
	default:		return "???";
	}
}

#endif


//------------------------------------------------------------------------------------
//


void cmd_init() {
	provider_init();
	channel_init();
	xcmd_init();

	// default
	provider_set_ext_charset("PETSCII");
}

/**
 * take the command line, search for "-A<driv>=<name>" parameters and assign
 * the value
 */
void cmd_assign_from_cmdline(int argc, char *argv[]) {

	for (int i = 0; i < argc; i++) {

		if (argv[i][0] != '-') {
			continue;
		}

		if ((strlen(argv[i]) >2) 
			&& argv[i][1] == 'X') {

			if (strchr(argv[i]+2, ':') == NULL) {
				// we need a ':' as separator between bus name and actual command
				log_error("Could not find bus name separator ':' in '%s'\n", argv[i]+2);
				continue;
			}

			xcmd_register(argv[i]+2);
		}
		if ((strlen(argv[i]) >4) 
			&& argv[i][1] == 'A') {

			if (!isdigit(argv[i][2])) {
				log_error("Could not identify %c as drive number!\n", argv[i][2]);
				continue;
			}

			if (argv[i][3] != ':') {
				log_error("Could not identify %s as ASSIGN parameter\n", argv[i]);
				continue;
			}
	
			// int rv = provider_assign(argv[i][2] & 0x0f, &(argv[i][4]));
			int rv;
			int drive = argv[i][2] & 0x0f;
			char provider_name[MAX_LEN_OF_PROVIDER_NAME + 1];
			char *provider_parameter;

			// provider name followed by parameter?
			char *p = strchr(argv[i], '=');
			if (p) {
				if ((p - argv[i] - 4) > MAX_LEN_OF_PROVIDER_NAME) {
					log_error("Provider name '%.8s'.. exceeds %d characters\n", argv[i] + 4,
						  MAX_LEN_OF_PROVIDER_NAME);
				} else {
					strncpy (provider_name, argv[i] + 4, p - argv[i] +1);
					provider_name[p - argv[i] - 4] = 0;
					provider_parameter = p + 1;
					log_debug("cmdline_assign '%s' = '%s'\n", provider_name, 
							provider_parameter);
					rv = provider_assign(drive, provider_name, provider_parameter);
				}
			} else {
				log_debug("No parameter for cmdline_assign\n");
				rv = provider_assign(drive, argv[i] + 4, NULL);
			} 
			if (rv < 0) {
				log_error("Could not assign, error number is %d\n", rv);
			}
		}
	}
}

static void cmd_sync(int readfd, int writefd) {
	char syncbuf[1];

	// first sync the device and the server.
	// I.e. we send an FS_SYNC byte, and wait for the sync byte return,
	// ignoreing all other bytes
	//
	// write the sync byte
	syncbuf[0] = FS_SYNC;
	int e = 0;
	do {
#if defined DEBUG_WRITE || defined DEBUG_CMD
		printf("sync write %02x\n", syncbuf[0]);
#endif
		e = write(writefd, syncbuf, 1);
	} while (e == 0 || (e < 0 &&  errno == EAGAIN));
	if (e < 0) {
		log_errno("Error on sync write", errno);
	}
	// wait for a sync byte
	int n = 0;
	do {
		n = read(readfd, syncbuf, 1);
#ifdef DEBUG_READ
	        printf("sync read %d bytes: ",n);
	        for(int i=0;i<n;i++) printf("%02x ",255&syncbuf[i]); printf("\n");
#endif
		if (n < 0) {
			log_errno("Error on sync read", errno);
		}
	} while (n != 1 || (0xff & syncbuf[0]) != FS_SYNC);
}


static void cmd_sendxcmd(int writefd, char buf[]) {
	// now send all the X-commands
	int ncmds = xcmd_num_options();
	log_debug("Got %d options to send:\n", ncmds);
	for (int i = 0; i < ncmds; i++) {
		const char *opt = xcmd_option(i);
		log_debug("Option %d: %s\n", i, opt);

		int len = strlen(opt);
		if (len > MAX_BUFFER_SIZE - FSP_DATA) {
			log_error("Option is too long to be sent: '%s'\n", opt);
		} else {
			buf[FSP_CMD] = FS_SETOPT;
			buf[FSP_LEN] = FSP_DATA + len + 1;
			buf[FSP_FD] = FSFD_SETOPT;
			strncpy(buf+FSP_DATA, opt, MAX_BUFFER_SIZE);
			buf[FSP_DATA + len] = 0;

			// TODO: error handling
			write_packet(writefd, buf);
		}
	}
}

static void cmd_sendreset(int writefd, char buf[]) {
	buf[FSP_CMD] = FS_RESET;
	buf[FSP_LEN] = FSP_DATA;
	buf[FSP_FD] = FSFD_SETOPT;
	write_packet(writefd, buf);
}

/**
 * this is the main loop of the program
 *
 * Here the data is read from the given readfd, put into a packet buffer,
 * then given to cmd_dispatch() for the actual execution, and the reply is 
 * again packeted and written to the writefd
 *
 * returns
 *   1 if read fails (errno gives more information)
 *   0 if other strange things happened
 */
int cmd_loop(int readfd, int writefd) {

        char buf[8192];
        int wrp,rdp,plen, cmd;
        int n;

	// sync device and server
	cmd_sync(readfd, writefd);

	// tell the device we've reset
	// (it will answer with FS_RESET, which gives us the chance to send the X commands)
	cmd_sendreset(writefd, buf);
	
        /* write and read pointers in the input buffer "buf" */
        wrp = rdp = 0;

        for(;;) {
	      n = read(readfd, buf+wrp, 8192-wrp);
#ifdef DEBUG_READ
	      printf("read %d bytes (wrp=%d, rdp=%d: ",n,wrp,rdp);
	      for(int i=0;i<n;i++) printf("%02x ",255&buf[wrp+i]); printf("\n");
#endif

              if(n <= 0) {
                fprintf(stderr,"fsser: read error %d (%s)\n",errno,strerror(errno));
		fprintf(stderr,"Did you power off your device?\n");
                return 1;
              }
              wrp+=n;
              if(rdp && (wrp==8192 || rdp==wrp)) {
                if(rdp!=wrp) {
                  memmove(buf, buf+rdp, wrp-rdp);
                }
                wrp -= rdp;
                rdp = 0;
              }
              // as long as we have more than FSP_LEN bytes in the buffer
              // i.e. 2 or more, we loop and process packets
              // FSP_LEN is the position of the packet length
              while(wrp-rdp > FSP_LEN) {
                // first byte in packet is command, second is length of packet
                plen = 255 & buf[rdp+FSP_LEN];	//  AND with 255 to fix sign
                cmd = 255 & buf[rdp+FSP_CMD];	//  AND with 255 to fix sign
                // full packet received already?
                if (cmd == FS_SYNC || plen < FSP_DATA) {
		  // a packet is at least 3 bytes (when with zero data length)
		  // or the byte is the FS_SYNC command
		  // so ignore byte and shift one in buffer position
		  rdp++;
		} else 
                if(wrp-rdp >= plen) {
		  // did we already receive the full packet?
                  // yes, then execute
		  //printf("dispatch @rdp=%d [%02x %02x ... ]\n", rdp, buf[rdp], buf[rdp+1]);
                  cmd_dispatch(buf+rdp, writefd);
                  rdp +=plen;
                } else {
		  // no, then break out of the while, to read more data
		  break;
                }
              }
            }

}


char *get_options(char *name, int len) {
	int l = strlen(name);
	if ((l + 1) < len) {
		return name + l + 1;
	}
	return NULL;
}

// ----------------------------------------------------------------------------------

/**
 * This function executes a single packet from the device.
 * 
 * It takes the values from the received buffer, and forwards the command
 * to the appropriate provider for further processing, using C-style arguments
 * (and not buffer + offsets).
 *
 * The return packet is written into the file descriptor fd
 */
static void cmd_dispatch(char *buf, int fd) {
	int tfd, cmd;
	unsigned int len;
	char retbuf[200];
	int rv;
	char *name2;

	cmd = buf[FSP_CMD];		// 0
	len = 255 & buf[FSP_LEN];	// 1

	if (cmd == FS_TERM) {
#ifdef DEBUG_CMD_TERM
		{
			int n = buf[FSP_LEN];
			log_debug("term: %d bytes @%p: ",n, buf);
			for(int i=0;i<n;i++) log_debug("%02x ",255&buf[i]); log_debug("\n");
		}
#endif

		if (len > 200) {
			log_error("TERM length exceeds 200! Is %d\n", len);
			len = 200;
		}
		memcpy(retbuf, buf+3, len-3);
		retbuf[len-3] = 0;
		log_term(retbuf);
		return;
	}

#ifdef DEBUG_CMD
	{
		int n = buf[FSP_LEN];
		log_debug("cmd %s :%d bytes @%p : ", nameofcmd(255&buf[FSP_CMD]), n, buf);
		for(int i=0;i<n;i++) log_debug("%02x ",255&buf[i]); log_debug("\n");
	}
#endif

	// not on FS_TERM
	tfd = buf[FSP_FD];
	if (tfd < 0 /*&& ((unsigned char) tfd) != FSFD_SETOPT*/) {
		log_error("Illegal file descriptor: %d\n", tfd);
		return;
	}

	// this stupidly overwrites the first byte of following commands
	// in case the new command has already been received (e.g. FS_CHARSET
	// on reset. This code is commented here as reminder just in case if
	// some error pops up by removing it
	//buf[(unsigned int)buf[FSP_LEN]] = 0;	// 0-terminator

	provider_t *prov = NULL; //&fs_provider;
	endpoint_t *ep = NULL;

#if 0
	printf("got cmd=%d, fd=%d, name=%s",cmd,tfd,
			(cmd<FS_ASSIGN)?((char*)buf+FSP_DATA):"null");
	for (int i = 3; i < buf[FSP_LEN]; i++) printf(" %02x", (unsigned int)buf[i]);
	printf("\n");
#endif
	retbuf[FSP_CMD] = FS_REPLY;
	retbuf[FSP_LEN] = FSP_DATA + 1;		// 4 byte default packet
	retbuf[FSP_FD] = tfd;
	retbuf[FSP_DATA] = ERROR_FAULT;

	int readflag = 0;
	int outdeleted = 0;
	int retlen = 0;
	int sendreply = 1;
	char *name = buf+FSP_DATA+1;
	int drive = buf[FSP_DATA]&255;
	int convlen = len - FSP_DATA-1;

	// options string just in case
	char *options = NULL;

	switch(cmd) {
		// file-oriented commands
	case FS_OPEN_WR:
	case FS_OPEN_OW:
		ep = provider_lookup(drive, &name);
		if (ep != NULL) {
			prov = (provider_t*) ep->ptype;
			if (prov->open_wr != NULL) {
				provider_convto(prov)(name, convlen, name, convlen);
				options = get_options(name, len - FSP_DATA - 1);
				log_info("OPEN_%s(%d->%s:%s)\n", (cmd==FS_OPEN_OW)?"OW":"WR", tfd, 
					prov->name, name);
				rv = prov->open_wr(ep, tfd, name, options, cmd == FS_OPEN_OW);
				retbuf[FSP_DATA] = rv;
				if (rv == 0) {
					channel_set(tfd, ep);
					break; // out of switch() to escape provider_cleanup()
				} else {
					log_rv(rv);
				}
			}
			// cleanup when not needed anymore
			provider_cleanup(ep);
		}
		break;
	case FS_OPEN_RW:
		ep = provider_lookup(drive, &name);
		if (ep != NULL) {
			prov = (provider_t*) ep->ptype;
			if (prov->open_rw != NULL) {
				provider_convto(prov)(name, convlen, name, convlen);
				options = get_options(name, len - FSP_DATA - 1);
				log_info("OPEN_RW(%d->%s:%s)\n", tfd, prov->name, name);
				rv = prov->open_rw(ep, tfd, name, options);
				retbuf[FSP_DATA] = rv;
				if (rv == 0) {
					channel_set(tfd, ep);
					break; // out of switch() to escape provider_cleanup()
				} else {
					log_rv(rv);
				}
			}
			// cleanup when not needed anymore
			provider_cleanup(ep);
		}
		break;
	case FS_OPEN_DR:
		//log_debug("Open directory for drive: %d\n", 0xff & buf[FSP_DATA]);
		ep = provider_lookup(drive, &name);
		if (ep != NULL) {
			prov = (provider_t*) ep->ptype;
			// not all providers support directory operation
			if (prov->opendir != NULL) {
				provider_convto(prov)(name, convlen, name, convlen);
				options = get_options(name, len - FSP_DATA - 1);
				log_info("OPEN_DR(%d->%s:%s)\n", tfd, prov->name, name);
				rv = prov->opendir(ep, tfd, name, options);
				retbuf[FSP_DATA] = rv;
				if (rv == 0) {
					channel_set(tfd, ep);
					break; // out of switch() to escape provider_cleanup()
				} else {
					log_rv(rv);
				}
			}
			// cleanup when not needed anymore
			provider_cleanup(ep);
		}
		break;
	case FS_OPEN_RD:
		ep = provider_lookup(drive, &name);
		if (ep != NULL) {
			prov = (provider_t*) ep->ptype;
			if (prov->open_rd != NULL) {
				provider_convto(prov)(name, convlen, name, convlen);
				options = get_options(name, len - FSP_DATA - 1);
				log_info("OPEN_RD(%d->%s:%s)\n", tfd, prov->name, name);
				rv = prov->open_rd(ep, tfd, name, options);
				retbuf[FSP_DATA] = rv;
				if (rv == 0) {
					channel_set(tfd, ep);
					break; // out of switch() to escape provider_cleanup()
				} else {
					log_rv(rv);
				}
			}
			// cleanup when not needed anymore
			provider_cleanup(ep);
		}
		break;
	case FS_OPEN_AP:
		ep = provider_lookup(drive, &name);
		if (ep != NULL) {
			prov = (provider_t*) ep->ptype;
			if (prov->open_ap != NULL) {
				provider_convto(prov)(name, convlen, name, convlen);
				options = get_options(name, len - FSP_DATA - 1);
				log_info("OPEN_AP(%d->%s:%s\n", tfd, prov->name, name);
				rv = prov->open_ap(ep, tfd, name, options);
				retbuf[FSP_DATA] = rv;
				if (rv == 0) {
					channel_set(tfd, ep);
					break; // out of switch() to escape provider_cleanup()
				} else {
					log_rv(rv);
				}
			}
			// cleanup when not needed anymore
			provider_cleanup(ep);
		}
		break;
	case FS_READ:
		ep = channel_to_endpoint(tfd);
		if (ep != NULL) {
			readflag = 0;	// default just in case
			prov = (provider_t*) ep->ptype;
			rv = prov->readfile(ep, tfd, retbuf + FSP_DATA, MAX_BUFFER_SIZE-FSP_DATA, &readflag);
			// TODO: handle error (rv<0)
			if (rv < 0) {
				// an error is sent as REPLY with error code
				retbuf[FSP_DATA] = rv;
				log_rv(-rv);
			} else {
				// a WRITE mirrors the READ request when ok 
				retbuf[FSP_CMD] = FS_WRITE;
				retbuf[FSP_LEN] = FSP_DATA + rv;
				if (readflag & READFLAG_EOF) {
					log_info("SEND_EOF(%d)\n", tfd);
					retbuf[FSP_CMD] = FS_EOF;
				}
				if (readflag & READFLAG_DENTRY) {
					log_info("SEND DIRENTRY(%d)\n", tfd);
					provider_convfrom(prov)(retbuf+FSP_DATA+FS_DIR_NAME, 
								strlen(retbuf+FSP_DATA+FS_DIR_NAME), 
								retbuf+FSP_DATA+FS_DIR_NAME, 
								strlen(retbuf+FSP_DATA+FS_DIR_NAME));
				}
			}
		}
		break;
	case FS_WRITE:
	case FS_EOF:
		ep = channel_to_endpoint(tfd);
		//printf("WRITE: chan=%d, ep=%p\n", tfd, ep);
		if (ep != NULL) {
			prov = (provider_t*) ep->ptype;
			if (cmd == FS_EOF) {
				log_info("CLOSE_BY_EOF(%d)\n", tfd);
			}
			rv = prov->writefile(ep, tfd, buf+FSP_DATA, len-FSP_DATA, cmd == FS_EOF);
			if (rv != 0) {
				log_rv(rv);
			}
			retbuf[FSP_DATA] = rv;
		}
		break;
	case FS_CLOSE:
		ep = channel_to_endpoint(tfd);
		if (ep != NULL) {
			prov = (provider_t*) ep->ptype;
			log_info("CLOSE(%d)\n", tfd);
			prov->close(ep, tfd);
			channel_free(tfd);
			retbuf[FSP_DATA] = ERROR_OK;
			// cleanup when not needed anymore
			provider_cleanup(ep);
		}
		break;

		// command operations
	case FS_DELETE:
		ep = provider_lookup(drive, &name);
		if (ep != NULL) {
			prov = (provider_t*) ep->ptype;
			if (prov->scratch != NULL) {
				provider_convto(prov)(name, convlen, name, convlen);
				log_info("DELETE(%s)\n", name);

				rv = prov->scratch(ep, name, &outdeleted);
				if (rv == ERROR_SCRATCHED) {
					retbuf[FSP_DATA + 1] = outdeleted > 99 ? 99 :outdeleted;
					retbuf[FSP_LEN] = FSP_DATA + 2;
				} else 
				if (rv != 0) {
					log_rv(rv);
				}
				retbuf[FSP_DATA] = rv;
			}
			// cleanup temp ep when not needed anymore
			provider_cleanup(ep);
		}
		break;
	case FS_MOVE:
		ep = provider_lookup(drive, &name);
		if (ep != NULL) {
			prov = (provider_t*) ep->ptype;
			if (prov->rename != NULL) {
				char *namefrom = strchr(name, 0);	// points to null byte
				int drivefrom = namefrom[1] & 255;	// from drive after null byte
				namefrom += 2;				// points to name after drive

				provider_convto(prov)(name, strlen(name), name, strlen(name));
				provider_convto(prov)(namefrom, strlen(namefrom), namefrom, strlen(namefrom));

				if (drivefrom != drive
					&& drivefrom != NAMEINFO_UNUSED_DRIVE) {
					// currently not supported
					// R <prov>:<name1>=<prov>:<name2>
					// drivefrom is UNUSED, then same as driveto
					log_warn("Drive spec combination not supported\n");

					rv = ERROR_DRIVE_NOT_READY;
				} else {
				
					log_info("RENAME(%s -> %s)\n", namefrom, name);
	
					rv = prov->rename(ep, name, namefrom);
					if (rv != 0) {
						log_rv(rv);
					}
				}
				retbuf[FSP_DATA] = rv;
			}
			// cleanup when not needed anymore
			provider_cleanup(ep);
		}
		break;
	case FS_CHDIR:
		ep = provider_lookup(drive, &name);
		if (ep != NULL) {
			prov = (provider_t*) ep->ptype;
			if (prov->cd != NULL) {
				provider_convto(prov)(name, convlen, name, convlen);
				log_info("CHDIR(%s)\n", name);
				rv = prov->cd(ep, name);
				if (rv != 0) {
					log_rv(rv);
				}
				retbuf[FSP_DATA] = rv;
			}
			// cleanup when not needed anymore
			provider_cleanup(ep);
		}
		break;
	case FS_MKDIR:
		ep = provider_lookup(drive, &name);
		if (ep != NULL) {
			prov = (provider_t*) ep->ptype;
			if (prov->mkdir != NULL) {
				provider_convto(prov)(name, convlen, name, convlen);
				log_info("MKDIR(%s)\n", name);
				rv = prov->mkdir(ep, name);
				if (rv != 0) {
					log_rv(rv);
				}
				retbuf[FSP_DATA] = rv;
			}
			// cleanup when not needed anymore
			provider_cleanup(ep);
		}
		break;
	case FS_RMDIR:
		ep = provider_lookup(drive, &name);
		if (ep != NULL) {
			prov = (provider_t*) ep->ptype;
			if (prov->rmdir != NULL) {
				provider_convto(prov)(name, convlen, name, convlen);
				log_info("RMDIR(%s)\n", name);
				rv = prov->rmdir(ep, name);
				if (rv != 0) {
					log_rv(rv);
				}
				retbuf[FSP_DATA] = rv;
			}
			// cleanup when not needed anymore
			provider_cleanup(ep);
		}
		break;
	case FS_DIRECT:
		// not file-related, so no file descriptor (tfd)
		// we only support mapped drives (thus name is NULL)
		ep = provider_lookup(drive, NULL);
		if (ep != NULL) {
			prov = (provider_t*) ep->ptype;
			if (prov->direct != NULL) {
				provider_convto(prov)(name, convlen, name, convlen);
				log_info("DIRECT(%d,...)\n", tfd);
				rv = prov->direct(ep, buf + FSP_DATA + 1, retbuf + FSP_DATA + 1, &retlen);
				if (rv != 0) {
					log_rv(rv);
				}
				retbuf[FSP_LEN] = FSP_DATA + 1 + retlen;
				retbuf[FSP_DATA] = rv;
			}
			// cleanup when not needed anymore
			provider_cleanup(ep);
		}
		break;
	case FS_BLOCK:
		// file-related Block commands (e.g. U1/U2)
		ep = channel_to_endpoint(tfd);
		if (ep != NULL) {
			prov = (provider_t*) ep->ptype;
			if (prov->block != NULL) {
				log_info("BLOCK(%d,...)\n", tfd);
				rv = prov->block(ep, tfd, buf+FSP_DATA);
				if (rv != 0) {
					log_rv(rv);
				}
				retbuf[FSP_DATA] = rv;
			}
		}
		break;
	case FS_ASSIGN:
		// assign an endpoint number (i.e. a drive number for the PET)
		// to a filesystem provider and path
		// The drive number in buf[FSP_DATA] is the one to assign,
		// name = buf[FSP_DATA+1] contains the zero-terminated provider name,
		// followed by a zero-terminated provider parameter.
		//
		// A provider can be determined relative to an existing one. In 
		// this case the provider name is the endpoint (drive) number,
		// and the path is interpreted as relative to an existing endpoint.
		// If the provider name is a real name, the path is absolute.
		//
		name2 = strchr(name, 0) + 2;

		provider_convto(prov)(name, strlen(name), name, strlen(name));
		provider_convto(prov)(name2, strlen(name2), name2, strlen(name2));

		log_info("ASSIGN(%d -> %s = %s)\n", drive, name, name2);
		rv = provider_assign(drive, name, name2);
		if (rv != 0) {
			log_rv(rv);
		}
		retbuf[FSP_DATA] = rv;
		break;
	case FS_RESET:
		log_info("RESET\n");
		// send the X command line options again
		cmd_sendxcmd(fd, retbuf);
		// we have already sent everything
		sendreply = 0;
		break;
	case FS_CHARSET:
		log_info("CHARSET: %s\n", buf+FSP_DATA);
		if (tfd == FSFD_CMD) {
			provider_set_ext_charset(buf+FSP_DATA);
			retbuf[FSP_DATA] = ERROR_OK;
		}
		break;
	default:
		log_error("Received unknown command: %d in a %d byte packet\n", cmd, len);
	}

	if (sendreply) {
		write_packet(fd, retbuf);
	}
}

static void write_packet(int fd, char *retbuf) {

	int e = write(fd, retbuf, 0xff & retbuf[FSP_LEN]);
	if (e < 0) {
		printf("Error on write: %d\n", errno);
	}
#if defined(DEBUG_WRITE) || defined(DEBUG_CMD)
	log_debug("write %02x %02x %02x (%s):", 255&retbuf[0], 255&retbuf[1],
			255&retbuf[2], nameofcmd(255&retbuf[FSP_CMD]) );
	for (int i = 3; i<retbuf[FSP_LEN];i++) log_debug(" %02x", 255&retbuf[i]);
	log_debug("\n");
#endif
}


