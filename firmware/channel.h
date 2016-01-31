/************************************************************************

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
 * This file implements the central communication channels between the
 * IEEE layer and the provider layers
 */

#ifndef CHANNEL_H
#define CHANNEL_H

#include <stdio.h>

#include "packet.h"
#include "provider.h"

#define	DATA_BUFLEN	64

/**
 * writetype values as seen from the IEEE device
 *
 * READONLY and WRITEONLY files use the two buffers as 
 * alternating double-buffering buffers. So one buffer is loaded
 * from the device while the other is being sent to the server
 * or vice versa. 
 * The READWRITE files use buffer 0 to send to host, and buffer 1
 * to receive from host only, so no double-buffering is done there.
 */
#define	WTYPE_READONLY		0
#define	WTYPE_WRITEONLY		1
#define	WTYPE_READWRITE		2

#define	WTYPE_MASK		0x03	// mask to get rid of the following options

#define	WTYPE_NONBLOCKING	128	// ORd with one of the other three

/**
 * R/W buffer definitions
 */
#define	RW_PUSHBUF	0
#define	RW_PULLBUF	1

/**
 * pull_state values. The delay callback updates the state
 * pull means read (pull) data from the server
 */
#define	PULL_OPEN	0	// after open
#define	PULL_PRELOAD	1	// during read pre-load
#define	PULL_ONECONV	2	// first buffer is read, but may need to be converted
#define	PULL_ONEREAD	3	// one buffer read, the other unused
#define	PULL_PULL2ND	4	// one buffer in use, the other is being pulled
#define	PULL_TWOCONV	5	// second buffer is read, but may still need to be converted
#define	PULL_TWOREAD	6	// both buffers read and valid

/**
 * push_state values. The delay callback updates the state
 * push means send (push) data to the server
 */
#define	PUSH_OPEN	0	// after open
#define	PUSH_FILLONE	1	// filling the first buffer
#define	PUSH_FILLTWO	2	// filling the second buffer
#define	PUSH_CLOSE	3	// channel has been closed

#define	PUT_FLUSH	0x01
#define	PUT_SYNC	0x02

#define	GET_WAIT	0x01
#define	GET_SYNC	0x02
#define	GET_PRELOAD	0x80

typedef struct {
	// channel globals
	int8_t channel_no;
	uint8_t current;
	uint8_t writetype;
	uint8_t options;
	endpoint_t *endpoint;
	// close callback
        void (*close_callback)(int8_t errnum, uint8_t *rxdata);
	// directory handling
	uint8_t drive;
	 int8_t(*directory_converter) (void *ep, packet_t * packet,
				       uint8_t drive);
	// channel pull state - only one can be pulled at a time
	int8_t pull_state;
	int8_t last_pull_errorno;
	// channel push state - only one can be pushed at a time
	int8_t push_state;
	int8_t last_push_errorno;
	// channel state
	uint8_t had_data;
	// packet area
	packet_t buf[2];
	uint8_t data[2][DATA_BUFLEN];
} channel_t;

/*
 * init the channel code
 */
void channel_init(void);

/*
 * open and reserve a channel for the given chan channel number
 * returns negative on error, 0 on ok
 *
 * writetype is either 0 for read only, 1 for write, (as seen from ieee device)
 */
int8_t channel_open(int8_t chan, uint8_t writetype, endpoint_t * prov,
		    int8_t(*dirconverter) (void *ep, packet_t *, uint8_t drive),
		    uint8_t drive);

/*
 * re-open a file to wrap the channel for a relative file
 */
int8_t channel_reopen(int8_t chan, uint8_t writetype, endpoint_t * prov);

channel_t *channel_find(int8_t chan);

/**
 * flushes all messages, i.e. waits until writes are acknowledged
 * and in-progress reads are thrown away.
 * Used for block/user commands; returns channel_t* as convenience
 */
channel_t *channel_flush(int8_t chan);

/*
 * receives a byte
 *
 * data and iseof are out parameters for the received data. 
v* eof is set when an EOF has been received, cleared otherwise
 * err is set to the error number when applicable.
 *
 * flags is input; use the GET_* flags from above.
 * i.e. it returns when data is available or on error. When == 0 then
 * returns immediately (after sending FS_READ in the background)
 *
 * returns 0 on ok, -1 on error
 */
int8_t channel_get(channel_t * chan, uint8_t * data, uint8_t * iseof,
		   int8_t * err, uint8_t flags);

/*
 * send a byte
 *
 * for forceflush use the PUT_* flags from above.
 */
int8_t channel_put(channel_t * chan, uint8_t c, uint8_t forceflush);

cbm_errno_t channel_close(int8_t secondary_address, void (*close_callback)(int8_t errno, uint8_t *rxdata));

// close all channels for channel numbers between (including) the given range
void channel_close_range(uint8_t fromincl, uint8_t toincl);

#endif
