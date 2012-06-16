/****************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2012 Andre Fachat

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
 */
#define	WTYPE_READONLY	0		
#define	WTYPE_WRITEONLY	1
#define	WTYPE_READWRITE	2

/**
 * pull_state values. The interrupt callback updates the state, so state manipulation
 * must be irq-protected
 */
#define	PULL_OPEN	0		// after open
#define	PULL_PRELOAD	1		// during read pre-load
#define	PULL_ONECONV	2		// first buffer is read, but may need to be converted
#define	PULL_ONEREAD	3		// one buffer read, the other unused
#define	PULL_PULL2ND	4		// one buffer in use, the other is being pulled
#define	PULL_TWOCONV	5		// second buffer is read, but may still need to be converted
#define	PULL_TWOREAD	6		// both buffers read and valid

typedef struct {
	// channel globals
	int8_t		channel_no;
	uint8_t		current;
	uint8_t		writetype;
	provider_t	*provider;
	int8_t		(*directory_converter)(volatile packet_t *packet);
	// channel pull state - only one can be pulled at a time
	int8_t		pull_state;
	int8_t		last_pull_errorno;
	// packet area
	packet_t	buf[2];
	uint8_t		data[2][DATA_BUFLEN];
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
int8_t channel_open(int8_t chan, uint8_t writetype, provider_t *prov, int8_t (*dirconverter)(volatile packet_t *));

volatile channel_t* channel_find(int8_t chan);

static inline int8_t channel_is_writable(volatile channel_t *chan) {
	return chan->writetype == WTYPE_WRITEONLY;
}

static inline uint8_t channel_is_rel(volatile channel_t *chan) {
	// check recordlen > 0 for REL files
	return 0;	// we don't support REL files so far
}

char channel_current_byte(volatile channel_t *chan);

static inline uint8_t channel_is_eof(volatile channel_t *chan) {
        // return buf->sendeoi && (buf->position == buf->lastused);
        return packet_is_eof(&chan->buf[chan->current]);
}       

static inline uint8_t channel_current_is_eof(volatile channel_t *chan) {
        // return buf->sendeoi && (buf->position == buf->lastused);
        return packet_current_is_eof(&chan->buf[chan->current]);
}       

uint8_t channel_next(volatile channel_t *chan);

uint8_t channel_has_more(volatile channel_t *chan);

volatile channel_t* channel_refill(volatile channel_t *chan);

void channel_preload(int8_t channelno);

static inline channel_t* channel_put(volatile channel_t *chan, char c, int forceflush) {
#if 0	/* this would be the sequence to be used when we would use the original
 	   sd2iec buffers code. But we replaced it, so it only a reference here
	*/
      /* Flush buffer if full */
      if (chan->mustflush) {
        if (chan->refill(buf)) return -2;
        /* Search the buffer again,                     */
        /* it can change when using large buffers       */
        chan = find_buffer(ieee_data.secondary_address);
      }

      chan->data[chan->position] = c;
      mark_buffer_dirty(chan);

      if (chan->lastused < chan->position) {
        chan->lastused = chan->position;
      }
      chan->position++;

      /* Mark buffer for flushing if position wrapped */
      if (chan->position == 0) {
        chan->mustflush = 1;
      }

      /* REL files must be syncronized on EOI */
      if(forceflush) {
        if (chan->refill(chan)) return null;
      }
      return chan;
#endif
	return NULL;
}

void channel_close(int8_t secondary_address);

static inline void channel_status_set(uint8_t *error_buffer, int len) {
  //if (errornum >= 20 && errornum != ERROR_DOSVERSION) {
  //  FIXME: Compare to E648
  //  // NOTE: 1571 doesn't write the BAM and closes some buffers if an error occured
  //  led_state |= LED_ERROR;
  //} else {
  //  led_state &= (uint8_t)~LED_ERROR;
  //  set_error_led(0);
  //}
  //// WTF?
  //buffers[CONFIG_BUFFER_COUNT].lastused = msg - error_buffer;
  ///* Send message without the final 0x0d */
  //display_errorchannel(msg - error_buffer, error_buffer);
}

// close all channels for channel numbers between (including) the given range
void channel_close_range(uint8_t fromincl, uint8_t toincl);

#endif
