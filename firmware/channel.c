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
 * This file implements the central communication channels between the 
 * IEEE layer and the provider layers
 */

#include <stdio.h>
#include <inttypes.h>

#include "delay.h"
#include "packet.h"
#include "channel.h"
#include "provider.h"

#include "serial.h"

#include "led.h"
#include "debug.h"
#include "term.h"

#undef DEBUG_CHANNEL

#define	MAX_CHANNELS	4		// number of maximum open channels

channel_t channels[MAX_CHANNELS];

static uint8_t _push_callback(int8_t channelno, int8_t errnum, packet_t *rxpacket);
static void channel_close_int(channel_t *chan, uint8_t force);
static void channel_write_flush(channel_t *chan, packet_t *curpack, uint8_t forceflush);
static channel_t* channel_refill(channel_t *chan, uint8_t options);

static inline int8_t channel_last_pull_error(channel_t *chan) {
        return chan->last_pull_errorno;
}

static inline int8_t channel_last_push_error(channel_t *chan) {
        return chan->last_push_errorno;
}

void channel_init(void) {
	for (int8_t i = MAX_CHANNELS-1; i>= 0; i--) {
		channel_close_int(&channels[i], 1);
	}
}

static uint8_t _pull_callback(int8_t channel_no, int8_t errorno, packet_t *rxpacket) {
	channel_t *p = channel_find(channel_no);
	if (p != NULL) {
		if (errorno < 0 || rxpacket == NULL) {
                	p->last_pull_errorno = CBM_ERROR_FAULT;
		} else if (packet_get_type(rxpacket) == FS_REPLY) {
			p->last_pull_errorno = packet_get_buffer(rxpacket)[0];
		} else {
			p->last_pull_errorno = CBM_ERROR_OK;
		}

		// TODO: only if errorno == 0?
		// Probably need some PULL_ERROR as well	
		if (p->pull_state == PULL_PRELOAD) {
			p->pull_state = PULL_ONECONV;
		} else
		if (p->pull_state == PULL_PULL2ND) {
			p->pull_state = PULL_TWOCONV;
		}
	}
	return 0;
}

static uint8_t _close_callback(int8_t channel_no, int8_t errorno, packet_t *rxpacket) {
	channel_t *p = channel_find(channel_no);
	if (p != NULL) {
debug_printf("close_cb: c=%d, errorno=%d, rxp=%p\n", channel_no, errorno, rxpacket);
                p->last_push_errorno = errorno;
		p->push_state = PUSH_CLOSE;
	}
	return 0;
}

static inline uint8_t channel_is_eof(channel_t *chan) {
        // return buf->sendeoi && (buf->position == buf->lastused);
        return packet_is_eof(&chan->buf[chan->current]);
}       

/**
 * pull in a buffer from the server
 *
 * The "slot" number determines which of the channel packet buffers is used
 */
static void channel_pull(channel_t *c, uint8_t slot, uint8_t options) {
	packet_t *p = &c->buf[slot];

#ifdef DEBUG_CHANNEL
	debug_printf("pull: chan=%p, channo=%d (ep=%p)\n",
		c, c->channel_no, (void*)c->endpoint);
#endif

	// prepare to write a buffer with length 0
	packet_set_filled(p, c->channel_no, FS_READ, 0);

	endpoint_t *endpoint = c->endpoint;

	// not irq-protected, as exlusive state conditions
	if (c->pull_state == PULL_OPEN) {
		c->pull_state = PULL_PRELOAD;
		endpoint->provider->submit_call(endpoint->provdata, c->channel_no, p, p, _pull_callback);

		if (options & GET_SYNC) {
			while (c->pull_state == PULL_PRELOAD) {
				delayms(1);
			}
		}
	} else
	if (c->pull_state == PULL_ONEREAD && c->writetype == WTYPE_READONLY) {
		// only if we're read-only pull in second buffer
		c->pull_state = PULL_PULL2ND;
		endpoint->provider->submit_call(endpoint->provdata, c->channel_no, p, p, _pull_callback);

		if (options & GET_SYNC) {
			while (c->pull_state == PULL_PULL2ND) {
				delayms(1);
			}
		}
	}
}

/*
 * open and reserve a channel for the given chan channel number
 * returns negative on error, 0 on ok
 *
 * writetype is either 0 for read only, 1 for write, (as seen from ieee device)
 */
int8_t channel_open(int8_t chan, uint8_t writetype, endpoint_t *prov, 
	int8_t (*dirconv)(void *ep, packet_t *, uint8_t drive), 
	uint8_t drive) {

#ifdef DEBUG_CHANNEL
	debug_printf("channel_open: chan=%d\n", chan);
#endif

	for (int8_t i = MAX_CHANNELS-1; i>= 0; i--) {
		if (channels[i].channel_no < 0) {
			channels[i].channel_no = chan;
			channels[i].current = 0;
			channels[i].writetype = writetype & WTYPE_MASK;
			channels[i].options = writetype & ~WTYPE_MASK;
#ifdef DEBUG_CHANNEL
			debug_printf("wtype=%d, option=%d\n", channels[i].writetype,
				channels[i].options);
#endif
			channels[i].endpoint = prov;
			channels[i].directory_converter = dirconv;
			channels[i].drive = drive;
			channels[i].pull_state = PULL_OPEN;
			channels[i].push_state = PUSH_OPEN;
			// note: we should not channel_pull() here, as file open has not yet even been sent
			// the pull is done in the open callback for a read-only channel
			for (uint8_t j = 0; j < 2; j++) {
				packet_reset(&channels[i].buf[j], chan);
			}
			return 0;
		}
	}
	return -1;
}

/*
 * re-open a file to wrap the channel for a relative file
 */
int8_t channel_reopen(int8_t chan, uint8_t writetype, endpoint_t *prov) {
	
	channel_t *channel = channel_find(chan);
	if (channel != NULL) {
		channel->endpoint = prov;
		channel->writetype = writetype & WTYPE_MASK;
		channel->options = writetype & ~WTYPE_MASK;
	} else {
		return CBM_ERROR_NO_CHANNEL;
	}
	
	return CBM_ERROR_OK;
}


channel_t* channel_find(int8_t chan) {
	for (int8_t i = MAX_CHANNELS-1; i>= 0; i--) {
		if (channels[i].channel_no == chan) {
			return &channels[i];	
		}
	}
	return NULL;
}

static inline uint8_t pull_slot(channel_t *chan) {
	return (chan->writetype == WTYPE_READWRITE) ? RW_PULLBUF : chan->current;
}

static inline uint8_t push_slot(channel_t *chan) {
	return (chan->writetype == WTYPE_READWRITE) ? RW_PUSHBUF : chan->current;
}

channel_t* channel_flush(int8_t channo) {

	channel_t *chan = channel_find(channo);
	if (chan == NULL) {
		term_printf("DID NOT FIND CHANNEL TO FLUSH FOR %d\n", channo);
		return NULL;
	}

	packet_t *curpack = &chan->buf[push_slot(chan)];

	if (chan->push_state != PUSH_OPEN) {
		channel_write_flush(chan, curpack, PUT_SYNC);
	}

	while (chan->pull_state == PULL_PRELOAD
		|| chan->pull_state == PULL_PULL2ND) {

		delayms(1);
		main_delay();
	}

	//debug_printf("pull_state on flush: %d\n", chan->pull_state);
	chan->pull_state = PULL_OPEN;

	return chan;
}

// returns 0 when data is available, and -1 when no data is available
static int8_t channel_preload_int(channel_t *chan, uint8_t wait) {

	// TODO:fix for read/write
	if (chan->writetype == WTYPE_WRITEONLY) return -1;

	do {
	    if (chan->pull_state == PULL_OPEN) {
		chan->current = 0;
		chan->current = pull_slot(chan);
		channel_pull(chan, chan->current, GET_SYNC);
	    }
	    if (wait) {
		// if we need to wait, well, wait until the 
		// request has been received, processed, and answered, and the
		// pull_callback (called from deep within delayms()) has updated
		// the status
		while (chan->pull_state == PULL_PRELOAD) {
			delayms(1);
		}
	    }
	    if (chan->pull_state == PULL_ONECONV) {
		//debug_puts("Got one packet (PULL_ONECONV)!\n");
		// one packet received
		packet_t *curpack = &(chan->buf[chan->current]);
		if ((!packet_has_data(curpack)) && (!packet_is_last(curpack))) {
			// zero length packet received
			chan->pull_state = PULL_OPEN;
			// if we have a non-blocking channel, a zero-length packet is 
			// fully ok. 
			if (chan->options & WTYPE_NONBLOCKING) {
				return -1;
			}
		} else {
			if (chan->directory_converter != NULL) {
				//debug_printf(">>1: %p, p=%p\n",chan->directory_converter, &chan->buf[chan->current]);
				//debug_printf(">>1: b=%p\n", packet_get_buffer(&chan->buf[chan->current]));
				chan->directory_converter(chan->endpoint, &chan->buf[chan->current], chan->drive);
			}
			// we have one packet, and it's already converted as well
			chan->pull_state = PULL_ONEREAD;
		}
	    }
	    if (chan->pull_state == PULL_TWOCONV) {
		// we already have received a second packet
		// (so we basically did a fall-through through the code above)
		// should only happen on READONLY anyway
		packet_t *opack = &(chan->buf[1-chan->current]);
		if ((!packet_has_data(opack)) && (!packet_is_last(opack))) {
			// zero length packet received
			chan->pull_state = PULL_ONEREAD;
		} else {
			if (chan->directory_converter != NULL) {
				//debug_printf(">>2: %p, p=%p\n",chan->directory_converter, &chan->buf[1-chan->current]);
				//debug_printf(">>2: b=%p\n", packet_get_buffer(&chan->buf[1-chan->current]));
				chan->directory_converter(chan->endpoint, &chan->buf[1-chan->current], chan->drive);
			}
			chan->pull_state = PULL_TWOREAD;
		}
	    }
	}
	while (chan->pull_state == PULL_OPEN);

	return 0;
}

/**
 * pre-load data for a read-only channel
 */
static int8_t channel_preloadp(channel_t *chan) {
	return channel_preload_int(chan, 1);
}

static char channel_current_byte(channel_t *chan, uint8_t *iseof) {
	channel_preload_int(chan, 1);
	*iseof = packet_current_is_eof(&chan->buf[chan->current]);
        return packet_peek_data(&chan->buf[pull_slot(chan)]);
}

/**
 * return true (non-zero) when there is still a byte available in the buffer
 */
static uint8_t channel_next(channel_t *chan, uint8_t options) {

// commenting this is not necessarily ok, but needed for relative files for now
//	if (channel_is_eof(chan)) {
//		return 0;
//	}

	// make sure we do have something at least
	int8_t no_data = channel_preload_int(chan, 1);

	if (chan->writetype == WTYPE_READONLY) {
		// this is an optimization:
		// pull in the "other" buffer in the background
		uint8_t other = 1-chan->current;
		//if (packet_is_done(&chan->buf[other]) && (!packet_is_eoi(&chan->buf[chan->current]))) {
		if ((chan->pull_state == PULL_ONEREAD) && (!packet_is_last(&chan->buf[chan->current]))) {
			// if the other packet is free ("done"), and the current packet is not the last one ("eoi")
			// We should only do this on "standard" files though, not relative or others
			channel_pull(chan, other, options);
		}
	}

	if (!no_data) {
		// we should have some data
		if (packet_next(&chan->buf[pull_slot(chan)])) {
			return 1;	// ok
		}

// commenting this is not necessarily ok, but needed for relative files for now
//		if (!packet_is_eof(&chan->buf[pull_slot(chan)])) {
			// not eof packet, so pull another one
			channel_refill(chan, options);
			return 1;
//		}
	}
	return 0;
}

static void channel_close_int(channel_t *chan, uint8_t force) {
	// do nothing but mostly invalidate channel_no,
	// as we do not currently support FS_OPEN_RW yet, which
	// would require an explicit close on the server

	chan->channel_no = -1;
	chan->pull_state = PULL_OPEN;
	chan->push_state = PUSH_OPEN;
	packet_init(&chan->buf[0], DATA_BUFLEN, chan->data[0]);
	packet_init(&chan->buf[1], DATA_BUFLEN, chan->data[1]);
}

void channel_close(int8_t channel_no) {

	// flush out any remaining data (without EOF)
	channel_t *chan = channel_flush(channel_no);

#ifdef DEBUG_CHANNEL
	debug_printf("channel_close(%p -> %d), push=%d, pull=%d\n", chan, channel_no,
		chan == NULL ? -1 : chan->push_state, chan == NULL ? -1 : chan->pull_state); debug_flush();
#endif

	if (chan != NULL) {

		// send FS_CLOSE packet
		packet_t *curpack = &chan->buf[pull_slot(chan)];
	        packet_set_filled(curpack, channel_no, FS_CLOSE, 0);

		endpoint_t *endpoint = chan->endpoint;
       	        endpoint->provider->submit_call(endpoint->provdata, 
			channel_no, curpack, curpack, _close_callback);

       		// wait until the packet has been sent and been responded to
       		while (chan->push_state != PUSH_CLOSE) {
               	       	delayms(1);
		}	

		channel_close_int(chan, 0);
	}
}

/**
 * should be called ONLY(!) when channel_next() false has indicated that
 * the current buffer is empty, but channel_has_more() true has indicated
 * that the current buffer is not the last one
 */
static channel_t* channel_refill(channel_t *chan, uint8_t options) {
	// buf->refill();
	// buf = find_buffer(...)
	//
	if (chan->writetype == WTYPE_READONLY) {
	    uint8_t other = 1-chan->current;
	    if (!packet_is_last(&chan->buf[chan->current])) {
		// current packet is not last one
		// other packet should have been pulled in channel_next()
		// so it is either empty (request, or FS_EOF), or has data
		packet_t *opacket = &chan->buf[other];

		// wait until available	
		while(chan->pull_state == PULL_PULL2ND) {
			delayms(1);
		}
		if (chan->pull_state == PULL_TWOCONV) {
			if (chan->directory_converter != NULL) {
				chan->directory_converter(chan->endpoint, &chan->buf[1-chan->current], chan->drive);
			}
			chan->pull_state = PULL_TWOREAD;
		}
		
		if (packet_has_data(opacket)) {
			// switch packets
			chan->current = other;

			chan->pull_state = PULL_ONEREAD;

			if (!packet_is_last(&chan->buf[chan->current])) {
				channel_pull(chan, 1-other, options);
			}
			return chan;
		}
	    }
	} else {
		// WTYPE_READWRITE
		chan->pull_state = PULL_OPEN;
		return chan;
	}

	// no more data - close it
	//channel_close_int(chan, 0);
	return NULL;
}

static uint8_t _push_callback(int8_t channelno, int8_t errnum, packet_t *rxpacket) {
        channel_t *p = channel_find(channelno);
        if (p != NULL) {
#ifdef DEBUG_CHANNEL
		debug_printf("push_cb: c=%d, p=%p, type=%d, errnum=%d, rxp=%p, p[0]=%02x\n",
			channelno, p, packet_get_type(rxpacket), errnum, rxpacket,  packet_get_buffer(rxpacket)[0]);
#endif
		if (errnum < 0 || rxpacket == NULL) {
                	p->last_push_errorno = CBM_ERROR_FAULT;
		} else if (packet_get_type(rxpacket) == FS_REPLY) {
			p->last_push_errorno = packet_get_buffer(rxpacket)[0];
		} else {
			p->last_push_errorno = CBM_ERROR_OK;
		}
#ifdef DEBUG_CHANNEL
		if (p->last_push_errorno) debug_printf("last_push_errno -> %d\n", p->last_push_errorno);
#endif

                // TODO: only if errorno == 0?
                // Probably need some PUSH_ERROR as well
                if (p->push_state == PUSH_FILLTWO) {
			// release possibly waiting channel_put()
                        p->push_state = PUSH_FILLONE;
                } else 
		if (p->push_state == PUSH_FILLONE) {
			// release possibly waiting channel_close()
			p->push_state = PUSH_OPEN;
		}
        }
	return 0;
}
	

int8_t channel_put(channel_t *chan, uint8_t c, uint8_t forceflush) {

	uint8_t channo = chan->channel_no;

	// provider shortcut where applicable
	if (chan->endpoint->provider->channel_put != NULL) {
		return chan->endpoint->provider->channel_put(chan->endpoint->provdata, channo, c, forceflush);
	}

	packet_t *curpack = &chan->buf[push_slot(chan)];

#ifdef DEBUG_CHANNEL
	debug_printf("channel_put(%02x), flush=%d, push_state=%d\n", c, forceflush, chan->push_state);
#endif

	if (chan->push_state == PUSH_OPEN) {
		chan->push_state = PUSH_FILLONE;
		packet_reset(curpack, channo);
	}

	packet_write_char(curpack, (uint8_t) c);

	if (packet_is_full(curpack) || (forceflush & PUT_FLUSH)) {

		channel_write_flush(chan, curpack, forceflush);

	}

	return channel_last_push_error(chan);
}

static void channel_write_flush(channel_t *chan, packet_t *curpack, uint8_t forceflush) {

		uint8_t channo = chan->channel_no;

		packet_set_filled(curpack, channo, 
			(forceflush & PUT_FLUSH) ? FS_EOF : FS_WRITE, 
			packet_get_contentlen(curpack));

		// wait until the other packet has been replied to,
		// i.e. it has been sent, the buffer is free again 
		// which we need for the next channel_put
		while (chan->push_state == PUSH_FILLTWO) {
			delayms(1);
		}

		if (packet_get_contentlen(curpack) != 0) {
			// note that we are pushing one and are now filling the second
			// change that before pushing, as callback might already be done during push
			chan->push_state = PUSH_FILLTWO;

			// use same packet as rx/tx buffer
			endpoint_t *endpoint = chan->endpoint;
			endpoint->provider->submit_call(endpoint->provdata, 
				channo, curpack, curpack, _push_callback);

			// callback is already done?
			if (chan->push_state == PUSH_FILLONE) {
				// TODO the need for this may indicate a race condition
				// buffer PRINT# with single chars trigger a two-byte
				// instead of one-byte packet on direct blocks, but not on
				// files
				chan->push_state = PUSH_OPEN;
			}
		}

		if (chan->writetype == WTYPE_WRITEONLY) {
			// switch packet buffers for double buffering
			chan->current = 1-chan->current;
			packet_reset(&chan->buf[chan->current], channo);
		}

		if ((chan->writetype == WTYPE_READWRITE) || (forceflush & PUT_SYNC)) {
			// we are forced to wait for the reply, e.g. from the IEC code
			// as the interrupt block prevents us from really receiving all
			// replies - so we have to make sure we really got it

			// we also wait in case we're read/write, to make sure the 
			// buffer is free for the next write.

			// wait until the other packet has been replied to,
			// i.e. it has been sent, the buffer is free again 
			// which we need for the next channel_put
			while (chan->push_state == PUSH_FILLTWO) {
				delayms(1);
				main_delay();
			}

			if (chan->writetype == WTYPE_READWRITE) {
				chan->push_state = PUSH_OPEN;
			}
		}
}

// close all channels for channel numbers between (including) the given range
void channel_close_range(uint8_t fromincl, uint8_t toincl) {

	for (uint8_t i = fromincl; i <= toincl; i++) {
		// simple, brute force...
		channel_t *chan = channel_find(i);
		if (chan != NULL) {
			// force
			channel_close_int(chan, 1);
		}
	}
}

/*
 * receives a byte
 *
 * data and iseof are out parameters for the received data. 
 * eof is set when an EOF has been received, cleared otherwise
 * err is set to the error number when applicable.
 *
 * flags is input; currently when <> 0 then the call is synchronous
 * i.e. it returns when data is available or on error. When == 0 then
 * returns immediately (after sending FS_READ in the background)
 *
 * returns 0 on ok, -1 on error
 */
int8_t channel_get(channel_t *chan, uint8_t *data, uint8_t *iseof, int8_t *err, uint8_t preload) {
#ifdef DEBUG_BUS
//debug_printf("rx: chan=%p, channo=%d\n", channel, chan->channel_no);
#endif
	// provider shortcut where applicable
	if (chan->endpoint->provider->channel_get != NULL) {
		return chan->endpoint->provider->channel_get(chan->endpoint->provdata,
				chan->channel_no, data, iseof, err, preload);
	}

	*iseof = 0;
	*err = CBM_ERROR_OK;

        // first fillup the buffers
        if (channel_preloadp(chan) < 0) {

        	// no data available

#ifdef DEBUG_BUS
                debug_printf("preload on chan %p (%d) gives no data (st=%04x)", chan,
                                chan->channel_no, st);
#endif
                *err = channel_last_pull_error(chan);

		return -1;
        } else {

                *data = channel_current_byte(chan, iseof);

                if (!(preload & GET_PRELOAD)) {
        		// make sure the next call does have a data byte
                        if (!channel_next(chan, preload & GET_SYNC)) {
                       		// no further data on channel available
                                *err = channel_last_pull_error(chan);
                        }
                }
        }
	return 0;
}


