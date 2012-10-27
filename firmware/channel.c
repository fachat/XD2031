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

#define	MAX_CHANNELS	8		// number of maximum open channels

channel_t channels[MAX_CHANNELS];

static uint8_t _push_callback(int8_t channelno, int8_t errnum);
static void channel_close_int(channel_t *chan, uint8_t force);

void channel_init(void) {
	for (int8_t i = MAX_CHANNELS-1; i>= 0; i--) {
		channel_close_int(&channels[i], 1);
	}
}

static uint8_t _pull_callback(int8_t channel_no, int8_t errorno) {
	channel_t *p = channel_find(channel_no);
	if (p != NULL) {
		p->last_pull_errorno = errorno;

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

/**
 * pull in a buffer from the server
 */
static void channel_pull(channel_t *c, uint8_t slot, uint8_t options) {
	packet_t *p = &c->buf[slot];

	//debug_printf("pull: chan=%p, channo=%d\n", c, c->channel_no);

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
int8_t channel_open(int8_t chan, uint8_t writetype, endpoint_t *prov, int8_t (*dirconv)(packet_t *, uint8_t drive), 
	uint8_t drive) {

//debug_printf("channel_open: chan=%d\n", chan);

	for (int8_t i = MAX_CHANNELS-1; i>= 0; i--) {
		if (channels[i].channel_no < 0) {
			channels[i].channel_no = chan;
			channels[i].current = 0;
			channels[i].writetype = writetype;
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

channel_t* channel_find(int8_t chan) {
	for (int8_t i = MAX_CHANNELS-1; i>= 0; i--) {
		if (channels[i].channel_no == chan) {
			return &channels[i];	
		}
	}
	return NULL;
}

static void channel_preload_int(channel_t *chan, uint8_t wait) {

	// TODO:fix for read/write
	if (chan->writetype != WTYPE_READONLY) return;

	do {
	    if (chan->pull_state == PULL_OPEN) {
		chan->current = 0;
		channel_pull(chan, 0, GET_SYNC);
	    }
	    if (wait) {
		while (chan->pull_state == PULL_PRELOAD) {
			delayms(1);
		}
	    }
	    if (chan->pull_state == PULL_ONECONV) {
		packet_t *curpack = &(chan->buf[chan->current]);
		if ((!packet_has_data(curpack)) && (!packet_is_last(curpack))) {
			// zero length packet received
			chan->pull_state = PULL_OPEN;
		} else {
			if (chan->directory_converter != NULL) {
				//debug_printf(">>1: %p, p=%p\n",chan->directory_converter, &chan->buf[chan->current]);
				//debug_printf(">>1: b=%p\n", packet_get_buffer(&chan->buf[chan->current]));
				chan->directory_converter(&chan->buf[chan->current], chan->drive);
			}
			chan->pull_state = PULL_ONEREAD;
		}
	    }
	    if (chan->pull_state == PULL_TWOCONV) {
		packet_t *opack = &(chan->buf[1-chan->current]);
		if ((!packet_has_data(opack)) && (!packet_is_last(opack))) {
			// zero length packet received
			chan->pull_state = PULL_ONEREAD;
		} else {
			if (chan->directory_converter != NULL) {
				//debug_printf(">>2: %p, p=%p\n",chan->directory_converter, &chan->buf[1-chan->current]);
				//debug_printf(">>2: b=%p\n", packet_get_buffer(&chan->buf[1-chan->current]));
				chan->directory_converter(&chan->buf[1-chan->current], chan->drive);
			}
			chan->pull_state = PULL_TWOREAD;
		}
	    }
	}
	while (chan->pull_state == PULL_OPEN);
//led_on();
}

/**
 * pre-load data for a read-only channel
 */
void channel_preload(int8_t chan) {
	channel_t *channel = channel_find(chan);

	if (channel != NULL) {
		channel_preload_int(channel, 1);	
	}
}

void channel_preloadp(channel_t *chan) {
	channel_preload_int(chan, 1);
}

char channel_current_byte(channel_t *chan) {
	channel_preload_int(chan, 1);
        return packet_peek_data(&chan->buf[chan->current]);
}

/**
 * return true (non-zero) when there is still a byte available in the buffer
 */
uint8_t channel_next(channel_t *chan, uint8_t options) {

	// make sure we do have something at least
	channel_preload_int(chan, 1);

//debug_puts("_next: "); debug_puthex(chan->pull_state); debug_putcrlf();

	// this is an optimization:
	// pull in the "other" buffer in the background
	uint8_t other = 1-chan->current;
	//if (packet_is_done(&chan->buf[other]) && (!packet_is_eoi(&chan->buf[chan->current]))) {
	if ((chan->pull_state == PULL_ONEREAD) && (!packet_is_last(&chan->buf[chan->current]))) {
		// if the other packet is free ("done"), and the current packet is not the last one ("eoi")
		// We should only do this on "standard" files though, not relative or others
		channel_pull(chan, other, options);
	}

	// return the actual value requested
	return packet_next(&chan->buf[chan->current]);
}

/*
 * return whether the current packet is the last one
 * called when the packet is empty
 */
uint8_t channel_has_more(channel_t *chan) {
	// make sure at least one packet is there
	channel_preload_int(chan, 1);
	return !packet_is_eof(&chan->buf[chan->current]);
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
	channel_t *chan = channel_find(channel_no);

	debug_printf("channel_close(%p -> %d), push_state=%d\n", chan, channel_no, chan->push_state); debug_flush();

	if (chan != NULL) {
		if (chan->push_state != PUSH_OPEN) {
			// if it's not PUSH_FILLONE, then it is in the process
			// of being pushed
			while (chan->push_state != PUSH_FILLONE) {
				delayms(1);
			}

			packet_t *curpack = &chan->buf[chan->current];
			int l = packet_get_contentlen(curpack);

			// even if l==0, send an EOF packet to close the file
		        packet_set_filled(curpack, channel_no, FS_EOF, l);

			endpoint_t *endpoint = chan->endpoint;
        	        endpoint->provider->submit_call(endpoint->provdata, 
				channel_no, curpack, curpack, _push_callback);

               		// wait until the packet has been sent and been responded to
      	 		while (chan->push_state == PUSH_FILLONE) {
                	       	delayms(1);
			}	
		}
		channel_close_int(chan, 0);
	}
}

/**
 * should be called ONLY(!) when channel_next() false has indicated that
 * the current buffer is empty, but channel_has_more() true has indicated
 * that the current buffer is not the last one
 */
channel_t* channel_refill(channel_t *chan, uint8_t options) {
	// buf->refill();
	// buf = find_buffer(...)
	//
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
				chan->directory_converter(&chan->buf[1-chan->current], chan->drive);
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

	// no more data - close it
	channel_close_int(chan, 0);
	return NULL;
}

static uint8_t _push_callback(int8_t channelno, int8_t errnum) {
        channel_t *p = channel_find(channelno);
        if (p != NULL) {
                p->last_push_errorno = errnum;

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
	

channel_t* channel_put(channel_t *chan, char c, uint8_t forceflush) {

	if (chan->push_state == PUSH_OPEN) {
		chan->push_state = PUSH_FILLONE;
	}

	uint8_t channo = chan->channel_no;
	packet_t *curpack = &chan->buf[chan->current];

	packet_write_char(curpack, (uint8_t) c);

	if (packet_is_full(curpack) || (forceflush & PUT_FLUSH)) {
		packet_set_filled(curpack, channo, FS_WRITE, packet_get_contentlen(curpack));

		// wait until the other packet has been replied to,
		// i.e. it has been sent, the buffer is free again 
		// which we need for the next channel_put
		while (chan->push_state == PUSH_FILLTWO) {
			delayms(1);
		}

		// note that we are pushing one and are now filling the second
		// change that before pushing, as callback might already be done during push
		chan->push_state = PUSH_FILLTWO;

		// use same packet as rx/tx buffer
		endpoint_t *endpoint = chan->endpoint;
		endpoint->provider->submit_call(endpoint->provdata, 
			channo, curpack, curpack, _push_callback);

		// switch
		chan->current = 1-chan->current;
		packet_reset(&chan->buf[chan->current], channo);

		if (forceflush & PUT_SYNC) {
			// we are forced to wait for the reply, e.g. from the IEC code
			// as the interrupt block prevents us from really receiving all
			// replies - so we have to make sure we really got it

			// wait until the other packet has been replied to,
			// i.e. it has been sent, the buffer is free again 
			// which we need for the next channel_put
			while (chan->push_state == PUSH_FILLTWO) {
				delayms(1);
				main_delay();
			}

		}
	}
	return chan;
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

