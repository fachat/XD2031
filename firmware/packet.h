/****************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2012 Andre Fachat

    Derived from:
    OS/A65 Version 1.3.12
    Multitasking Operating System for 6502 Computers
    Copyright (C) 1989-1997 Andre Fachat

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation;
    version 2 of the License ONLY.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

****************************************************************************/

/**
 * Packet handling
 */

#ifndef PACKET_H
#define	PACKET_H

#include <inttypes.h>

#include "wireformat.h"
#include "delay.h"

#include "debug.h"

/***********************************************************************************
 * packet stuff 
 *
 * A packet is a structure to communicate with the pc server. packets are sent and
 * received via serial line.
 *
 * Logically and on the wire a packet contains a type, a packet length, and the data payload.
 * The length on the wire contains the type and length bytes, so it is payload length plus two.
 *
 * A packet structure contains a buffer pointer, with length, read and write pointers, and
 * a read/write state machine
 *
 * The internal length and read/write pointers work on the data payload, so packet.len is
 * two bytes short of the length on the wire.
 *
 * Note that some types of packets contain the channel number as first byte of the payload.
 * Also all FS_REPLY packets have this, so this knowledge is hardcoded in the uart code.
 */

#define	PACKET_EMPTY	0
#define	PACKET_WR	1
#define	PACKET_TYPE	2
#define	PACKET_LEN	3
#define	PACKET_DATA	4
#define	PACKET_DONE	5

typedef	uint8_t		bool;

typedef struct {
	uint8_t		type;		// one of the FS_* codes as packet type
	uint8_t 	*buffer;	// address of buffer
	uint8_t		wp;		// write pointer (write to buffer[wp], then wp++)
	uint8_t		rp;		// read pointer (if rp<wp then read from buffer[rp], then rp++)
	uint8_t		len;		// length of buffer
	int8_t		chan;		// channel number
} packet_t;

static inline int8_t packet_get_chan(packet_t *packet) {
	return packet->chan;
}

static inline int8_t packet_get_type(packet_t *packet) {
	return packet->type;
}

static inline int8_t packet_get_contentlen(packet_t *packet) {
	return packet->wp;
}

static inline int8_t packet_get_capacity(packet_t *packet) {
	return packet->len;
}

static inline uint8_t* packet_get_buffer(packet_t *packet) {
	return packet->buffer;
}

static inline void packet_init(packet_t *packet, uint8_t len, uint8_t *buffer) {

	packet->buffer = buffer;
	packet->len = len;
	packet->chan = -3;
	packet->rp = 0;
	packet->wp = 0;
}

static inline void packet_reset(packet_t *packet, uint8_t channo) {
	packet->rp = 0;
	packet->wp = 0;
	packet->chan = channo;
}

static inline bool packet_is_done(packet_t *buf) {
	return buf->wp == buf->rp;
}

static inline bool packet_is_full(packet_t *buf) {
	return buf->wp == buf->len;
}

static inline void packet_wait_done(packet_t *buf) {
	while( ! packet_is_done(buf) ) { 
		delayms(1);
	}
}

static inline void packet_set_read(packet_t *packet) {
	packet->rp = 0;
}

static inline int8_t packet_set_write(packet_t *packet, int8_t chan, uint8_t type, uint8_t datalen) {
	if (datalen > packet->len) {
		return -1;
	}
	packet->type = type;
	packet->chan = chan;
	packet->wp = 0;
	return 0;
}

/**
 * tell the packet that it has been filled "out of band", e.g. by directly writing to 
 * the buffer the packet points to.
 */
static inline void packet_set_filled(packet_t *packet, int8_t chan, uint8_t type, uint8_t datalen) {
	packet->type = type;
	packet->chan = chan;
	packet->wp = datalen;
	packet_set_read(packet);
}

static inline void packet_update_wp(packet_t *packet, int8_t datalen) {
	packet->wp = datalen;
}

/*
 * writes a byte into the buffer.
 * Note: no bounds check!
 */
static inline void packet_write_char(packet_t *buf, uint8_t ch) {
	buf->buffer[buf->wp++] = ch;
}

static inline uint8_t packet_read_char(packet_t *buf) {
	return buf->buffer[buf->rp++];
}


// interface for IEEE side. Ignores the type and packet bytes
static inline uint8_t packet_peek_data(packet_t *buf) {
	return buf->buffer[buf->rp];
}

// commit read: advance read pointer, check if there is still data to read available
//
// stop sending if we had an EOI on a normal file (non command)
// that is also not a relative file (recordlen), and also not a
// direct buffer (whatever that is...)
//if(buf->sendeoi && ieee_data.secondary_address != 0x0f &&
//  !buf->recordlen && buf->refill != directbuffer_refill) {
//  buf->read = 0;
//}
// return buf->position++ < buf->lastused;
static inline uint8_t packet_next(packet_t *buf) {
	buf->rp++;
	return (buf->rp < buf->wp) ? 1 : 0;
}

static inline uint8_t packet_is_last(packet_t *buf) {
//debug_puts("packet_eof: "); debug_puthex(buf->type); debug_puthex(buf->rp); debug_puthex(buf->wp); debug_putcrlf();
	return (buf->type == ((uint8_t)(FS_EOF & 0xff)));
}

static inline uint8_t packet_is_eof(packet_t *buf) {
//debug_puts("packet_eof: "); debug_puthex(buf->type); debug_puthex(buf->rp); debug_puthex(buf->wp); debug_putcrlf();
	return (buf->type == ((uint8_t)(FS_EOF & 0xff))) && (buf->rp >= buf->wp);
}

static inline uint8_t packet_current_is_eof(packet_t *buf) {
//debug_puts("packet_eof: "); debug_puthex(buf->type); debug_puthex(buf->rp); debug_puthex(buf->wp); debug_putcrlf();
	return (buf->type == ((uint8_t)(FS_EOF & 0xff))) && ((buf->rp + 1) >= buf->wp);
}

static inline bool packet_has_data(packet_t *buf) {
	return (buf->rp < buf->wp) ? 1 : 0;
}

#endif

