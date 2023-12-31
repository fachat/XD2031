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

#include "charconvert.h"
#include "types.h"
#include "mem.h"
#include "name.h"
#include "in_device.h"
#include "provider.h"
#include "wireformat.h"
#include "xcmd.h"
#include "list.h"
#include "cmd.h"
#include "serial.h"
#include "cmdnames.h"

#define DEBUG_CMD
#undef DEBUG_CMD_TERM
#undef DEBUG_READ	/* low level reads from line data */
#define DEBUG_WRITE


#define	MAX_OPT_BUFFER_SIZE		16
#define	RET_BUFFER_SIZE			200


static void in_device_constructor(const type_t *t, void *o) {
	(void) t;
	in_device_t *d = (in_device_t*)o;

	d->wrp = 0;
	d->rdp = 0;

	// inits with drive 0
	drive_and_name_init(&d->lastdrv);

	d->charset = cconv_getcharset(CHARSET_ASCII_NAME);
}
	
static type_t in_device_type = {
	"in_device_t",
	sizeof(in_device_t),
	in_device_constructor
};

//------------------------------------------------------------------------------------
// helpers


static void dev_sync(serial_port_t readfd, serial_port_t writefd) {
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
	        log_debug("sync read %d bytes: \n",n);
		log_hexdump(syncbuf, n, 0);
#endif
		if (n < 0) {
			if (errno != EAGAIN && errno != EWOULDBLOCK) {
				log_errno("Error on sync read", errno);
				break;
			}
		} else
		if (n == 0) {
			// EOF
			break;
		}
	} while (n != 1 || (0xff & syncbuf[0]) != FS_SYNC);
}


static void dev_write_packet(serial_port_t fd, char *retbuf) {

	int e = os_write(fd, retbuf, 0xff & retbuf[FSP_LEN]);
	if (e < 0) {
		log_error("Error on write: %d\n", errno);
	}
#if defined(DEBUG_WRITE) //|| defined(DEBUG_CMD)
	log_debug("write %02x %02x %02x (%s):\n", 255&retbuf[0], 255&retbuf[1],
			255&retbuf[2], command_to_name(255&retbuf[FSP_CMD]) );
	log_hexdump(retbuf + 3, retbuf[FSP_LEN] - 3, 0);
#endif
}


static void cmd_sendxcmd(serial_port_t writefd, char buf[]) {
	// now send all the X-commands
	int ncmds = xcmd_num_options();
	log_debug("Got %d options to send:\n", ncmds);
	for (int i = 0; i < ncmds; i++) {
		const char *opt = xcmd_option(i);
		log_debug("Option %d: %s\n", i, opt);

		int len = strlen(opt);
		if (len > MAX_OPT_BUFFER_SIZE - FSP_DATA) {
			log_error("Option is too long to be sent: '%s'\n", opt);
		} else {
			buf[FSP_CMD] = FS_SETOPT;
			buf[FSP_LEN] = FSP_DATA + len + 1;
			buf[FSP_FD] = FSFD_SETOPT;
			strncpy(buf+FSP_DATA, opt, MAX_OPT_BUFFER_SIZE);
			buf[FSP_DATA + len] = 0;

			// TODO: error handling
			dev_write_packet(writefd, buf);
		}
	}
}

static void dev_sendreset(serial_port_t writefd) {

	char buf[FSP_DATA+1];

	buf[FSP_CMD] = FS_RESET;
	buf[FSP_LEN] = FSP_DATA;
	buf[FSP_FD] = FSFD_SETOPT;
	dev_write_packet(writefd, buf);
}

/**
 * This function executes a single packet from the device.
 * 
 * It takes the values from the received buffer, and forwards the command
 * to the appropriate provider for further processing, using C-style arguments
 * (and not buffer + offsets).
 *
 * The return packet is written into the file descriptor fd
 */
static void dev_dispatch(char *buf, in_device_t *dt) {
	int tfd, cmd;
	unsigned int len;
	char retbuf[RET_BUFFER_SIZE];
	int rv;

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
		log_info("cmd %s :%d bytes @%p : \n", command_to_name(255&buf[FSP_CMD]), n, buf);
		log_hexdump(buf, n, 0);
	}
#endif

	// not on FS_TERM
	tfd = buf[FSP_FD];
	if (tfd < 0 /*&& ((unsigned char) tfd) != FSFD_SETOPT*/) {
		log_error("Illegal file descriptor: %d\n", tfd);
		return;
	}

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
	int sendreply = 1;
	int outlen = 0;

	// dispatch to the correct cmd_* routine.
	// may someday be replaced by an array lookup when the routine calls have been unified...
	switch(cmd) {
		// file-oriented commands
	case FS_OPEN_RD:
	case FS_OPEN_AP:
	case FS_OPEN_WR:
	case FS_OPEN_OW:
	case FS_OPEN_RW:
		rv = cmd_open_file(tfd, buf+FSP_DATA, len-FSP_DATA, dt->charset, &dt->lastdrv, retbuf+FSP_DATA+1, &outlen, cmd);
		retbuf[FSP_DATA] = rv;
		retbuf[FSP_LEN] = FSP_DATA + 1 + outlen;
		break;
	case FS_OPEN_DR:
		rv = cmd_open_dir(tfd, buf+FSP_DATA, len-FSP_DATA, dt->charset, &dt->lastdrv);
		retbuf[FSP_DATA] = rv;
		retbuf[FSP_LEN] = FSP_DATA + 1;
		break;
	case FS_READ:
		// note that on the server side, we do not need to handle FS_DATA*, as we only send those
		rv = cmd_read(tfd, retbuf+FSP_DATA, &outlen, &readflag, dt->charset, &dt->lastdrv);
		if (rv != CBM_ERROR_OK) {
			retbuf[FSP_DATA] = rv;
			retbuf[FSP_LEN] = FSP_DATA + 1;
		} else {
			retbuf[FSP_CMD] = (readflag & READFLAG_EOF) ? FS_DATA_EOF : FS_DATA;
			retbuf[FSP_LEN] = FSP_DATA + outlen;
		}
		break;
	case FS_INFO:
		cmd_info(retbuf+FSP_DATA, &outlen, dt->charset);
		retbuf[FSP_CMD] = FS_DATA_EOF;
		retbuf[FSP_LEN] = FSP_DATA + outlen;
		break;
	case FS_WRITE:
	case FS_WRITE_EOF:
		rv = cmd_write(tfd, cmd, buf+FSP_DATA, len-FSP_DATA);
		retbuf[FSP_DATA] = rv;
		retbuf[FSP_LEN] = FSP_DATA + 1;
		break;
	case FS_POSITION:
		rv = cmd_position(tfd, buf+FSP_DATA, len-FSP_DATA);
		retbuf[FSP_DATA] = rv;
		retbuf[FSP_LEN] = FSP_DATA + 1;
		break;
	case FS_CLOSE:
		rv = cmd_close(tfd, retbuf+FSP_DATA+1, &outlen);
		retbuf[FSP_DATA] = rv;
		retbuf[FSP_LEN] = FSP_DATA + 1 + outlen;
		break;

		// command operations
	case FS_DELETE:
		rv = cmd_delete(buf+FSP_DATA, len-FSP_DATA, dt->charset, retbuf+FSP_DATA+1, &outlen, 0);
		retbuf[FSP_DATA] = rv;
		retbuf[FSP_LEN] = FSP_DATA + 1 + outlen;
		break;
	case FS_MOVE:
		rv = cmd_move(buf+FSP_DATA, len-FSP_DATA, dt->charset);
		retbuf[FSP_DATA] = rv;
		retbuf[FSP_LEN] = FSP_DATA + 1;
		break;
	case FS_CHDIR:
		rv = cmd_chdir(buf+FSP_DATA, len-FSP_DATA, dt->charset);
		retbuf[FSP_DATA] = rv;
		retbuf[FSP_LEN] = FSP_DATA + 1;
		break;
	case FS_MKDIR:
		rv = cmd_mkdir(buf+FSP_DATA, len-FSP_DATA, dt->charset);
		retbuf[FSP_DATA] = rv;
		retbuf[FSP_LEN] = FSP_DATA + 1;
		break;
	case FS_RMDIR:
		rv = cmd_delete(buf+FSP_DATA, len-FSP_DATA, dt->charset, retbuf+FSP_DATA+1, &outlen, 1);
		retbuf[FSP_DATA] = rv;
		retbuf[FSP_LEN] = FSP_DATA + 1 + outlen;
		break;
	case FS_COPY:
		rv = cmd_copy(buf+FSP_DATA, len-FSP_DATA, dt->charset);
		retbuf[FSP_DATA] = rv;
		retbuf[FSP_LEN] = FSP_DATA + 1 + outlen;
      		break;
	case FS_BLOCK:
		rv = cmd_block(tfd, buf+FSP_DATA, len-FSP_DATA, retbuf+FSP_DATA+1, &outlen);
		retbuf[FSP_DATA] = rv;
		retbuf[FSP_LEN] = FSP_DATA + 1 + outlen;
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
		rv = cmd_assign_packet(buf+FSP_DATA, len-FSP_DATA, dt->charset);
		//rv = provider_assign(drive, name, name2, dt->charset, 0);
		if (rv != 0) {
			log_rv(rv);
		}
		retbuf[FSP_DATA] = rv;
		break;
	case FS_RESET:
		log_info("RESET\n");
		sleep(1);
		// send the X command line options again
		cmd_sendxcmd(dt->writefd, retbuf);
		// we have already sent everything
		sendreply = 0;
		break;
	case FS_CHARSET:
		log_info("CHARSET: %s\n", buf+FSP_DATA);
		if (tfd == FSFD_CMD) {
			charset_t c = cconv_getcharset(buf + FSP_DATA);
			dt->charset = c;
			//provider_set_ext_charset(buf+FSP_DATA);
			retbuf[FSP_DATA] = CBM_ERROR_OK;
		}
		break;
	case FS_FORMAT:
		rv = cmd_format(buf+FSP_DATA, len-FSP_DATA, dt->charset);
		retbuf[FSP_DATA] = rv;
      		break;
	case FS_DUPLICATE:
		log_warn("DUPLICATE: %s <--- NOT IMPLEMTED\n", buf+FSP_DATA);
      		break;
	case FS_CHKDSK:
		log_warn("VALIDATE: %s <--- NOT IMPLEMTED\n", buf+FSP_DATA);
      		break;
	case FS_INITIALIZE:
		log_info("INITIALIZE: %s\n", buf+FSP_DATA);
		retbuf[FSP_DATA] = CBM_ERROR_OK;
      		break;
	default:
		log_error("Received unknown command: %d in a %d byte packet\n", cmd, len);
		int n = buf[FSP_LEN];
		log_info("cmd %s :%d bytes @%p : \n", command_to_name(255&buf[FSP_CMD]), n, buf);
		log_hexdump(buf, n, 0);
	}

	if (sendreply) {
		dev_write_packet(dt->writefd, retbuf);
	}
}


//------------------------------------------------------------------------------------
// init

in_device_t *in_device_init(serial_port_t readfd, serial_port_t writefd, int do_reset) {

	in_device_t *tp = mem_alloc(&in_device_type);	
	tp->readfd = readfd;
	tp->writefd = writefd;

	if (do_reset) {
		// sync device and server
		dev_sync(readfd, writefd);

		// tell the device we've reset
		// (it will answer with FS_RESET, which gives us the chance to send the X commands)
		dev_sendreset(writefd);
	}
	return tp;
}
	

//------------------------------------------------------------------------------------
// loop


/**
 *
 * Here the data is read from the given readfd, put into a packet buffer,
 * then given to cmd_dispatch() for the actual execution, and the reply is 
 * again packeted and written to the writefd
 *
 * returns
 *   2 if read fails (errno gives more information)
 *   1 if no data has been read
 *   0 if processing succeeded
 */
int in_device_loop(in_device_t *tp) {

        int n;
	int plen;
	int cmd;

	      n = os_read(tp->readfd, tp->buf+tp->wrp, 8192-tp->wrp);
#ifdef DEBUG_READ
	      if(n) {
		log_debug("read %d bytes (wrp=%d, rdp=%d: ",n,tp->wrp,tp->rdp);
		log_hexdump(tp->buf+tp->wrp, n, 0);
              }
#endif

#if 0	// this only works for the actual serial device
	      if(!n) {
		if(!device_still_present()) {
			log_error("Device lost.\n");
			return 2;
                }
		return 1;
	      }
#endif

              if(n < 0) {

		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return 1;
		}
                log_error("fsser: read error %d (%s) on fd %d\nDid you power off your device?\n",
			os_errno(),strerror(os_errno()), tp->readfd);
                return 2;
              }
              tp->wrp+=n;
              if(tp->rdp && (tp->wrp==8192 || tp->rdp==tp->wrp)) {
                if(tp->rdp!=tp->wrp) {
                  memmove(tp->buf, tp->buf+tp->rdp, tp->wrp-tp->rdp);
                }
                tp->wrp -= tp->rdp;
                tp->rdp = 0;
              }
              // as long as we have more than FSP_LEN bytes in the buffer
              // i.e. 2 or more, we loop and process packets
              // FSP_LEN is the position of the packet length
              while(tp->wrp-tp->rdp > FSP_LEN) {
                // first byte in packet is command, second is length of packet
                plen = 255 & tp->buf[tp->rdp+FSP_LEN];	//  AND with 255 to fix sign
                cmd = 255 & tp->buf[tp->rdp+FSP_CMD];	//  AND with 255 to fix sign
                // full packet received already?
                if (cmd == FS_SYNC || plen < FSP_DATA) {
		  // a packet is at least 3 bytes (when with zero data length)
		  // or the byte is the FS_SYNC command
		  // so ignore byte and shift one in buffer position
		  tp->rdp++;
		} else 
                if(tp->wrp-tp->rdp >= plen) {
		  // did we already receive the full packet?
                  // yes, then execute
		  //printf("dispatch @rdp=%d [%02x %02x ... ]\n", rdp, buf[rdp], buf[rdp+1]);
                  dev_dispatch(tp->buf+tp->rdp, tp);
                  tp->rdp +=plen;
                } else {
		  // no, then break out of the while, to read more data
		  break;
                }
              }
	return 0;

}


