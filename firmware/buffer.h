/****************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2012 Andre Fachat

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

****************************************************************************/

/**
 * This file implements the disk drive commands
 */

#ifndef BUFFER_H
#define BUFFER_H

// ----------------------------------------------------------------------------------
// buffer handling (#-file, U1/U2/B-W/B-R/B-P
// we only have a single buffer

typedef struct {
	// what channel has the buffer, -1 unused
	int8_t channel_no;
	// read and write pointers in the buffer
	// wptr is one "behind" the rptr (which is incremented below), to
	// accomodate for the preload byte
	uint8_t rptr;		// read pointer
	uint8_t wptr;		// read pointer
	// preload flag
	uint8_t pflag;		// see PFLAG defines below
	// pointer to the last valid bytes (can be 0-255, where 0 is only the pointer is valid)
	uint8_t lastvalid;
	// relative file information
	// the proxied endpoint for rel files
	endpoint_t *real_endpoint;
	// record length (max 254 byte)
	uint8_t recordlen;
	// the record number for the (first) record in the buffer
	uint16_t buf_recordno;
	// position of current record in buffer (multiple may be loaded in one read)
	uint8_t pos_of_record;
	// current position in record
	uint8_t cur_pos_in_record;
	// the actual 256 byte buffer
	uint8_t buffer[256];

} cmdbuf_t;

extern cmdbuf_t buffers[];

// place for command, channel, drive, track sector 
#define CMD_BUFFER_LENGTH       FS_BLOCK_PAR_LEN

extern char buf[];

#define PFLAG_PRELOAD           1	// preload has happened
#define PFLAG_ISREAD            2	// buffer has been read from (for rel files)

void buffer_init();

// reserve a free buffer for the given channel
cmdbuf_t *buf_reserve(int8_t channel_no);

// reserve a given buffer for the channel
cmdbuf_t *buf_reserve_buf(int8_t channel_no, uint8_t bufno);

// find buffer for channel
cmdbuf_t *buf_find(int8_t channel_no);

// free a buffer
uint8_t buf_free(int8_t channel_no);

// set up cmdpack to send and datapack to receive, then call cmdbuf_call()
extern packet_t buf_cmdpack;
extern packet_t buf_datapack;
uint8_t buf_call(endpoint_t * ep, void *provdata, uint8_t channelno,
		 packet_t * sendbuf, packet_t * rxbuf);

void buffer_close(uint8_t channel_no, endpoint_t * endpoint);
uint8_t buffer_write_buffer(uint8_t channel_no, endpoint_t * endpoint,
			    uint8_t start_of_data, uint16_t send_nbytes);
uint8_t buffer_read_buffer(uint8_t channel_no, endpoint_t * endpoint,
			   uint16_t receive_nbytes);

#endif
