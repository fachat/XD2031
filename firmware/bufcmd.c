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
#include "led.h"


#define	DEBUG_USER
#define	DEBUG_BLOCK
#undef	DEBUG_RELFILE


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
	uint8_t		pflag;			// see PFLAG defines below
	// pointer to the last valid bytes (can be 0-255, where 0 is only the pointer is valid)
	uint8_t		lastvalid;
	// relative file information
	// the proxied endpoint for rel files
	endpoint_t	*real_endpoint;		
	// record length (max 254 byte)
	uint8_t		recordlen;
	// the record number for the (first) record in the buffer
	uint16_t	buf_recordno;		
	// position of current record in buffer (multiple may be loaded in one read)
	uint8_t		pos_of_record;
	// current position in record
	uint8_t		cur_pos_in_record;
	// the actual 256 byte buffer
	uint8_t		buffer[256];

} cmdbuf_t;

#define	PFLAG_PRELOAD		1		// preload has happened
#define	PFLAG_ISREAD		2		// buffer has been read from (for rel files)

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
			buffers[i].real_endpoint = NULL;
			buffers[i].recordlen = 0;
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
static uint8_t cmd_callback(int8_t channelno, int8_t errnum, packet_t *rxpacket) {
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
		&cmdpack, &cmdpack, cmd_callback);
	cmd_wait_cb();
}


static uint8_t bufcmd_write_buffer(uint8_t channel_no, endpoint_t *endpoint, 
		uint8_t start_of_data, uint16_t send_nbytes) {

	uint16_t restlength = send_nbytes;

	uint8_t ptype = FS_WRITE;
	uint8_t plen;
	uint8_t rv = CBM_ERROR_OK;

	debug_printf("bufcmd_write_buffer: my chan=%d\n", channel_no);

	cmdbuf_t *buffer = cmdbuf_find(channel_no);
	if (buffer == NULL) {
		return CBM_ERROR_NO_CHANNEL;
	}

	while (ptype != FS_EOF && restlength != 0 && rv == CBM_ERROR_OK) {

                packet_init(&datapack, DATA_BUFLEN, buffer->buffer + start_of_data + send_nbytes - restlength);
                packet_init(&cmdpack, CMD_BUFFER_LENGTH, (uint8_t*) buf);

		plen = (restlength > DATA_BUFLEN) ? DATA_BUFLEN : restlength;
		if (plen >= restlength) {
			ptype = FS_EOF;
		} else {
			ptype = FS_WRITE;
		}
                packet_set_filled(&datapack, channel_no, ptype, plen);

for (uint8_t i = 0; i < plen; i++) {
	debug_printf(" %02x", buffer->buffer[start_of_data + send_nbytes - restlength + i]);
}
debug_puts(" < sent\n");

		cbstat = 0;
                endpoint->provider->submit_call(endpoint->provdata, channel_no, 
			&datapack, &cmdpack, cmd_callback);
		rv = cmd_wait_cb();

		if (rv == CBM_ERROR_OK) {
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

static uint8_t bufcmd_read_buffer(uint8_t channel_no, endpoint_t *endpoint, uint16_t receive_nbytes) {

	uint16_t lengthread = 0;

	uint8_t ptype = FS_REPLY;

	uint8_t rv = CBM_ERROR_OK;

	debug_printf("bufcmd_read_buffer: my chan=%d\n", channel_no);

	cmdbuf_t *buffer = cmdbuf_find(channel_no);
	if (buffer == NULL) {
		return CBM_ERROR_NO_CHANNEL;
	}

	while (ptype != FS_EOF && lengthread < receive_nbytes) {

                packet_init(&datapack, 128, buffer->buffer + lengthread);
                packet_init(&cmdpack, CMD_BUFFER_LENGTH, (uint8_t*) buf);
                packet_set_filled(&cmdpack, channel_no, FS_READ, 0);
	
		cbstat = 0;
                endpoint->provider->submit_call(endpoint->provdata, channel_no, 
			&cmdpack, &datapack, cmd_callback);
		cmd_wait_cb();

		ptype = packet_get_type(&datapack);

		if (ptype == FS_REPLY) {
			// error (should not happen though)
			rv = packet_get_buffer(&datapack)[0];
			break; // out of loop
		} else
		if (ptype == FS_WRITE || ptype == FS_EOF) {

			lengthread += packet_get_contentlen(&datapack);
		}
	}

	if (lengthread < receive_nbytes) {
		// received short package
		term_printf("RECEIVED SHORT BUFFER, EXPECTED %d, GOT %d\n", 
					receive_nbytes, lengthread);
	}

	buffer->rptr = 0;
	buffer->wptr =lengthread & 0xff;

	return rv;
}

uint8_t bufcmd_set_ptr(bus_t *bus, char *cmdbuf, errormsg_t *error) {
	
	int ichan;
	int ptr;

	uint8_t rv;

	rv = sscanf(cmdbuf, "%d%*[, ]%d", &ichan, &ptr);
	if (rv != 2) {
		return CBM_ERROR_SYNTAX_INVAL;
	}

	rv = CBM_ERROR_OK;

	uint8_t channel = bus_secaddr_adjust(bus, ichan);

	cmdbuf_t *buffer = cmdbuf_find(channel);
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
uint8_t cmd_user_u12(bus_t *bus, uint8_t cmd, char *pars, errormsg_t *error, uint8_t blockflag) {

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
	buffer = cmdbuf_find(channel);
	if (buffer == NULL) {
		return CBM_ERROR_NO_CHANNEL;
	}
	buffer->pflag |= PFLAG_PRELOAD;

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
	packet_set_filled(&cmdpack, bus_secaddr_adjust(bus, CMD_SECADDR), FS_BLOCK, FS_BLOCK_PAR_LEN);

	endpoint_t *endpoint = provider_lookup(drive, NULL);
		
	if (endpoint != NULL) {	
		cbstat = 0;
		endpoint->provider->submit_call(NULL, bus_secaddr_adjust(bus, CMD_SECADDR), 
						&cmdpack, &cmdpack, cmd_callback);
		rv = cmd_wait_cb();
debug_printf("Sent command - got: %d, rptr=%d, wptr=%d\n", rv, buffer->rptr, buffer->wptr);

		if (rv == CBM_ERROR_OK) {
			if (cmd == FS_BLOCK_U1) {
				// U1
				rv = bufcmd_read_buffer(channel, endpoint, 256);
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
				rv = bufcmd_write_buffer(channel, endpoint, 0, 256);
			}
			bufcmd_close(channel, endpoint);
			if (rv == CBM_ERROR_OK) {
				// no error so far, catch CLOSE error if any
				rv = packet_get_buffer(&cmdpack)[0];
			}
		}
		if (rv != CBM_ERROR_OK) {
		
			set_error_ts(error, rv, track > 255 ? 255 : track, sector > 255 ? 255 : sector);
			// means: don't wait, error is already set
			return -1;
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
uint8_t cmd_user(bus_t *bus, char *cmdbuf, errormsg_t *error) {

	char *pars;	
	uint8_t cmd = cmdbuf[1];

#ifdef DEBUG_USER
	debug_printf("CMD USER (%02x): %s\n", cmd, cmdbuf);
#endif

	// start of parameter string into pars
	pars = cmdbuf+2;
	
	uint8_t rv = CBM_ERROR_SYNTAX_UNKNOWN;

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
	
	uint8_t rv = CBM_ERROR_DRIVE_NOT_READY;

	rv = sscanf(cmdbuf, "%d%*[, ]%d%*[, ]%d", &drive, &track, &sector);
	if (rv != 3) {
		return CBM_ERROR_SYNTAX_INVAL;
	}

        buf[FS_BLOCK_PAR_DRIVE] = drive;		// comes first similar to other FS_* cmds
        buf[FS_BLOCK_PAR_CMD] = fscmd;
        buf[FS_BLOCK_PAR_TRACK] = track & 0xff;
        buf[FS_BLOCK_PAR_TRACK+1] = (track >> 8) & 0xff;
        buf[FS_BLOCK_PAR_SECTOR] = sector & 0xff;
        buf[FS_BLOCK_PAR_SECTOR+1] = (sector >> 8) & 0xff;

	packet_init(&cmdpack, CMD_BUFFER_LENGTH, (uint8_t*) buf);
	packet_set_filled(&cmdpack, channel, FS_BLOCK, FS_BLOCK_PAR_SECTOR+2);

        endpoint_t *endpoint = provider_lookup(drive, NULL);

        if (endpoint != NULL) {
        	cbstat = 0;
                endpoint->provider->submit_call(NULL, channel,
                                            &cmdpack, &cmdpack, cmd_callback);

		rv = cmd_wait_cb();

		// buf[1]/buf[2] contain the T&S - need to get that into the error
		track = (buf[1] & 0xff) | ((buf[2] << 8) & 0xff00);
		sector = (buf[3] & 0xff) | ((buf[4] << 8) & 0xff00);
		debug_printf("block_allocfree: drive=%d, t&s=%d, %d\n", drive, track, sector);

		if (rv != CBM_ERROR_OK) {
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
 * returns CBM_ERROR_OK/CBM_ERROR_* on direct ok/error, or -1 if a callback has been submitted
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
		return CBM_ERROR_SYNTAX_UNKNOWN;
	}
	cmdbuf++;

	char cchar = *cmdbuf;
	
	while (*cmdbuf != 0 && *cmdbuf != ':') {
		cmdbuf++;
	}
	if (*cmdbuf == 0) {
		return CBM_ERROR_SYNTAX_UNKNOWN;
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
	ptr+=2;
	cmdbuf_t *buffer = NULL;

	switch(txbuf->type) {
	case FS_OPEN_DIRECT:
		ptr = packet_get_buffer(txbuf);
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
		buffer = cmdbuf_find(channelno);
		rtype = FS_EOF;		// just in case, for an error
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
		buffer = cmdbuf_find(channelno);
		if (buffer != NULL) {
			plen = cmdbuf_free(channelno);
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
// ----------------------------------------------------------------------------------
// provider for relative files

/**
 * send FS_POSITION
 */
static int8_t bufcmd_send_position(cmdbuf_t *buffer, int8_t channel) {

	uint16_t recordno = buffer->buf_recordno;
	int8_t rv;

#ifdef DEBUG_RELFILE
	debug_printf("send_position: chan=%d, record=%d\n", channel, recordno);
	debug_flush();
#endif

	// protocol is zero-based, CBM is 1-based
	if (recordno != 0) {
		recordno--;
	}

        buf[0] = recordno & 0xff;        
        buf[1] = (recordno >> 8) & 0xff;

        packet_init(&cmdpack, CMD_BUFFER_LENGTH, (uint8_t*) buf);
        // FS_POSITION packet with 2 data bytes, sent on file channel
        packet_set_filled(&cmdpack, channel, FS_POSITION, 2);

	endpoint_t *endpoint = buffer->real_endpoint;

	cbstat = 0;
	endpoint->provider->submit_call(endpoint->provdata, channel, 
						&cmdpack, &cmdpack, cmd_callback);
	rv = cmd_wait_cb();
debug_printf("Sent position - got: %d\n", rv); debug_flush();

	return rv;
}

/**
 * relative file read/write the current record
 */
static int8_t bufcmd_rw_record(cmdbuf_t *buffer, uint8_t is_write) {

	int8_t channel = buffer->channel_no;
	int8_t rv = CBM_ERROR_FAULT;

#ifdef DEBUG_RELFILE
	debug_printf("rw_record: chan=%d, iswrite=%d\n", channel, is_write);
	debug_flush();
#endif

	bufcmd_send_position(buffer, channel);

	if (is_write) {
		rv = bufcmd_write_buffer(channel, buffer->real_endpoint, 
				buffer->pos_of_record, buffer->recordlen);
	} else {
		if (rv == CBM_ERROR_RECORD_NOT_PRESENT) {
			uint8_t *b = buffer->buffer;
			for (uint8_t i = buffer->recordlen - 1; i >= 0; i--) {
				*(b++) = 0;
			}
			buffer->lastvalid = 0;
		} else {
			rv = bufcmd_read_buffer(channel, buffer->real_endpoint, buffer->recordlen);
			buffer->lastvalid = buffer->wptr;
			if (buffer->lastvalid > 0) {
				buffer->lastvalid --;
			}
		}
	}
	//debug_printf("Transferred data - got: %d\n", rv); debug_flush();

	buffer->cur_pos_in_record = 0;
	buffer->pos_of_record = 0;
	buffer->wptr = 0;
	buffer->rptr = 0;
	buffer->pflag |= PFLAG_PRELOAD;
	buffer->pflag &= ~PFLAG_ISREAD;

	return rv;
} 


static void relfile_submit_call(void *pdata, int8_t channelno, packet_t *txbuf, packet_t *rxbuf,
                uint8_t (*fncallback)(int8_t channelno, int8_t errnum, packet_t *rxpacket)) {

#ifdef DEBUG_BLOCK
	debug_printf("submit call for relative file, chan=%d, cmd=%d, len=%d, name=%s\n", 
		channelno, txbuf->type, txbuf->wp, ((txbuf->type == FS_OPEN_DIRECT || txbuf->type == FS_OPEN_RW) ? ((char*) txbuf->buffer+1) : ""));
debug_flush();
#endif

	uint8_t plen;
	uint8_t *ptr = NULL;
	ptr+=2;
	cmdbuf_t *buffer = NULL;

	switch(txbuf->type) {
/*
	case FS_OPEN_RD:
	case FS_OPEN_WR:
	case FS_OPEN_RW:
		// proxy the open call for relative files
		buffer = cmdbuf_find(channelno);
		if (buffer != NULL && buffer->real_endpoint != NULL) {
			cbstat = 0;
debug_printf("opened file with: rxplen=%d\n", packet_get_capacity(rxbuf)); debug_flush();
			buffer->real_endpoint->provider->submit_call(buffer->real_endpoint->provdata,
				channelno, txbuf, rxbuf, cmd_callback);

			cmd_wait_cb();
			plen = packet_get_contentlen(rxbuf);
                        uint8_t *buf = packet_get_buffer(rxbuf);
                        uint16_t reclen = 0;
debug_printf("opened file to get: plen=%d, buf[0]=%d\n", plen, buf[0]); debug_flush();
                        // store record length if given
                        if (plen == 3 && buf[0] == CBM_ERROR_OPEN_REL) {
                                reclen = (buf[1] & 0xff) | ((buf[2] & 0xff) << 8);
				// this should always be ok, as we only request valid record lengths
				// and the provider should only positively ack this when everything
				// is ok with this record length
				buffer->recordlen = reclen & 0xff;
				buffer->buf_recordno = 0;	// not loaded
				buffer->cur_pos_in_record = 0;	// not loaded
                                buf[0] = CBM_ERROR_OK;
				buffer->pflag = 0;
                        }
		}
		break;
*/
	case FS_CLOSE:
		buffer = cmdbuf_find(channelno);
		if (buffer != NULL && buffer->real_endpoint != NULL) {
			// close proxied file
			cbstat = 0;
			buffer->real_endpoint->provider->submit_call(
				buffer->real_endpoint->provdata, channelno,
				txbuf, rxbuf, &cmd_callback);
			cmd_wait_cb();
			// ignore error here?
			cmdbuf_free(channelno);
		}
		break;
	}
	fncallback(channelno, 0, rxbuf);
}

// channel_get shortcut into provider (where applicable)
int8_t relfile_get(void *pdata, int8_t channelno,
                                uint8_t *data, uint8_t *iseof, int8_t *err, uint8_t preload) {

	cmdbuf_t *buffer = cmdbuf_find(channelno);
	*err = CBM_ERROR_OK;		// just in case, for an error
	*iseof = 1;			// defaults to EOF
	if (buffer != NULL) {
		if ((buffer->pflag & PFLAG_PRELOAD) == 0) {
			// this should only happen on GET_PRELOAD, otherwise
			// it may cause a timeout error
			if ((preload & GET_PRELOAD) == 0) {
				debug_puts("NEEDED TO LOAD BUFFER DURING FETCH");
			}
			// position to record and read into buffer
			bufcmd_rw_record(buffer, 0);
		} 
		//debug_printf("rptr=%d, 1st data=%d (%02x)\n", buffer_rptr, 
		//		direct_buffer[buffer_rptr], direct_buffer[buffer_rptr]);
		*data = buffer->buffer[buffer->rptr];
		// wptr is one "behind" the rptr (which is incremented below), to
		// accomodate for the preload byte. 
		// as long as we track it here, this needs to be single-byte packets
		buffer->wptr = buffer->rptr;
		// see DOS 2.7 @ $d885 (getbyt)
		// mark so write will skip to next record
		if ((preload & GET_PRELOAD) == 0) {
			buffer->pflag |= PFLAG_ISREAD;
		}

		if (buffer->cur_pos_in_record < buffer->recordlen) {
			// check if the rest of the record is empty
			uint8_t *p = buffer->buffer + buffer->rptr + 1;
			uint8_t n = buffer->recordlen - buffer->cur_pos_in_record - 1;
			while (n) {
				//debug_printf("check n=%d, *p=%d (%d)\n", n, *p, p-buffer->buffer);
				if (*p) {
					*iseof = 0;	// not an EOF
					break;
				}
				n--;
				p++;
			}

			if ((preload & GET_PRELOAD) == 0) {
				buffer->rptr ++;
				buffer->cur_pos_in_record++;
			}
		} 

		if (*iseof && (preload & GET_PRELOAD) == 0) {
			// defaults to EOF
			// go to next record (read it when used)
			buffer->buf_recordno++;
			buffer->pflag &= ~PFLAG_ISREAD;
			if ((buffer->lastvalid - buffer->rptr) > buffer->recordlen) {
				// the following record is still in the buffer
				buffer->cur_pos_in_record = 0;
				buffer->pos_of_record += buffer->recordlen;
				buffer->rptr = buffer->pos_of_record;
				buffer->wptr = buffer->pos_of_record;
			} else {
				// read it when needed on the next read
				buffer->pflag &= ~PFLAG_PRELOAD;
			}
		}
#ifdef DEBUG_RELFILE
debug_printf("read -> %s (pload=%d, data=%d, err=%d)(ptr=%d, rec pos=%d, reclen=%d, lastvalid=%d)\n", 
		(*iseof) ? "EOF" : "WRITE", preload, *data, *err, buffer->rptr, buffer->cur_pos_in_record,
		buffer->recordlen, buffer->lastvalid);
#endif
	}

	return 0;
}


// channel_put shortcut into provider (where applicable)
// Note: (forceflush & PUT_FLUSH) indicates an EOF
int8_t relfile_put(void *pdata, int8_t channelno,
                                char c, uint8_t forceflush) {

	int8_t err = CBM_ERROR_OK;

	cmdbuf_t *buffer = cmdbuf_find(channelno);
	if (buffer != NULL) {
#ifdef DEBUG_RELFILE
		debug_printf("wptr=%d, pflag=%02x, 1st data=%d (%02x)\n", 
					buffer->wptr, 
					buffer->pflag, c, c);
#endif

		if (buffer->pflag & PFLAG_ISREAD) {
			// we need to skip to the beginning of the next record
			// and as we overwrite it, there is no need to read it first
			buffer->buf_recordno++;
			buffer->cur_pos_in_record = 0;
			buffer->pos_of_record = 0;
			buffer->rptr = 0;
			buffer->wptr = 0;
			buffer->pflag &= ~PFLAG_ISREAD;
		}
		err = CBM_ERROR_OK;

		// write single byte
		//debug_printf("w %0x @ wptr=%d, pos in rec=%d, reclen=%d\n", c, buffer->wptr, buffer->cur_pos_in_record, buffer->recordlen);
		if (buffer->cur_pos_in_record < buffer->recordlen) {
			// not rel file or end of record reached
			buffer->buffer[buffer->wptr] = c;
			// rolls over on 256
			buffer->wptr++;
			buffer->cur_pos_in_record++;
		} else {
			err = CBM_ERROR_OVERFLOW_IN_RECORD;
			//debug_printf("-> overflow %d\n", err);
			//return err;
		}

		// disable preload
		buffer->pflag |= PFLAG_PRELOAD;
		// align pointers
		buffer->rptr = buffer->wptr;
		buffer->lastvalid = (buffer->wptr == 0) ? 0 : buffer->wptr - 1;

		// PUT_FLUSH means EOF, i.e. end of write
		if (err == CBM_ERROR_OVERFLOW_IN_RECORD || forceflush & PUT_FLUSH) {
			// fill up record with zero
			while (buffer->cur_pos_in_record < buffer->recordlen) {
				buffer->buffer[buffer->wptr] = 0;
				buffer->wptr++;
				buffer->cur_pos_in_record++;
			}
			// write current record
			bufcmd_rw_record(buffer, 1);
			buffer->buf_recordno++;
			buffer->pos_of_record = 0;
			buffer->pflag = 0;
		}
	} else {
		err = CBM_ERROR_NO_CHANNEL;
	}
debug_printf("-> send err %02x\n", err);

	return err;
}

// execute a P command
int8_t bufcmd_position(bus_t *bus, char *cmdpars, uint8_t namelen, errormsg_t *errormsg) {
	int8_t rv;

	// NOP for now
	if (namelen < 3) {
		return CBM_ERROR_SYNTAX_UNKNOWN;
	}

	uint8_t channel = (uint8_t)cmdpars[0];
	uint16_t recordno = ((uint8_t)(cmdpars[1]) & 0xff) | (((uint8_t)(cmdpars[2]) & 0xff) << 8);
	uint8_t position = (namelen == 3) ? 0 : ((uint8_t)(cmdpars[3]));

	// nasty DOS bug - RECORD# adds 96 to the actual secondary channel number,
	// so we need to mask that away!
	channel &= 0x1f;

debug_printf("position: chan=%d, recordno=%d, in record=%d\n", channel, recordno, position);

	cmdbuf_t *buffer = cmdbuf_find(channel);
	if (buffer == NULL) {
		return CBM_ERROR_NO_CHANNEL;
	}

        channel_flush(channel);

	buffer->buf_recordno = recordno;
	// position is 1-based
	if (position > 0) {
		position --;
	}
	if (position == 0) {
		buffer->pflag &= ~PFLAG_ISREAD;
		buffer->pflag &= ~PFLAG_PRELOAD;
		// this send_position is only done to get the NO RECORD error.
		// wouldn't be necessary otherwise
		rv = bufcmd_send_position(buffer, channel);
	} else {
		rv = bufcmd_rw_record(buffer, 0);
		buffer->rptr += position;
		buffer->wptr += position;
	}
	buffer->cur_pos_in_record = position;
	
	return rv;
}


static charset_t charset(void *epdata) {
	return CHARSET_ASCII;
}

static provider_t block_provider = {
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

static provider_t relfile_provider = {
	NULL,			// prov_assign
	NULL,			// prov_free
	charset,		// get current character set
	NULL,			// set new charset
	NULL,			// submit
	relfile_submit_call,	// submit_call
	NULL,			// directory_converter
	relfile_get,		// channel_get
	relfile_put		// channel_put
};

static endpoint_t block_endpoint = {
	&block_provider,
	NULL
};

static endpoint_t relfile_endpoint = {
	&relfile_provider,
	NULL
};

endpoint_t *bufcmd_provider(void) {
	return &block_endpoint;
}

/*
// proxies a relative file through the bufcmd layer
int8_t bufcmd_open_relative(endpoint_t **ep, uint8_t channel_no, uint16_t reclen) {

	int8_t err = CBM_ERROR_NO_CHANNEL;

	if (reclen > 254) {
		return CBM_ERROR_OVERFLOW_IN_RECORD;
	}

	cmdbuf_t *buffer = cmdbuf_reserve(channel_no);
	if (buffer != NULL) {
		buffer->real_endpoint = *ep;
		buffer->recordlen = reclen;
		*ep = &relfile_endpoint;
		err = CBM_ERROR_OK;
	}
	return err;
}
*/

// wraps the opened channel on the original real_endpoint through the
// relative file provider, when an "CBM_ERROR_OPEN_REL" is received from the 
// server.
int8_t bufcmd_relfile_proxy(uint8_t channel_no, endpoint_t *real_endpoint, uint16_t reclen) {

	int8_t err = CBM_ERROR_NO_CHANNEL;

	if (reclen > 254) {
		return CBM_ERROR_OVERFLOW_IN_RECORD;
	}

	cmdbuf_t *buffer = cmdbuf_reserve(channel_no);
	if (buffer != NULL) {
		buffer->real_endpoint = real_endpoint;

		buffer->recordlen = reclen & 0xff;
		buffer->buf_recordno = 0;	// not loaded
		buffer->cur_pos_in_record = 0;	// not loaded
                buf[0] = CBM_ERROR_OK;
		buffer->pflag = 0;

		err = channel_reopen(channel_no, WTYPE_READWRITE, &relfile_endpoint);
		if (err != CBM_ERROR_OK) {
			cmdbuf_free(channel_no);
		}
	}

	if (err != CBM_ERROR_OK) {
		bufcmd_close(channel_no, real_endpoint);
	}
	return err;
}


