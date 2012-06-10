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

#include <stdio.h>
#include <inttypes.h>
#include <util/delay.h>

#include "packet.h"
#include "channel.h"
#include "provider.h"

//#include "led.h"

#define	MAX_CHANNELS	8		// number of maximum open channels

volatile channel_t channels[MAX_CHANNELS];

static void channel_close_int(volatile channel_t *chan, uint8_t force);

void channel_init(void) {
	for (int8_t i = MAX_CHANNELS-1; i>= 0; i--) {
		channel_close_int(&channels[i], 1);
	}
}

static void _pull_callback(int8_t channel_no, int8_t errorno) {
	volatile channel_t *p = channel_find(channel_no);
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
}

/**
 * pull in a buffer from the server
 */
static void channel_pull(volatile channel_t *c, uint8_t slot) {
	packet_t *p = &c->buf[slot];

	// prepare to write a buffer with length 0
	packet_set_filled(p, c->channel_no, FS_READ, 0);

	// not irq-protected, as exlusive state conditions
	if (c->pull_state == PULL_OPEN) {
		c->pull_state = PULL_PRELOAD;
	} else
	if (c->pull_state == PULL_ONEREAD) {
		c->pull_state = PULL_PULL2ND;
	}

	c->provider->submit_call(c->channel_no, p, p, _pull_callback);
//led_on();
//debug_puts("pull_state is "); debug_puthex(c->pull_state); debug_putcrlf();
}

/*
 * open and reserve a channel for the given chan channel number
 * returns negative on error, 0 on ok
 *
 * writetype is either 0 for read only, 1 for write, (as seen from ieee device)
 */
int8_t channel_open(int8_t chan, uint8_t writetype, provider_t *prov, int8_t (*dirconv)(volatile packet_t *)) {
	for (int8_t i = MAX_CHANNELS-1; i>= 0; i--) {
		if (channels[i].channel_no < 0) {
			channels[i].channel_no = chan;
			channels[i].current = -1;
			channels[i].writetype = writetype;
			channels[i].provider = prov;
			channels[i].directory_converter = dirconv;
			// note: we should not channel_pull() here, as file open has not yet even been sent
			for (uint8_t j = 0; j < 2; j++) {
				packet_reset(&channels[i].buf[j]);
			}
			return 0;	
		}
	}
	return -1;
}

volatile channel_t* channel_find(int8_t chan) {
	for (int8_t i = MAX_CHANNELS-1; i>= 0; i--) {
		if (channels[i].channel_no == chan) {
			return &channels[i];	
		}
	}
	return NULL;
}

static void channel_preload_int(volatile channel_t *chan, uint8_t wait) {
	if (chan->pull_state == PULL_OPEN) {
		chan->current = 0;
		channel_pull(chan, 0);
	}
	if (wait) {
		while (chan->pull_state == PULL_PRELOAD) {
			_delay_ms(1);
		}
	}
	if (chan->pull_state == PULL_ONECONV) {
		if (chan->directory_converter != NULL) {
debug_printf(">>1: %p, p=%p\n",chan->directory_converter, &chan->buf[chan->current]);
debug_printf(">>1: b=%p\n", packet_get_buffer(&chan->buf[chan->current]));
			chan->directory_converter(&chan->buf[chan->current]);
		}
		chan->pull_state = PULL_ONEREAD;
	}
	if (chan->pull_state == PULL_TWOCONV) {
		if (chan->directory_converter != NULL) {
debug_printf(">>2: %p, p=%p\n",chan->directory_converter, &chan->buf[1-chan->current]);
debug_printf(">>2: b=%p\n", packet_get_buffer(&chan->buf[1-chan->current]));
			chan->directory_converter(&chan->buf[1-chan->current]);
		}
		chan->pull_state = PULL_TWOREAD;
	}
//led_on();
}

/**
 * pre-load data for a read-only channel
 */
void channel_preload(int8_t chan) {
	volatile channel_t *channel = channel_find(chan);
	if (channel != NULL) {
		channel_preload_int(channel, 1);	
	}
}

char channel_current_byte(volatile channel_t *chan) {
	channel_preload_int(chan, 1);
        return packet_peek_data(&chan->buf[chan->current]);
}

/**
 * return true (non-zero) when there is still a byte available in the buffer
 */
uint8_t channel_next(volatile channel_t *chan) {

	// make sure we do have something at least
	channel_preload_int(chan, 1);

//debug_puts("_next: "); debug_puthex(chan->pull_state); debug_putcrlf();

	// this is an optimization:
	// pull in the "other" buffer in the background
	uint8_t other = 1-chan->current;
	//if (packet_is_done(&chan->buf[other]) && (!packet_is_eoi(&chan->buf[chan->current]))) {
	if ((chan->pull_state == PULL_ONEREAD) && (!packet_is_eof(&chan->buf[chan->current]))) {
		// if the other packet is free ("done"), and the current packet is not the last one ("eoi")
		// We should only do this on "standard" files though, not relative or others
		channel_pull(chan, other);
	}

	// return the actual value requested
	return packet_next(&chan->buf[chan->current]);
}

/*
 * return whether the current packet is the last one
 * called when the packet is empty
 */
uint8_t channel_has_more(volatile channel_t *chan) {
	// make sure at least one packet is there
	channel_preload_int(chan, 1);
	return !packet_is_eof(&chan->buf[chan->current]);
}

static void channel_close_int(volatile channel_t *chan, uint8_t force) {
	// do nothing but mostly invalidate channel_no,
	// as we do not currently support FS_OPEN_RW yet, which
	// would require an explicit close on the server
	chan->channel_no = -1;
	chan->pull_state = PULL_OPEN;
	packet_init(&chan->buf[0], DATA_BUFLEN, chan->data[0]);
	packet_init(&chan->buf[1], DATA_BUFLEN, chan->data[1]);
}

void channel_close(int8_t secondary_address) {
	volatile channel_t *chan = channel_find(secondary_address);
	channel_close_int(chan, 0);
}

/**
 * should be called ONLY(!) when channel_next() false has indicated that
 * the current buffer is empty, but channel_has_more() true has indicated
 * that the current buffer is not the last one
 */
volatile channel_t* channel_refill(volatile channel_t *chan) {
	// buf->refill();
	// buf = find_buffer(...)
	//
	uint8_t other = 1-chan->current;
	if (!packet_is_eof(&chan->buf[chan->current])) {
		// current packet is not last one
		// other packet should have been pulled in channel_next()
		// so it is either empty (request, or FS_EOF), or has data
		volatile packet_t *opacket = &chan->buf[other];

		// wait until available	
		while(chan->pull_state == PULL_PULL2ND) {
			_delay_ms(1);
		}
		if (chan->pull_state == PULL_TWOCONV) {
			if (chan->directory_converter != NULL) {
debug_printf(">>3: %p, p=%p\n",chan->directory_converter, &chan->buf[1-chan->current]);
debug_printf(">>3: b=%p\n", packet_get_buffer(&chan->buf[1-chan->current]));
				chan->directory_converter(&chan->buf[1-chan->current]);
			}
			chan->pull_state = PULL_TWOREAD;
		}
		
		if (packet_has_data(opacket)) {
			// switch packets
			chan->current = other;

			chan->pull_state = PULL_ONEREAD;

			channel_pull(chan, 1-other);
			return chan;
		}
	}

	// no more data - close it
	channel_close_int(chan, 0);
	return NULL;
}


//static inline channel_t* channel_put(channel_t *chan, char c, int forceflush) {
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
//}

//static inline void channel_status_set(uint8_t *error_buffer, int len) {
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
//}

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

