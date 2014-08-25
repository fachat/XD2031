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

#include <stdint.h>
#include <string.h>
/*
#include <errno.h>
#include <stdlib.h>

#include "channel.h"
#include "packet.h"

#include "term.h"
#include "led.h"
*/
#include "bus.h"
#include "errormsg.h"
#include "provider.h"
#include "buffer.h"
#include "relfile.h"
#include "debug.h"

#undef	DEBUG_RELFILE


               	
// ----------------------------------------------------------------------------------

void relfile_init() {
}

// ----------------------------------------------------------------------------------
// provider for relative files

/**
 * send FS_POSITION
 */
static int8_t relfile_send_position(cmdbuf_t *buffer, int8_t channel) {

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

        packet_init(&buf_cmdpack, CMD_BUFFER_LENGTH, (uint8_t*) buf);
        // FS_POSITION packet with 2 data bytes, sent on file channel
        packet_set_filled(&buf_cmdpack, channel, FS_POSITION, 2);

	endpoint_t *endpoint = buffer->real_endpoint;

	rv = buf_call(endpoint, endpoint->provdata, channel, &buf_cmdpack, &buf_cmdpack);
/*
	cbstat = 0;
	endpoint->provider->submit_call(endpoint->provdata, channel, 
						&buf_cmdpack, &buf_cmdpack, cmd_callback);
	rv = cmd_wait_cb();
*/
debug_printf("Sent position - got: %d\n", rv); debug_flush();

	return rv;
}

/**
 * relative file read/write the current record
 */
static int8_t relfile_rw_record(cmdbuf_t *buffer, uint8_t is_write) {

	int8_t channel = buffer->channel_no;
	int8_t rv = CBM_ERROR_FAULT;

#ifdef DEBUG_RELFILE
	debug_printf("rw_record: chan=%d, iswrite=%d\n", channel, is_write);
	debug_flush();
#endif

	relfile_send_position(buffer, channel);

	if (is_write) {
		rv = buffer_write_buffer(channel, buffer->real_endpoint, 
				buffer->pos_of_record, buffer->recordlen);
	} else {
		if (rv == CBM_ERROR_RECORD_NOT_PRESENT) {
			memset(buffer->buffer, 0, buffer->recordlen);
			buffer->lastvalid = 0;
		} else {
			rv = buffer_read_buffer(channel, buffer->real_endpoint, buffer->recordlen);
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

	uint8_t *ptr = NULL;
	ptr+=2;
	cmdbuf_t *buffer = NULL;

	switch(txbuf->type) {
	case FS_CLOSE:
		buffer = buf_find(channelno);
		if (buffer != NULL && buffer->real_endpoint != NULL) {
			// close proxied file
			buf_call(buffer->real_endpoint, buffer->real_endpoint->provdata, channelno, txbuf, rxbuf);
/*
			cbstat = 0;
			buffer->real_endpoint->provider->submit_call(
				buffer->real_endpoint->provdata, channelno,
				txbuf, rxbuf, &cmd_callback);
			cmd_wait_cb();
*/
			// ignore error here?
			buf_free(channelno);
		}
		break;
	}
	fncallback(channelno, 0, rxbuf);
}

// channel_get shortcut into provider (where applicable)
int8_t relfile_get(void *pdata, int8_t channelno,
                                uint8_t *data, uint8_t *iseof, int8_t *err, uint8_t preload) {

	cmdbuf_t *buffer = buf_find(channelno);
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
			relfile_rw_record(buffer, 0);
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

	cmdbuf_t *buffer = buf_find(channelno);
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
			relfile_rw_record(buffer, 1);
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
int8_t relfile_position(bus_t *bus, char *cmdpars, uint8_t namelen, errormsg_t *errormsg) {
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

	cmdbuf_t *buffer = buf_find(channel);
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
		rv = relfile_send_position(buffer, channel);
	} else {
		rv = relfile_rw_record(buffer, 0);
		buffer->rptr += position;
		buffer->wptr += position;
	}
	buffer->cur_pos_in_record = position;
	
	return rv;
}


static charset_t charset(void *epdata) {
	return CHARSET_ASCII;
}

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

static endpoint_t relfile_endpoint = {
	&relfile_provider,
	NULL
};

// wraps the opened channel on the original real_endpoint through the
// relative file provider, when an "CBM_ERROR_OPEN_REL" is received from the 
// server.
int8_t relfile_proxy(uint8_t channel_no, endpoint_t *real_endpoint, uint16_t reclen) {

	int8_t err = CBM_ERROR_NO_CHANNEL;

	if (reclen > 254) {
		return CBM_ERROR_OVERFLOW_IN_RECORD;
	}

	cmdbuf_t *buffer = buf_reserve(channel_no);
	if (buffer != NULL) {
		buffer->real_endpoint = real_endpoint;

		buffer->recordlen = reclen & 0xff;
		buffer->buf_recordno = 0;	// not loaded
		buffer->cur_pos_in_record = 0;	// not loaded
                buf[0] = CBM_ERROR_OK;
		buffer->pflag = 0;

		err = channel_reopen(channel_no, WTYPE_READWRITE, &relfile_endpoint);
		if (err != CBM_ERROR_OK) {
			buf_free(channel_no);
		}
	}

	if (err != CBM_ERROR_OK) {
		buffer_close(channel_no, real_endpoint);
	}
	return err;
}


