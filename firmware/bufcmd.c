/* 
    XD-2031 - Serial line file server for CBMs
    Copyright (C) 2012  Andre Fachat

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
    MA  02110-1301, USA.

*/

#include <errno.h>
#include <stdlib.h>

#include "bus.h"
#include "errormsg.h"
#include "channel.h"
#include "provider.h"
#include "packet.h"

#include "debug.h"


#define	DEBUG_USER
#define	DEBUG_BLOCK


// place for command, channel, drive, track sector 
#define	CMD_BUFFER_LENGTH	FS_BLOCK_PAR_LEN

//#define CONFIG_NUM_DIRECT_BUFFERS       1


static char buf[CMD_BUFFER_LENGTH];
static packet_t cmdpack;
static packet_t datapack;
static uint8_t cbstat;
static int8_t cberr;

// ----------------------------------------------------------------------------------
// buffer handling (#-file, U1/U2/B-W/B-R/B-P
// we only have a single buffer


typedef struct {
	// what channel has the buffer, -1 unused
	int8_t		channel_no;		
	// read and write pointers in the buffer
	// wptr is one "behind" the rptr (which is incremented below), to
	// accomodate for the preload byte
	uint8_t		rptr;			// read pointer
	uint8_t		wptr;			// read pointer
	// preload flag
	uint8_t		pflag;			// 1 when first byte had been loaded
	// pointer to the last valid bytes (can be 0-255, where 0 is only the pointer is valid)
	uint8_t		lastvalid;
	// the actual 256 byte buffer
	uint8_t		buffer[256];
} cmdbuf_t;

static cmdbuf_t buffers[CONFIG_NUM_DIRECT_BUFFERS];

static int8_t cmdbuf_init(void) {
	// init the buffers
	uint8_t i;
	for (i = 0; i < CONFIG_NUM_DIRECT_BUFFERS; i++) {
		buffers[i].channel_no = -1;
	}
	return -1;
}

// reserve a free buffer for the given channel
static cmdbuf_t *cmdbuf_reserve(int8_t channel_no) {
	// reserve a direct buffer
	uint8_t i;
	for (i = 0; i < CONFIG_NUM_DIRECT_BUFFERS; i++) {
		if (buffers[i].channel_no < 0) {
			buffers[i].channel_no = channel_no;
			return buffers+i;
		}
	}
	return NULL;
}

// reserve a given buffer for the channel
static cmdbuf_t *cmdbuf_reserve_buf(int8_t channel_no, uint8_t bufno) {
	// reserve a direct buffer
	if (buffers[bufno].channel_no < 0) {
		buffers[bufno].channel_no = channel_no;
		return buffers+bufno;
	}
	return NULL;
}

static cmdbuf_t *cmdbuf_find(int8_t channel_no) {
	// find a direct buffer for a channel
	uint8_t i;
	for (i = 0; i < CONFIG_NUM_DIRECT_BUFFERS; i++) {
		if (buffers[i].channel_no == channel_no) {
			return buffers+i;
		}
	}
	return NULL;
}

static uint8_t cmdbuf_free(int8_t channel_no) {
	// find a direct buffer for a channel
	uint8_t n = 0;
	for (uint8_t i = 0; i < CONFIG_NUM_DIRECT_BUFFERS; i++) {
		if (buffers[i].channel_no == channel_no) {
			buffers[i].channel_no = -1;
			n++;
			// no break or return, to clean up others just in case
		}
	}
	return n;	// number of freed buffers
}

// ----------------------------------------------------------------------------------
/**
 * command callback
 */
static uint8_t callback(int8_t channelno, int8_t errnum, packet_t *rxpacket) {
	cberr = packet_get_buffer(rxpacket)[0];
	cbstat = 1;
	return 0;
}

static uint8_t cmd_wait_cb() {
	while (cbstat == 0) {
		delayms(1);
		main_delay();
	}

	return cberr;	// cberr is set to an FS_REPLY error in the actual callback
}

// ----------------------------------------------------------------------------------
// buffer handling (#-file, U1/U2/B-W/B-R/B-P

void bufcmd_init() {
	cmdbuf_init();
}

static void bufcmd_close(uint8_t channel_no, endpoint_t *endpoint) {
	// close file
	packet_init(&cmdpack, CMD_BUFFER_LENGTH, (uint8_t*) buf);
	packet_set_filled(&cmdpack, channel_no, FS_CLOSE, 0);
	cbstat = 0;
        endpoint->provider->submit_call(NULL, channel_no, 
		&cmdpack, &cmdpack, callback);
	cmd_wait_cb();
}


uint8_t bufcmd_write_buffer(uint8_t channel_no, endpoint_t *endpoint, uint16_t send_nbytes) {

	uint16_t restlength = send_nbytes;

	uint8_t ptype = FS_WRITE;
	uint8_t plen;
	uint8_t rv = ERROR_OK;

	debug_printf("bufcmd_write_buffer: my chan=%d\n", channel_no);

	cmdbuf_t *buffer = cmdbuf_find(channel_no);
	if (buffer == NULL) {
		return ERROR_NO_CHANNEL;
	}

	while (ptype != FS_EOF && restlength != 0 && rv == ERROR_OK) {

                packet_init(&datapack, DATA_BUFLEN, buffer->buffer + send_nbytes - restlength);
                packet_init(&cmdpack, CMD_BUFFER_LENGTH, (uint8_t*) buf);

		plen = (restlength > DATA_BUFLEN) ? DATA_BUFLEN : restlength;
		if (plen >= restlength) {
			ptype = FS_EOF;
		} else {
			ptype = FS_WRITE;
		}
                packet_set_filled(&datapack, channel_no, ptype, plen);

		cbstat = 0;
                endpoint->provider->submit_call(NULL, channel_no, 
			&datapack, &cmdpack, callback);
		rv = cmd_wait_cb();

		if (rv == ERROR_OK) {
			restlength -= plen;
		}
		debug_printf("write buffer: sent %d bytes, %d bytes left, rv=%d, ptype=%d, rxd cmd=%d\n", 
				plen, restlength, rv, ptype, packet_get_type(&cmdpack));
	}

	if (restlength > 0) {
		// received error on write
		term_printf("ONLY SHORT BUFFER, WAS ACCEPTED, SENDING %d, ACCEPTED %d\n", 256, 256-restlength);
	}

	return rv;
}

uint8_t bufcmd_read_buffer(uint8_t channel_no, endpoint_t *endpoint, uint16_t receive_nbytes) {

	uint16_t restlength = receive_nbytes;

	uint8_t ptype = FS_REPLY;

	uint8_t rv = ERROR_OK;

	debug_printf("bufcmd_read_buffer: my chan=%d\n", channel_no);

	cmdbuf_t *buffer = cmdbuf_find(channel_no);
	if (buffer == NULL) {
		return ERROR_NO_CHANNEL;
	}

	while (ptype != FS_EOF && restlength != 0) {

                packet_init(&datapack, 128, buffer->buffer + receive_nbytes - restlength);
                packet_init(&cmdpack, CMD_BUFFER_LENGTH, (uint8_t*) buf);
                packet_set_filled(&cmdpack, channel_no, FS_READ, 0);
	
		cbstat = 0;
                endpoint->provider->submit_call(NULL, channel_no, 
			&cmdpack, &datapack, callback);
		cmd_wait_cb();

		ptype = packet_get_type(&datapack);

		if (ptype == FS_REPLY) {
			// error (should not happen though)
			rv = packet_get_buffer(&datapack)[0];
			break; // out of loop
		} else
		if (ptype == FS_WRITE || ptype == FS_EOF) {

			restlength -= packet_get_contentlen(&datapack);
		}
	}

	if (restlength > 0) {
		// received short package
		term_printf("RECEIVED SHORT BUFFER, EXPECTED %d, GOT %d\n", 256, 256-restlength);
	}

	buffer->rptr = 0;
	buffer->wptr =(receive_nbytes - restlength) & 0xff;

	return rv;
}

uint8_t bufcmd_set_ptr(bus_t *bus, char *cmdbuf, errormsg_t *error) {
	
	int ichan;
	int ptr;

	uint8_t rv;

	rv = sscanf(cmdbuf, "%d%*[, ]%d", &ichan, &ptr);
	if (rv != 2) {
		return ERROR_SYNTAX_INVAL;
	}

	rv = ERROR_OK;

	uint8_t channel = bus_secaddr_adjust(bus, ichan);

	cmdbuf_t *buffer = cmdbuf_find(channel);
	if (buffer == NULL) {
		return ERROR_NO_CHANNEL;
	}

	if (ptr < 0 || ptr > 255) {
		return ERROR_OVERFLOW_IN_RECORD;
	}

	channel_flush(channel);

	// DOS 2.7 $d5cb (blkptr)
	buffer->rptr = ptr;
	buffer->wptr = ptr;

	return rv;
}

/**
 * user commands
 *
 * U1, U2
 *
 * cmdbuf pointer includes the full command, i.e. with the "U" in front.
 * 
 * blockflag is set when B-R/B-W should be done instead of U1/U2
 *
 * returns ERROR_OK/ERROR_* on direct ok/error, or -1 if a callback has been submitted
 */
uint8_t cmd_user_u12(bus_t *bus, uint8_t cmd, char *pars, errormsg_t *error, uint8_t blockflag) {

#ifdef DEBUG_USER
	debug_printf("cmd_user (%02x): %s\n", cmd, pars);
#endif

        int ichan;
        int drive;
        int track;
        int sector;

	uint8_t rv = ERROR_SYNTAX_UNKNOWN;
        uint8_t channel;
	cmdbuf_t *buffer;

    	rv = sscanf(pars, "%*[: ]%d%*[, ]%d%*[, ]%d%*[, ]%d", &ichan, &drive, &track, &sector);
       	if (rv != 4) {
               	return ERROR_SYNTAX_INVAL;
       	}
		
	channel = bus_secaddr_adjust(bus, ichan);
	buffer = cmdbuf_find(channel);
	if (buffer == NULL) {
		return ERROR_NO_CHANNEL;
	}
	buffer->pflag = 1;

	// read/write sector
#ifdef DEBUG_USER
	debug_printf("U1/2: sector ch=%d, dr=%d, tr=%d, se=%d\n", 
				channel, drive, track, sector);
#endif
			
	channel_flush(channel);

	buf[FS_BLOCK_PAR_DRIVE] = drive;		// first for provider dispatch on server
	buf[FS_BLOCK_PAR_CMD] = cmd;
	buf[FS_BLOCK_PAR_TRACK] = (track & 0xff);
	buf[FS_BLOCK_PAR_TRACK+1] = ((track >> 8) & 0xff);
	buf[FS_BLOCK_PAR_SECTOR] = (sector & 0xff);
	buf[FS_BLOCK_PAR_SECTOR+1] = ((sector >> 8) & 0xff);
	buf[FS_BLOCK_PAR_CHANNEL] = channel;		// extra compared to B-A/B-F

	packet_init(&cmdpack, CMD_BUFFER_LENGTH, (uint8_t*) buf);
	// FS_DIRECT packet with 5 data bytes, sent on cmd channel
	packet_set_filled(&cmdpack, bus_secaddr_adjust(bus, CMD_SECADDR), FS_DIRECT, FS_BLOCK_PAR_LEN);
		endpoint_t *endpoint = provider_lookup(drive, NULL);
		
	if (endpoint != NULL) {	
		cbstat = 0;
		endpoint->provider->submit_call(NULL, bus_secaddr_adjust(bus, CMD_SECADDR), 
						&cmdpack, &cmdpack, callback);
		rv = cmd_wait_cb();
debug_printf("Sent command - got: %d, rptr=%d, wptr=%d\n", rv, buffer->rptr, buffer->wptr);

		if (rv == ERROR_OK) {
			if (cmd == FS_BLOCK_U1) {
				// U1
				rv = bufcmd_read_buffer(channel, endpoint, 256);
				if (rv == ERROR_OK) {
					if (blockflag) {
						// B-R is pretty stupid here
						// DOS 2.7 ~$d562 (blkrd)
						buffer->lastvalid = buffer->buffer[0];
					} else {
						// DOS 2.7 $d56b (ublkrd)
						buffer->lastvalid = 255;
					}
				}
				if (blockflag) {
					buffer->rptr = 1;
					buffer->wptr = 1;
				} else {
					buffer->rptr = 0;
					buffer->wptr = 0;
				}
			} else {
				if (blockflag) {
					// B-W
					// DOS 2.7 $d57f (blkwt)
					if (buffer->wptr < 2) {
						buffer->buffer[0] = 1;
					} else {
						buffer->buffer[0] = buffer->wptr - 1;
					}
				}
				// U2
				rv = bufcmd_write_buffer(channel, endpoint, 256);
			}
			bufcmd_close(channel, endpoint);
			if (rv == ERROR_OK) {
				// no error so far, catch CLOSE error if any
				rv = packet_get_buffer(&cmdpack)[0];
			}
		}
		if (rv != ERROR_OK) {
		
			set_error_ts(error, rv, track > 255 ? 255 : track, sector > 255 ? 255 : sector);
			// means: don't wait, error is already set
			return -1;
		}

	} else {
		return ERROR_DRIVE_NOT_READY;
	}
		
	return rv;
}

/**
 * user commands
 *
 * U1, U2
 *
 * cmdbuf pointer includes the full command, i.e. with the "U" in front.
 * 
 * returns ERROR_OK/ERROR_* on direct ok/error, or -1 if a callback has been submitted
 */
uint8_t cmd_user(bus_t *bus, char *cmdbuf, errormsg_t *error) {

	char *pars;	
	uint8_t cmd = cmdbuf[1];

#ifdef DEBUG_USER
	debug_printf("CMD USER (%02x): %s\n", cmd, cmdbuf);
#endif

	// start of parameter string into pars
	pars = cmdbuf+2;
	
	uint8_t rv = ERROR_SYNTAX_UNKNOWN;

	switch(cmd & 0x0f) {
	case 1:		// U1
		rv = cmd_user_u12(bus, FS_BLOCK_U1, pars, error, 0);
		break;
	case 2:		// U2
		rv = cmd_user_u12(bus, FS_BLOCK_U2, pars, error, 0);
		break;
	default:
		break;
	}

	return rv;
}


/**
 * block commands
 */
uint8_t cmd_block_allocfree(bus_t *bus, char *cmdbuf, uint8_t fscmd, errormsg_t *error) {
	
	int drive;
	int track;
	int sector;

	uint8_t channel = bus_secaddr_adjust(bus, 15);
	
	uint8_t rv = ERROR_DRIVE_NOT_READY;

	rv = sscanf(cmdbuf, "%d%*[, ]%d%*[, ]%d", &drive, &track, &sector);
	if (rv != 3) {
		return ERROR_SYNTAX_INVAL;
	}

        buf[FS_BLOCK_PAR_DRIVE] = drive;		// comes first similar to other FS_* cmds
        buf[FS_BLOCK_PAR_CMD] = fscmd;
        buf[FS_BLOCK_PAR_TRACK] = track & 0xff;
        buf[FS_BLOCK_PAR_TRACK+1] = (track >> 8) & 0xff;
        buf[FS_BLOCK_PAR_SECTOR] = sector & 0xff;
        buf[FS_BLOCK_PAR_SECTOR+1] = (sector >> 8) & 0xff;

	packet_init(&cmdpack, CMD_BUFFER_LENGTH, (uint8_t*) buf);
	packet_set_filled(&cmdpack, channel, FS_DIRECT, FS_BLOCK_PAR_SECTOR+2);

        endpoint_t *endpoint = provider_lookup(drive, NULL);

        if (endpoint != NULL) {
        	cbstat = 0;
                endpoint->provider->submit_call(NULL, channel,
                                            &cmdpack, &cmdpack, callback);

		rv = cmd_wait_cb();

		// buf[1]/buf[2] contain the T&S - need to get that into the error
		track = (buf[1] & 0xff) | ((buf[2] << 8) & 0xff00);
		sector = (buf[3] & 0xff) | ((buf[4] << 8) & 0xff00);
		debug_printf("block_allocfree: drive=%d, t&s=%d, %d\n", drive, track, sector);

		if (rv != ERROR_OK) {
			set_error_ts(error, rv, track > 255 ? 255 : track, sector > 255 ? 255 : sector);

			// means: don't wait, error is already set
			return -1;
		}
	}
        return rv;
}

/*
 *
 * B-R, B-W, B-P, B-A, B-F
 *
 * cmdbuf pointer includes the full command, i.e. with the "U" in front.
 * 
 * returns ERROR_OK/ERROR_* on direct ok/error, or -1 if a callback has been submitted
 */
uint8_t cmd_block(bus_t *bus, char *cmdbuf, errormsg_t *error) {

#ifdef DEBUG_BLOCK
	debug_printf("CMD BLOCK: %s\n", cmdbuf);
#endif
	// identify command - just look for the '-' and take the following char
	while (*cmdbuf != 0 && *cmdbuf != '-') {
		cmdbuf++;
	}
	if (*cmdbuf == 0) {
		return ERROR_SYNTAX_UNKNOWN;
	}
	cmdbuf++;

	char cchar = *cmdbuf;
	
	while (*cmdbuf != 0 && *cmdbuf != ':') {
		cmdbuf++;
	}
	if (*cmdbuf == 0) {
		return ERROR_SYNTAX_UNKNOWN;
	}
	// char after the ':'
	cmdbuf++;	
		
	switch(cchar) {
	case 'A':
		return cmd_block_allocfree(bus, cmdbuf, FS_BLOCK_BA, error);
	case 'F':
		return cmd_block_allocfree(bus, cmdbuf, FS_BLOCK_BF, error);
	case 'P':
		return bufcmd_set_ptr(bus, cmdbuf, error);
	case 'R':
		return cmd_user_u12(bus, FS_BLOCK_U1, cmdbuf, error, 1);
	case 'W':
		return cmd_user_u12(bus, FS_BLOCK_U2, cmdbuf, error, 1);
	}

	return ERROR_SYNTAX_UNKNOWN;
}

// ----------------------------------------------------------------------------------
// provider for direct files

static void submit_call(void *pdata, int8_t channelno, packet_t *txbuf, packet_t *rxbuf,
                uint8_t (*callback)(int8_t channelno, int8_t errnum, packet_t *rxpacket)) {

#ifdef DEBUG_BLOCK
	debug_printf("submit call for direct file, chan=%d, cmd=%d, len=%d, name=%s\n", 
		channelno, txbuf->type, txbuf->wp, ((txbuf->type == FS_OPEN_DIRECT) ? ((char*) txbuf->buffer+1) : ""));
#endif

	int bufno;

	uint8_t rtype = FS_REPLY;
	uint8_t plen, p;
	uint8_t *ptr = packet_get_buffer(txbuf);
	ptr+=2;
	cmdbuf_t *buffer = NULL;

	switch(txbuf->type) {
	case FS_OPEN_DIRECT:
		if (*ptr) {
			// name after '#' is not empty - parse buffer number
			plen = sscanf((char*)ptr, "%d", &bufno);
			if (plen == 1) {
				buffer = cmdbuf_reserve_buf(channelno, bufno);
			}
		} else {
			// reserve buffer
			buffer = cmdbuf_reserve(channelno);
		}
		if (buffer == NULL) {
			packet_get_buffer(rxbuf)[0] = ERROR_NO_CHANNEL;
		} else {
			packet_get_buffer(rxbuf)[0] = ERROR_OK;
			// directly after OPEN, the pointer is at position 1
			buffer->rptr = 1;
			buffer->wptr = 1;
			buffer->pflag = 0;
		}
		packet_set_filled(rxbuf, channelno, FS_REPLY, 1);
		break;
	case FS_READ:
		buffer = cmdbuf_find(channelno);
		rtype = FS_EOF;		// just in case, for an error
		if (buffer != NULL) {
			if (buffer->pflag == 0) {
				// buffer not yet loaded - set channel number
				packet_get_buffer(rxbuf)[0] = buffer-buffers;
			} else {
				//debug_printf("rptr=%d, 1st data=%d (%02x)\n", buffer_rptr, 
				//		direct_buffer[buffer_rptr], direct_buffer[buffer_rptr]);
				packet_get_buffer(rxbuf)[0] = buffer->buffer[buffer->rptr];
				// wptr is one "behind" the rptr (which is incremented below), to
				// accomodate for the preload byte. 
				// as long as we track it here, this needs to be single-byte packets
				buffer->wptr = buffer->rptr;
				// see DOS 2.7 @ $d885 (getbyt)
				if (buffer->lastvalid == 0 || buffer->rptr != buffer->lastvalid) {
					rtype = FS_WRITE;
					buffer->rptr ++;
				} else {
					buffer->rptr = 0;
					// not sure if this is entirely true.
					// if IEEE preloads the EOF, but never fetches it?
					buffer->wptr = 1;
				}
debug_printf("read -> %s (ptr=%d, lastvalid=%d)\n", rtype == FS_EOF ? "EOF" : "WRITE", buffer->rptr, buffer->lastvalid);
			}
		}
		packet_set_filled(rxbuf, channelno, rtype, 1);
		break;
	case FS_WRITE:
	case FS_EOF:
		buffer = cmdbuf_find(channelno);
		if (buffer != NULL) {
			plen = packet_get_contentlen(txbuf);
			ptr = packet_get_buffer(txbuf);

			debug_printf("wptr=%d, len=%d, 1st data=%d,%d (%02x, %02x)\n", buffer->wptr, 
					plen, ptr[0], ptr[1], ptr[0], ptr[1]);

			for (p = 0; p < plen; p++) {
				buffer->buffer[buffer->wptr] = *ptr;
				ptr++;
				// rolls over on 256
				buffer->wptr++;
			}
			// disable preload
			buffer->pflag = 1;
			// align pointers
			buffer->rptr = buffer->wptr;
			buffer->lastvalid = (buffer->wptr == 0) ? 0 : buffer->wptr - 1;
			packet_get_buffer(rxbuf)[0] = ERROR_OK;
		} else {
			packet_get_buffer(rxbuf)[0] = ERROR_NO_CHANNEL;
		}
		packet_set_filled(rxbuf, channelno, FS_REPLY, 1);
		break;
	case FS_CLOSE:
		plen = cmdbuf_free(channelno);
		if (plen == 0) {
			packet_get_buffer(rxbuf)[0] = ERROR_NO_CHANNEL;
		} else {
			packet_get_buffer(rxbuf)[0] = ERROR_OK;
		}
		packet_set_filled(rxbuf, channelno, FS_REPLY, 1);
		break;
	}
	callback(channelno, 0, rxbuf);	
}


static provider_t provider = {
	NULL,			// prov_assign
	NULL,			// prov_free
	NULL,			// submit
	submit_call,		// submit_call
	NULL,			// directory_converter
	NULL			// to_provider
};

static endpoint_t endpoint = {
	&provider,
	NULL
};

endpoint_t *bufcmd_provider(void) {
	return &endpoint;
}


