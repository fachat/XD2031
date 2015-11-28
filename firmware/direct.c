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

#include <stdio.h>
#include <stdint.h>

#include "debug.h"
#include "bus.h"
#include "provider.h"
#include "errormsg.h"
#include "buffer.h"
#include "direct.h"

#define	DEBUG_USER
#define	DEBUG_BLOCK


// place for command, channel, drive, track sector 
#define	CMD_BUFFER_LENGTH	FS_BLOCK_PAR_LEN

//#define CONFIG_NUM_DIRECT_BUFFERS       1


// ----------------------------------------------------------------------------------
// buffer handling (#-file, U1/U2/B-W/B-R/B-P

void direct_init() {
}


uint8_t direct_set_ptr(bus_t *bus, char *cmdbuf) {
	
	int ichan;
	int ptr;

	uint8_t rv;

	rv = sscanf(cmdbuf, "%*[ :]%d%*[, ]%d", &ichan, &ptr);
	if (rv != 2) {
		return CBM_ERROR_SYNTAX_INVAL;
	}

	rv = CBM_ERROR_OK;

	uint8_t channel = bus_secaddr_adjust(bus, ichan);

	cmdbuf_t *buffer = buf_find(channel);
	if (buffer == NULL) {
		return CBM_ERROR_NO_CHANNEL;
	}

	if (ptr < 0 || ptr > 255) {
		return CBM_ERROR_OVERFLOW_IN_RECORD;
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
 * returns CBM_ERROR_OK/CBM_ERROR_* on direct ok/error, or -1 if a callback has been submitted
 */
static int8_t cmd_user_u12(bus_t *bus, uint8_t cmd, char *pars, uint8_t blockflag, 
			uint8_t *err_trk, uint8_t *err_sec, uint8_t *err_drv) {

#ifdef DEBUG_USER
	debug_printf("cmd_user (%02x): %s\n", cmd, pars);
#endif

        int ichan;
        int drive;
        int track;
        int sector;

	uint8_t rv = CBM_ERROR_SYNTAX_UNKNOWN;
        uint8_t channel;
	cmdbuf_t *buffer;

    	rv = sscanf(pars, "%*[: ]%d%*[, ]%d%*[, ]%d%*[, ]%d", &ichan, &drive, &track, &sector);
       	if (rv != 4) {
               	return CBM_ERROR_SYNTAX_INVAL;
       	}
	
	
	channel = bus_secaddr_adjust(bus, ichan);
	buffer = buf_find(channel);
	if (buffer == NULL) {
		return CBM_ERROR_NO_CHANNEL;
	}
	buffer->pflag |= PFLAG_PRELOAD;

	*err_drv = drive;

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

	packet_init(&buf_cmdpack, CMD_BUFFER_LENGTH, (uint8_t*) buf);
	// FS_DIRECT packet with 7 data bytes, sent on cmd channel
	packet_set_filled(&buf_cmdpack, bus_secaddr_adjust(bus, CMD_SECADDR), FS_BLOCK, FS_BLOCK_PAR_LEN);

	endpoint_t *endpoint = provider_lookup(drive, NULL);
		
	if (endpoint != NULL) {	
		rv = buf_call(endpoint, NULL, bus_secaddr_adjust(bus, CMD_SECADDR), &buf_cmdpack, &buf_cmdpack);
/*
		cbstat = 0;
		endpoint->provider->submit_call(NULL, bus_secaddr_adjust(bus, CMD_SECADDR), 
						&buf_cmdpack, &buf_cmdpack, cmd_callback);
		rv = cmd_wait_cb();
*/
debug_printf("Sent command - got: %d, rptr=%d, wptr=%d\n", rv, buffer->rptr, buffer->wptr);

		if (rv == CBM_ERROR_OK) {
			if (cmd == FS_BLOCK_U1) {
				// U1
				rv = buffer_read_buffer(channel, endpoint, 256);
				if (rv == CBM_ERROR_OK) {
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
				rv = buffer_write_buffer(channel, endpoint, 0, 256);
			}
			buffer_close(channel, endpoint);
			if (rv == CBM_ERROR_OK) {
				// no error so far, catch CLOSE error if any
				rv = packet_get_buffer(&buf_cmdpack)[0];
			}
		}
		if (rv != CBM_ERROR_OK) {

			*err_trk = track > 255 ? 255 : track;	
			*err_sec = sector > 255 ? 255 : sector;
			return -rv;
		}

	} else {
		return CBM_ERROR_DRIVE_NOT_READY;
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
 * returns CBM_ERROR_OK/CBM_ERROR_* on direct ok/error, or -1 if a callback has been submitted
 */
int8_t cmd_user(bus_t *bus, char *cmdbuf, uint8_t *err_trk, uint8_t *err_sec, uint8_t *err_drv) {

	char *pars;	
	uint8_t cmd = cmdbuf[1];

#ifdef DEBUG_USER
	debug_printf("CMD USER (%02x): %s\n", cmd, cmdbuf);
#endif

	// start of parameter string into pars
	pars = cmdbuf+2;
	
	int8_t rv = CBM_ERROR_SYNTAX_UNKNOWN;

        // digits and letters allowed: U1 = UA etc.
	switch(cmd & 0x0f) {
	case 1:		// U1
		rv = cmd_user_u12(bus, FS_BLOCK_U1, pars, 0, err_trk, err_sec, err_drv);
		break;
	case 2:		// U2
		rv = cmd_user_u12(bus, FS_BLOCK_U2, pars, 0, err_trk, err_sec, err_drv);
		break;
	case 9:		// U9 / UI
		rv = CBM_ERROR_DOSVERSION;
		break;
	default:
		break;
	}

	return rv;
}


/**
 * block commands
 */
static int8_t cmd_block_allocfree(bus_t *bus, char *cmdbuf, uint8_t fscmd, 
			uint8_t *err_trk, uint8_t *err_sec, uint8_t *err_drv) {
	
	int drive;
	int track;
	int sector;

	uint8_t channel = bus_secaddr_adjust(bus, 15);
	
	uint8_t rv = CBM_ERROR_DRIVE_NOT_READY;

	if (3 != sscanf(cmdbuf, "%*[: ]%d%*[, ]%d%*[, ]%d", &drive, &track, &sector)) {
		return CBM_ERROR_SYNTAX_INVAL;
	}

	*err_drv = drive;

        buf[FS_BLOCK_PAR_DRIVE] = drive;		// comes first similar to other FS_* cmds
        buf[FS_BLOCK_PAR_CMD] = fscmd;
        buf[FS_BLOCK_PAR_TRACK] = track & 0xff;
        buf[FS_BLOCK_PAR_TRACK+1] = (track >> 8) & 0xff;
        buf[FS_BLOCK_PAR_SECTOR] = sector & 0xff;
        buf[FS_BLOCK_PAR_SECTOR+1] = (sector >> 8) & 0xff;

	packet_init(&buf_cmdpack, CMD_BUFFER_LENGTH, (uint8_t*) buf);
	packet_set_filled(&buf_cmdpack, channel, FS_BLOCK, FS_BLOCK_PAR_SECTOR+2);

        endpoint_t *endpoint = provider_lookup(drive, NULL);

        if (endpoint != NULL) {
		rv = buf_call(endpoint, NULL, channel, &buf_cmdpack, &buf_cmdpack);

		// buf[1]/buf[2] contain the T&S - need to get that into the error
		track = (buf[1] & 0xff) | ((buf[2] << 8) & 0xff00);
		sector = (buf[3] & 0xff) | ((buf[4] << 8) & 0xff00);
		debug_printf("block_allocfree: drive=%d, t&s=%d, %d\n", drive, track, sector);

		if (rv != CBM_ERROR_OK) {
			*err_trk = track > 255 ? 255 : track;
			*err_sec = sector > 255 ? 255 : sector;
			return -rv;
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
 * returns CBM_ERROR_OK/CBM_ERROR_* on direct ok/error, or -1 if a callback has been submitted
 */
int8_t cmd_block(bus_t *bus, char *cmdbuf, uint8_t *err_trk, uint8_t *err_sec, uint8_t *err_drv) {

#ifdef DEBUG_BLOCK
	debug_printf("CMD BLOCK: %s\n", cmdbuf);
#endif
	// identify command - just look for the '-' and take the following char
	while (*cmdbuf != 0 && *cmdbuf != '-') {
		cmdbuf++;
	}
	if (*cmdbuf == 0) {
		return CBM_ERROR_SYNTAX_UNKNOWN;
	}
	cmdbuf++;

	char cchar = *cmdbuf++;
	
	// skip chars until the following blank or colon, which denote start of params
	while (*cmdbuf != 0 && *cmdbuf != ' ' && *cmdbuf != ':') {
		cmdbuf++;
	}
	if (*cmdbuf == 0) {
		return CBM_ERROR_SYNTAX_UNKNOWN;
	}
		
	switch(cchar) {
	case 'A':
		return cmd_block_allocfree(bus, cmdbuf, FS_BLOCK_BA, err_trk, err_sec, err_drv);
	case 'F':
		return cmd_block_allocfree(bus, cmdbuf, FS_BLOCK_BF, err_trk, err_sec, err_drv);
	case 'P':
		return direct_set_ptr(bus, cmdbuf);
	case 'R':
		return cmd_user_u12(bus, FS_BLOCK_U1, cmdbuf, 1, err_trk, err_sec, err_drv);
	case 'W':
		return cmd_user_u12(bus, FS_BLOCK_U2, cmdbuf, 1, err_trk, err_sec, err_drv);
	}

	return CBM_ERROR_SYNTAX_UNKNOWN;
}

// ----------------------------------------------------------------------------------
// provider for direct files

static void block_submit_call(void *pdata, int8_t channelno, packet_t *txbuf, packet_t *rxbuf,
                uint8_t (*fncallback)(int8_t channelno, int8_t errnum, packet_t *rxpacket)) {

#ifdef DEBUG_BLOCK
	debug_printf("submit call for direct file, chan=%d, cmd=%d, len=%d, name=%s\n", 
		channelno, txbuf->type, txbuf->wp, ((txbuf->type == FS_OPEN_DIRECT || txbuf->type == FS_OPEN_RW) ? ((char*) txbuf->buffer+1) : ""));
debug_flush();
#endif

	int bufno;

	int8_t err = CBM_ERROR_NO_CHANNEL;
	uint8_t rtype = FS_REPLY;
	uint8_t plen, p;
	uint8_t *ptr = NULL;
	cmdbuf_t *buffer = NULL;

	switch(txbuf->type) {
	case FS_OPEN_DIRECT:
		ptr = packet_get_buffer(txbuf) + 2;
		if (*ptr) {
			// name after '#' is not empty - parse buffer number
			plen = sscanf((char*)ptr, "%d", &bufno);
			if (plen == 1) {
				buffer = buf_reserve_buf(channelno, bufno);
			}
		} else {
			// reserve buffer
			buffer = buf_reserve(channelno);
		}
		if (buffer == NULL) {
			packet_get_buffer(rxbuf)[0] = CBM_ERROR_NO_CHANNEL;
		} else {
			packet_get_buffer(rxbuf)[0] = CBM_ERROR_OK;
			// directly after OPEN, the pointer is at position 1
			buffer->rptr = 1;
			buffer->wptr = 1;
			buffer->pflag = 0;
		}
		packet_set_filled(rxbuf, channelno, FS_REPLY, 1);
		break;
	case FS_READ:
		buffer = buf_find(channelno);
		rtype = FS_DATA_EOF;		// just in case, for an error
		if (buffer != NULL) {
			if ((buffer->pflag & PFLAG_PRELOAD) == 0) {
				// only direct block access
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
					rtype = FS_DATA;
					buffer->rptr ++;
				} else {
					buffer->rptr = 0;
					// not sure if this is entirely true.
					// if IEEE preloads the EOF, but never fetches it?
					buffer->wptr = 1;
				}
debug_printf("read -> %s (ptr=%d, lastvalid=%d)\n", rtype == FS_DATA_EOF ? "DATA_EOF" : "DATA", buffer->rptr, buffer->lastvalid);
			}
		}
		packet_set_filled(rxbuf, channelno, rtype, 1);
		break;
	case FS_WRITE:
	case FS_WRITE_EOF:
		buffer = buf_find(channelno);
		if (buffer != NULL) {
			plen = packet_get_contentlen(txbuf);
			ptr = packet_get_buffer(txbuf);

			debug_printf("wptr=%d, len=%d, 1st data=%d,%d (%02x, %02x)\n", buffer->wptr, 
					plen, ptr[0], ptr[1], ptr[0], ptr[1]);

			err = CBM_ERROR_OK;

			for (p = 0; p < plen; p++) {
				// not rel file or end of record reached
				buffer->buffer[buffer->wptr] = *ptr;
				ptr++;
				// rolls over on 256
				buffer->wptr++;
				if (buffer->recordlen > 0) {
					buffer->cur_pos_in_record++;
				}
			}
			// disable preload
			buffer->pflag |= PFLAG_PRELOAD;
			// align pointers
			buffer->rptr = buffer->wptr;
			buffer->lastvalid = (buffer->wptr == 0) ? 0 : buffer->wptr - 1;
			packet_get_buffer(rxbuf)[0] = err;
		} else {
			packet_get_buffer(rxbuf)[0] = CBM_ERROR_NO_CHANNEL;
		}
		packet_set_filled(rxbuf, channelno, FS_REPLY, 1);
		break;
	case FS_CLOSE:
		buffer = buf_find(channelno);
		if (buffer != NULL) {
			plen = buf_free(channelno);
			if (plen == 0) {
				packet_get_buffer(rxbuf)[0] = CBM_ERROR_NO_CHANNEL;
			} else {
				packet_get_buffer(rxbuf)[0] = CBM_ERROR_OK;
			}
			packet_set_filled(rxbuf, channelno, FS_REPLY, 1);
		}
		break;
	}
	fncallback(channelno, 0, rxbuf);
}

static charset_t charset(void *epdata) {
	return CHARSET_ASCII;
}

static provider_t directprovider = {
	NULL,			// prov_assign
	NULL,			// prov_free
	charset,		// get current character set
	NULL,			// set new charset
	NULL,			// submit
	block_submit_call,	// submit_call
	NULL,			// directory_converter
	NULL,			// channel_get
	NULL			// channel_put
};

static endpoint_t direct_endpoint = {
	&directprovider,
	NULL
};


endpoint_t *direct_provider(void) {
	return &direct_endpoint;
}



