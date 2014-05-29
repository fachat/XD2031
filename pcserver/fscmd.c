/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2012,2014 Andre Fachat, Nils Eilers

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

#include "os.h"

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>

#include "wireformat.h"
#include "fscmd.h"
#include "charconvert.h"
#include "petscii.h"
#include "provider.h"
#include "dir.h"
#include "handler.h"
#include "log.h"
#include "xcmd.h"
#include "channel.h"
#include "serial.h"
#include "handler.h"

#define DEBUG_CMD
#undef DEBUG_CMD_TERM
#undef DEBUG_READ
#undef DEBUG_WRITE

#define	MAX_BUFFER_SIZE			64
#define	RET_BUFFER_SIZE			200

static void cmd_dispatch(char *buf, serial_port_t fs);
static void write_packet(serial_port_t fd, char *retbuf);

static int user_interface_enabled = true;

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
	case FS_POSITION:	return "POSITION";
	case FS_GETDATIM:	return "GETDATIM";
	case FS_CHARSET:	return "CHARSET";
	case FS_COPY:    	return "COPY";
	case FS_DUPLICATE: 	return "DUPLICATE";
	case FS_INITIALIZE: 	return "INITIALIZE";
	default:		return "???";
	}
}

#endif


//------------------------------------------------------------------------------------
//


void cmd_init() {
	handler_init();
	provider_init();
	channel_init();
	xcmd_init();

	// init P00/S00/R00/... file handler
	x00_handler_init();

	// default
	provider_set_ext_charset("PETSCII");
}

static int cmd_assign(const char *assign_str, int from_cmdline) {

	log_debug("Assigning from server: '%s'\n", assign_str);

			if (!isdigit(assign_str[0])) {
				log_error("Could not identify %c as drive number!\n", assign_str[0]);
				return CBM_ERROR_FAULT;
			}

			if (assign_str[1] != ':') {
				log_error("Could not identify %s as ASSIGN parameter\n", assign_str);
				return CBM_ERROR_FAULT;
			}
	
			// int rv = provider_assign(argv[i][2] & 0x0f, &(argv[i][4]));
			int rv=0;
			int drive = assign_str[0] & 0x0f;
			char provider_name[MAX_LEN_OF_PROVIDER_NAME + 1];
			char *provider_parameter;

			// provider name followed by parameter?
			char *p = strchr(assign_str, '=');
			if (p) {
				if ((p - assign_str - 2) > MAX_LEN_OF_PROVIDER_NAME) {
					log_error("Provider name '%.8s'.. exceeds %d characters\n", assign_str + 2,
						  MAX_LEN_OF_PROVIDER_NAME);
				} else {
					// fix provider parameter character set
					const char *orig_charset = mem_alloc_str(provider_get_ext_charset());
					provider_set_ext_charset(CHARSET_ASCII_NAME);

					strncpy (provider_name, assign_str + 2, p - assign_str +1);
					provider_name[p - assign_str - 2] = 0;
					provider_parameter = p + 1;
					log_debug("cmdline_assign '%s' = '%s'\n", provider_name, 
							provider_parameter);
					rv = provider_assign(drive, provider_name, provider_parameter, 
												from_cmdline);

					// reset character set
					provider_set_ext_charset(orig_charset);
					mem_free(orig_charset);
				}
			} else {
				log_debug("No parameter for cmdline_assign\n");
				rv = provider_assign(drive, assign_str + 2, NULL, 0);
			} 
			if (rv < 0) {
				log_error("Could not assign, error number is %d\n", rv);
			}
	return rv;
}

/**
 * take the command line, search for "-A<driv>=<name>" parameters and assign
 * the value
 */
void cmd_assign_from_cmdline(int argc, char *argv[]) {

	int err = CBM_ERROR_OK;

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

			err = cmd_assign(argv[i]+2, 1);

			if (err != CBM_ERROR_OK) {
				log_error("%d Error assigning %s\n", err, argv[i]+2);
			}
			continue;
		}
	}
}

static void cmd_sync(serial_port_t readfd, serial_port_t writefd) {
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
		log_debug("sync write %02x\n", syncbuf[0]);
#endif
		e = os_write(writefd, syncbuf, 1);
	} while (e == 0 || (e < 0 &&  errno == EAGAIN));
	if (e < 0) {
		log_errno("Error on sync write", errno);
	}
	// wait for a sync byte
	int n = 0;
	do {
		n = os_read(readfd, syncbuf, 1);
#ifdef DEBUG_READ
	        log_debug("sync read %d bytes: ",n);
		log_hexdump(syncbuf, n, 0);
#endif
		if (n < 0) {
			log_errno("Error on sync read", errno);
		}
	} while (n != 1 || (0xff & syncbuf[0]) != FS_SYNC);
}


static void cmd_sendxcmd(serial_port_t writefd, char buf[]) {
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

static void cmd_sendreset(serial_port_t writefd, char buf[]) {
	buf[FSP_CMD] = FS_RESET;
	buf[FSP_LEN] = FSP_DATA;
	buf[FSP_FD] = FSFD_SETOPT;
	write_packet(writefd, buf);
}

void disable_user_interface(void) {
	user_interface_enabled = false;
	log_warn("User interface disabled. Abort with \"service fsser stop\".\n");
}

#define INBUF_SIZE 1024
// reads stdin and returns true, if the main loop should abort
static int cmd_process_stdin(void) {

	int err;
	char buf[INBUF_SIZE + 1];

	log_debug("cmd_process_stdin()\n");

	fgets(buf, INBUF_SIZE, stdin);
	drop_crlf(buf);

	log_debug("stdin: %s\n", buf);

	// Q / QUIT
	if((!strcasecmp(buf, "Q")) || (!strcasecmp(buf, "QUIT"))) {
		log_info("Aborted by user request.\n");
		return true;
	}

	// Enable *=+ / disable *=- advanced wildcards
	if ((buf[0] == '*') && (buf[1] == '=')) {
		if (buf[2] == '+') {
			advanced_wildcards = true;
			log_info("Advanced wildcards enabled.\n");
			return false;
		}
		if (buf[2] == '-') {
			advanced_wildcards = false;
			log_info("Advanced wildcards disabled.\n");
			return false;
		}
	}

	if (!strcmp(buf, "D")) {
		// dump memory structures for analysis

		// dump open endpoints, files etc
		// maybe later compare with dump from mem to find memory leaks
		provider_dump();
		return false;
	}

	if (buf[0] == 'A' || buf[0] == 'a') {
		// assign from stdin control
		err = cmd_assign(buf+1, 1);
                if (err != CBM_ERROR_OK) {
                        log_error("%d Error assigning %s\n", err, buf+1);
                }
		return false;
	}

	log_error("Syntax error: '%s'\n", buf);
	return false;
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
 *   0 if user requests to abort the Server
 */
int cmd_loop(serial_port_t readfd, serial_port_t writefd) {

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
	      if(user_interface_enabled) {
		if(os_stdin_has_data()) if(cmd_process_stdin()) return 0;
	      }

	      n = os_read(readfd, buf+wrp, 8192-wrp);
#ifdef DEBUG_READ
	      if(n) {
		log_debug("read %d bytes (wrp=%d, rdp=%d: ",n,wrp,rdp);
		log_hexdump(buf[wrp], n, 0);
              }
#endif

	      if(!n) {
		if(!device_still_present()) {
			log_error("Device lost.\n");
			return 1;
                }
	      }

              if(n < 0) {
                log_error("fsser: read error %d (%s)\nDid you power off your device?\n",
			os_errno(),strerror(os_errno()));
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


const char *get_options(const char *name, int len) {
	int l = strlen(name);
	if ((l + 1) < len) {
		return name + l + 1;
	}
	return NULL;
}

// ----------------------------------------------------------------------------------

int cmd_delete(const char *inname, int namelen, char *outbuf, int *outlen, int isrmdir) {
	int rv = CBM_ERROR_FAULT;
	int outdeleted = 0;
	file_t *file = NULL;
	file_t *dir = NULL;
	int readflag;
	const char *name;
	const char *outname;

	(void) namelen;	// silence unused warning

	endpoint_t *ep = provider_lookup(inname, namelen, &name, NAMEINFO_UNDEF_DRIVE);
	if (ep != NULL) {
		rv = handler_resolve_dir(ep, &dir, name, NULL, NULL);

		if (rv == CBM_ERROR_OK) {

			if (dir->handler->direntry != NULL) {

				while (((rv = dir->handler->direntry(dir, &file, 1, &readflag, &outname))
							== CBM_ERROR_OK)
					&& file != NULL) {

					log_info("DELETE(%s)\n", file->filename);

					if (isrmdir) {
						if (file->handler->rmdir != NULL) {
							// supports RMDIR
							rv = file->handler->rmdir(file);
						} else {
							log_warn("File %s does not support RMDIR\n", 
									file->filename);
						}
					} else {
						rv = file->handler->scratch(file);
					}

					if (rv != CBM_ERROR_OK) {
						break;
					}
					outdeleted++;
				}

				if (rv == CBM_ERROR_OK) {
					outbuf[0] = outdeleted > 99 ? 99 : outdeleted;
					*outlen = 1;
					rv = CBM_ERROR_SCRATCHED;
				}
			} else {
				rv = CBM_ERROR_FAULT;
			}
			dir->handler->close(dir, 1);
		}
		mem_free(name);
		provider_cleanup(ep);
	}
	return rv;
}

int cmd_mkdir(const char *inname, int namelen) {

	int rv = CBM_ERROR_FAULT;
	file_t *newdir = NULL;
	const char *name = NULL;

	(void) namelen;	// silence unused warning

	endpoint_t *ep = provider_lookup(inname, namelen, &name, NAMEINFO_UNDEF_DRIVE);
	if (ep != NULL) {
		log_info("MKDIR(%s)\n", name);
		rv = handler_resolve_file(ep, &newdir, name, NULL, FS_MKDIR);

		provider_cleanup(ep);
		mem_free(name);
	}
	return rv;
}

int cmd_chdir(const char *inname, int namelen) {

	int rv = CBM_ERROR_FAULT;

	log_info("CHDIR(%s)\n", inname);

	rv = provider_chdir(inname, namelen);

	return rv;
}

int cmd_move(const char *inname, int namelen) {

	int err = CBM_ERROR_FAULT;

	int todrive = inname[0];
	const char *fromname = NULL;
	const char *toname = NULL;
	endpoint_t *epto = provider_lookup(inname, namelen, &toname, NAMEINFO_UNDEF_DRIVE);
	if (epto != NULL) {
		const char *name2 = strchr(inname+1, 0);	// points to null byte after name
		name2++;					// first byte of second name
		endpoint_t *epfrom = provider_lookup(name2, namelen, &fromname, todrive);

		if (epfrom != NULL) {
			file_t *fromfile = NULL;
			
			err = handler_resolve_file(epfrom, &fromfile, fromname, NULL, FS_MOVE);
			
			if (err == CBM_ERROR_OK) {
				file_t *todir = NULL;
				const char *topattern = NULL;
	
				err = handler_resolve_dir(epto, &todir, toname, &topattern, NULL);

				if (err == CBM_ERROR_OK && todir != NULL) {

					// TODO check if that is really working (with those temp provs...)
					if (fromfile->endpoint == todir->endpoint) {
						// we can just forward it to the provider proper

						if (fromfile->handler->move != NULL) {
							err = fromfile->handler->move(fromfile, todir, topattern);
						} else {
							// e.g. x00 does not support it now
							log_warn("File type spec not supported\n");
							err = CBM_ERROR_DRIVE_NOT_READY;
						}	
					} else {
						// TODO some kind of copy/remove stuff...
						log_warn("Drive spec combination not supported\n");
						err = CBM_ERROR_DRIVE_NOT_READY;
					}
					todir->handler->close(todir, 1);
				}
				fromfile->handler->close(fromfile, 1);
			}
			provider_cleanup(epfrom);
		}
		provider_cleanup(epto);
	}
	return err;
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
static void cmd_dispatch(char *buf, serial_port_t fd) {
	int tfd, cmd;
	unsigned int len;
	char retbuf[RET_BUFFER_SIZE];
	int rv;
	char *name2;

	cmd = buf[FSP_CMD];		// 0
	len = 255 & buf[FSP_LEN];	// 1
	
	if (cmd == FS_TERM) {
#ifdef DEBUG_CMD_TERM
		{
			int n = buf[FSP_LEN];
			log_debug("term: %d bytes @%p: ",n, buf);
			log_hexdump(buf, n, 0);
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
		log_debug("cmd %s :%d bytes @%p : \n", nameofcmd(255&buf[FSP_CMD]), n, buf);
		log_hexdump(buf, n, 0);
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
	file_t *fp = NULL;

#if 0
	printf("got cmd=%d, fd=%d, name=%s",cmd,tfd,
			(cmd<FS_ASSIGN)?((char*)buf+FSP_DATA):"null");
	for (int i = 3; i < buf[FSP_LEN]; i++) printf(" %02x", (unsigned int)buf[i]);
	printf("\n");
#endif
	retbuf[FSP_CMD] = FS_REPLY;
	retbuf[FSP_LEN] = FSP_DATA + 1;		// 4 byte default packet
	retbuf[FSP_FD] = tfd;
	retbuf[FSP_DATA] = CBM_ERROR_FAULT;

	int readflag = 0;
	int retlen = 0;
	int sendreply = 1;
	const char *name = buf+FSP_DATA+1;
	int drive = buf[FSP_DATA]&255;
	int convlen = len - FSP_DATA-1;
	int record = 0;
	int outlen = 0;
	char *inname = buf + FSP_DATA;
	int namelen = len - FSP_DATA;

	// options string just in case
	const char *options = NULL;

	switch(cmd) {
		// file-oriented commands
	case FS_OPEN_RD:
	case FS_OPEN_AP:
	case FS_OPEN_WR:
	case FS_OPEN_OW:
	case FS_OPEN_RW:
		ep = provider_lookup(inname, namelen, &name, NAMEINFO_UNDEF_DRIVE);
		if (ep != NULL) {
			prov = (provider_t*) ep->ptype;
			//provider_convto(prov)(name, convlen, name, convlen);
			options = get_options(inname + 1, namelen - 1);
			log_info("OPEN %d (%d->%s:%s)\n", cmd, tfd, 
				prov->name, name);
			rv = handler_resolve_file(ep, &fp, name, options, cmd);
			if (rv == CBM_ERROR_OK && fp->recordlen > 0) {
				record = fp->recordlen;
				retbuf[FSP_DATA+1] = record & 0xff;
				retbuf[FSP_DATA+2] = (record >> 8) & 0xff;
				retbuf[FSP_LEN] = FSP_DATA + 3;	
				rv = CBM_ERROR_OPEN_REL;
			}
			retbuf[FSP_DATA] = rv;
			if (rv == CBM_ERROR_OK || rv == CBM_ERROR_OPEN_REL) {
				channel_set(tfd, fp);
				break; // out of switch() to escape provider_cleanup()
			} else {
				if (fp != NULL) {
					fp->handler->close(fp, 1);
					fp = NULL;
				}
				log_rv(rv);
			}
			// cleanup when not needed anymore
			provider_cleanup(ep);
			mem_free(name);
		}
		break;
	case FS_OPEN_DR:
		//log_debug("Open directory for drive: %d\n", 0xff & buf[FSP_DATA]);
		ep = provider_lookup(inname, namelen, &name, NAMEINFO_UNDEF_DRIVE);
		if (ep != NULL) {
			prov = (provider_t*) ep->ptype;
			options = get_options(inname, namelen - 1);
			log_info("OPEN_DR(%d->%s:%s)\n", tfd, prov->name, name);
			rv = handler_resolve_dir(ep, &fp, name, NULL, options);
			retbuf[FSP_DATA] = rv;
			if (rv == 0) {
				channel_set(tfd, fp);
				break; // out of switch() to escape provider_cleanup()
			} else {
				log_rv(rv);
			}
			// cleanup when not needed anymore
			provider_cleanup(ep);
			mem_free(name);
		}
		break;
	case FS_READ:
		fp = channel_to_file(tfd);
		if (fp != NULL) {
			    readflag = 0;	// default just in case
			    rv = fp->handler->readfile(fp, retbuf + FSP_DATA, MAX_BUFFER_SIZE-FSP_DATA, &readflag);
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
					//convfrom(retbuf+FSP_DATA+FS_DIR_NAME, fp->endpoint->ptype);
					//fp->handler->convfrom(fp, provider_get_ext_charset())
					//		(retbuf+FSP_DATA+FS_DIR_NAME, 
					//			strlen(retbuf+FSP_DATA+FS_DIR_NAME), 
					//			retbuf+FSP_DATA+FS_DIR_NAME, 
					//			strlen(retbuf+FSP_DATA+FS_DIR_NAME));
				}
			    }
		}
		break;
	case FS_WRITE:
	case FS_EOF:
		fp = channel_to_file(tfd);
		//printf("WRITE: chan=%d, ep=%p\n", tfd, ep);
		if (fp != NULL) {
			if (cmd == FS_EOF) {
				log_info("CLOSE_BY_EOF(%d)\n", tfd);
			}
			rv = fp->handler->writefile(fp, buf+FSP_DATA, len-FSP_DATA, cmd == FS_EOF);
			if (rv != 0) {
				log_rv(rv);
			}
			retbuf[FSP_DATA] = rv;
		}
		break;
	case FS_POSITION:
		// position the read/write cursor into a file
		fp = channel_to_file(tfd);
		if (fp != NULL) {
			record = (buf[FSP_DATA] & 0xff) | ((buf[FSP_DATA+1] & 0xff) << 8);
			log_debug("POSITION: chan=%d, ep=%p, record=%d\n", tfd, (void*)ep, record);
			rv = fp->handler->seek(fp, record * fp->recordlen, SEEKFLAG_ABS);
			if (rv != 0) {
				log_rv(rv);
			}
			retbuf[FSP_DATA] = rv;
		}
		break;
	case FS_CLOSE:
		fp = channel_to_file(tfd);
		if (fp != NULL) {
			log_info("CLOSE(%d)\n", tfd);
			fp->handler->close(fp, 1);
			channel_free(tfd);
			retbuf[FSP_DATA] = CBM_ERROR_OK;
		}
		break;

		// command operations
	case FS_DELETE:
		rv = cmd_delete(buf+FSP_DATA, len-FSP_DATA, retbuf+FSP_DATA+1, &outlen, 0);
		retbuf[FSP_DATA] = rv;
		retbuf[FSP_LEN] = FSP_DATA + 1 + outlen;
		break;
	case FS_MOVE:
		rv = cmd_move(buf+FSP_DATA, len-FSP_DATA);
		retbuf[FSP_DATA] = rv;
		retbuf[FSP_LEN] = FSP_DATA + 1;
		break;
	case FS_CHDIR:
		rv = cmd_chdir(buf+FSP_DATA, len-FSP_DATA);
		retbuf[FSP_DATA] = rv;
		retbuf[FSP_LEN] = FSP_DATA + 1;
		break;
	case FS_MKDIR:
		rv = cmd_mkdir(buf+FSP_DATA, len-FSP_DATA);
		retbuf[FSP_DATA] = rv;
		retbuf[FSP_LEN] = FSP_DATA + 1;
		break;
	case FS_RMDIR:
		rv = cmd_delete(buf+FSP_DATA, len-FSP_DATA, retbuf+FSP_DATA+1, &outlen, 1);
		retbuf[FSP_DATA] = rv;
		retbuf[FSP_LEN] = FSP_DATA + 1 + outlen;
		break;
	case FS_BLOCK:
		// not file-related, so no file descriptor (tfd)
		// we only support mapped drives (thus name is NULL)
		ep = provider_lookup(inname, namelen, NULL, NAMEINFO_UNDEF_DRIVE);
		if (ep != NULL) {
			prov = (provider_t*) ep->ptype;
			if (prov->block != NULL) {
				provider_convto(prov)(name, convlen, name, convlen);
				log_info("DIRECT(%d,...)\n", tfd);
				rv = prov->block(ep, buf + FSP_DATA + 1, retbuf + FSP_DATA + 1, &retlen);
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

		log_info("ASSIGN(%d -> %s = %s)\n", drive, name, name2);
		rv = provider_assign(drive, name, name2, 0);
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
			retbuf[FSP_DATA] = CBM_ERROR_OK;
		}
		break;
	case FS_FORMAT:
		log_warn("FORMAT: %s <--- NOT IMPLEMTED\n", buf+FSP_DATA);
      		break;
	case FS_COPY:
		log_warn("COPY: %s <--- NOT IMPLEMTED\n", buf+FSP_DATA);
      		break;
	case FS_DUPLICATE:
		log_warn("DUPLICATE: %s <--- NOT IMPLEMTED\n", buf+FSP_DATA);
      		break;
	case FS_INITIALIZE:
		log_info("INITIALIZE: %s\n", buf+FSP_DATA);
		retbuf[FSP_DATA] = CBM_ERROR_OK;
      		break;
	default:
		log_error("Received unknown command: %d in a %d byte packet\n", cmd, len);
	}

	if (sendreply) {
		write_packet(fd, retbuf);
	}
}

static void write_packet(serial_port_t fd, char *retbuf) {

	int e = os_write(fd, retbuf, 0xff & retbuf[FSP_LEN]);
	if (e < 0) {
		log_error("Error on write: %d\n", errno);
	}
#if defined(DEBUG_WRITE) || defined(DEBUG_CMD)
	log_debug("write %02x %02x %02x (%s):\n", 255&retbuf[0], 255&retbuf[1],
			255&retbuf[2], nameofcmd(255&retbuf[FSP_CMD]) );
	log_hexdump(retbuf + 3, retbuf[FSP_LEN] - 3, 0);
#endif
}


