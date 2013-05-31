/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2012 Andre Fachat

    Derived from:
    OS/A65 Version 1.3.12
    Multitasking Operating System for 6502 Computers
    Copyright (C) 1989-1997 Andre Fachat

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

****************************************************************************/


#include <stdio.h>
#include <string.h>

#include "packet.h"
#include "provider.h"
#include "wireformat.h"
#include "serial.h"
#include "uarthw.h"
#include "dirconverter.h"

#include "debug.h"
#include "led.h"


#undef DEBUG_SERIAL

/***********************************************************************************
 * UART stuff
 *
 * Note that using the endpoint->provdata information given to the submit methods
 * it would even be possible to use different UARTs at the same time.
 */

static uint8_t serial_lock;

/**
 * submit the contents of a buffer to the UART
 * If buffer slot is available, return immediately.
 * Otherwise wait until slot is available
 *
 * Note: submitter must check packet_is_done() or friends
 * before reuse or freeing the memory!
 */
void serial_submit(void *epdata, packet_t *buf);

/*****************************************************************************
 * submit a channel rpc call to the UART
 * If buffer slot is available, return immediately.
 * Otherwise wait until slot is available, then return
 * (while data is transferred in the background)
 *
 * Note: submitter must check buf_is_empty() for true or buf_wait_free()
 * before reuse or freeing the memory!
 *
 * callback is called from delay context when the response has been
 * received
 */
void serial_submit_call(void *epdata, int8_t channelno, packet_t *txbuf, packet_t *rxbuf,
                uint8_t (*callback)(int8_t channelno, int8_t errnum, packet_t *packet));

// dummy
static void *prov_assign(const char *name) {
	return NULL;
}

// dummy
static void prov_free(void *epdata) {
	return;
}

provider_t serial_provider  = {
	prov_assign,
	prov_free,
        serial_submit,
        serial_submit_call,
	directory_converter,
	to_provider
};

#define	NUMBER_OF_SLOTS		4

// ----------------------------------
// send variables
static packet_t		*slots[NUMBER_OF_SLOTS];
static uint8_t		slots_used = 0;

static int8_t		txstate;

#define	TX_TYPE		0
#define	TX_LEN		1
#define	TX_CHANNEL	2
#define	TX_DATA		3
#define	TX_IDLE		4

// ----------------------------------
// receive variables
static struct {
	int8_t		channelno;	// -1 is unused
	packet_t	*rxpacket;
	uint8_t		(*callback)(int8_t channelno, int8_t errnum, packet_t *packet);
} rx_channels[NUMBER_OF_SLOTS];

#define	RX_IDLE		0
#define	RX_LEN		1	// got reply, read length next
#define	RX_CHANNELNO	2	// got length, read channelno next
#define	RX_DATA		3	// read the packet data from the wire
#define	RX_IGNORE	4	// pull the packet data from the wire, but ignore it

static int8_t			rxstate;
static int8_t			current_channelno;
static int8_t			current_channelpos;
static packet_t			*current_rxpacket;
static int8_t			current_data_left;
static int8_t			current_is_eoi;

/*****************************************************************************
 * communication with the low level interrupt routines
 */

// returns the next byte to send - first the packet type, then the data length
// then the channel and then the data (len bytes).
//
// Used by UART code to include type and length in packet
//
// returns -1 if empty
static int16_t read_char_from_packet(packet_t *buf) {
        int16_t rv = -1;
        switch (txstate) {
        case TX_TYPE:
                txstate = TX_LEN;
                rv = 0xff & packet_get_type(buf);
//if (rv == FS_TERM && packet_get_chan(buf) == FSFD_SETOPT) led_on();
                break;
        case TX_LEN:
                txstate = TX_CHANNEL;
                rv = (0xff & packet_get_contentlen(buf)) + 3;	// plus 2 to adhere to packet wire format
		break;
	case TX_CHANNEL:
		rv = 0xff & packet_get_chan(buf);
                if (packet_get_contentlen(buf) == 0) {
                        txstate = TX_IDLE;
                } else {
                        txstate = TX_DATA;
                }
                break;
        case TX_DATA:
                rv = 0xff & packet_read_char(buf);
                if (!packet_has_data(buf)) {
                        txstate = TX_IDLE;
                }
                break;
        default:
                break;
        }
        return rv;
}

/*
 * TODO: make that a ring buffer of slots!
 */
static void advance_slots() {
	uint8_t i = 0;
	slots_used--;
	while (i < slots_used) {
		slots[i] = slots[i+1];
		i++;
	}
	txstate = TX_TYPE;
}

static void send(void) {

	while (slots_used > 0 && uarthw_can_send()) {
		// read data
		int16_t data = read_char_from_packet(slots[0]);

		if (data >= 0) {
			// send it
			uarthw_send(data);
			//led_on();
		} else {
			// packet empty, so get next slot
			advance_slots();
		}
	}	
}


/**
 * interrupt for received data
 */
static void push_data_to_packet(int8_t rxdata)
{
	switch(rxstate) {
	case RX_IDLE:
		// no current packet
		if (rxdata == FS_SYNC) {
			// sync received
			// mirror the sync back
			// first flush all packets
			while (slots_used > 0) {
				send();
			}
			// can we send?
			while (!uarthw_can_send());
			// yes, send sync
			uarthw_send(FS_SYNC);
		} else
		if (rxdata == FS_REPLY || rxdata == FS_WRITE || rxdata == FS_EOF 
			|| rxdata == FS_SETOPT || rxdata == FS_RESET) {
			// note EOI flag
			current_is_eoi = rxdata;
			// start a reply handling
			rxstate = RX_LEN;
		} else {
			//led_toggle();
		}
		break;
	case RX_LEN:
		current_data_left = rxdata - 3;
		rxstate = RX_CHANNELNO;
		break;
	case RX_CHANNELNO:
		current_channelno = rxdata;
		rxstate = RX_IGNORE;	// fallback
		// find the current receive buffer

		for (uint8_t i = 0; i < NUMBER_OF_SLOTS; i++) {
			if (rx_channels[i].channelno == current_channelno) {
//if (current_is_eoi == FS_EOF && current_data_left == 0) led_toggle();
				current_channelpos = i;
				current_rxpacket = rx_channels[current_channelpos].rxpacket;
				if (packet_set_write(current_rxpacket, current_channelno,
						current_is_eoi, current_data_left) >= 0) {
//if (current_is_eoi == FS_SETOPT && current_channelno == FSFD_SETOPT) led_on();
					rxstate = RX_DATA;
				}
				break;
			}
		}
		// well, RX_IGNORE should not happen, but we have no means of telling anyone here
		if (current_data_left == 0) {
			// we are actually already done. do callback and set status to idle
			// prohibit receiving just in case (we reuse the rx buffer e.g. 
			// in X option)
			serial_lock = 1;
			if ((rxstate == RX_DATA) 
				&& (rx_channels[current_channelpos].callback(current_channelno, 
					(rxstate == RX_IGNORE) ? -1 : 0, 
					(rxstate == RX_IGNORE) ? NULL : current_rxpacket) == 0)) {
				rx_channels[current_channelpos].channelno = -1;
			}
			serial_lock = 0;
			rxstate = RX_IDLE;
		}
		break;
	case RX_DATA:
		packet_write_char(current_rxpacket, rxdata);
		current_data_left --;
		if (current_data_left <= 0) {
			// prohibit receiving just in case (we reuse the rx buffer e.g. 
			// in X option)
			serial_lock = 1;
			if (rx_channels[current_channelpos].callback(current_channelno, 
					(rxstate == RX_IGNORE) ? -1 : 0, 
					(rxstate == RX_IGNORE) ? NULL : current_rxpacket) == 0) {
				rx_channels[current_channelpos].channelno = -1;
			}
			serial_lock = 0;
			rxstate = RX_IDLE;
		}
		break;
	case RX_IGNORE:
		current_data_left --;
		if (current_data_left <= 0) {
			rxstate = RX_IDLE;
		}
		break;
	default:
		break;
	}
}

/*****************************************************************************
 * try to send data to the uart ring buffer, or try to receive something
 */
void serial_delay() {
	// can we receive?
	if (!serial_lock) {
		int16_t data = uarthw_receive();
		while (data >= 0) {
			push_data_to_packet(0xff & data);
			// try next byte
			data = uarthw_receive();
		}
	}
	// try to send
	send();
}

/*****************************************************************************
 * wait until everything has been flushed out - for debugging, to make
 * sure all messages have been sent
 */
void serial_flush() {
	while (slots_used > 0) {
		serial_delay();
	}
}

/*****************************************************************************
 * submit the contents of a buffer to the UART
 * If buffer slot is available, return immediately.
 * Otherwise wait until slot is available, then return
 * (while data is transferred in the background (=delay() calls))
 *
 * Note: submitter must check buf_is_empty() for true or buf_wait_free() 
 * before reuse or freeing the memory!
 */
void serial_submit(void *epdata, packet_t *buf) {

	// wait for slot free
	while (slots_used >= (NUMBER_OF_SLOTS-1)) {
		serial_delay();
	}


	// note: slots_used can only decrease until here, as this is the
	// only place to increase it, so there is no race from the while()
	// above to setting it here.	
	slots[slots_used] = buf;
	slots_used++;
	if (slots_used == 1) {
		// no packet before, so need to start sending
		txstate = TX_TYPE;
		send();
		//led_on();
	}
}

/*****************************************************************************
 * submit a channel rpc call to the UART
 * If buffer slot is available, return immediately.
 * Otherwise wait until slot is available, then return
 * (while data is transferred in the background)
 *
 * Note: submitter must check buf_is_empty() for true or buf_wait_free() 
 * before reuse or freeing the memory!
 *
 * callback is called from delay() context when the response has been
 * received
 */
void serial_submit_call(void *epdata, int8_t channelno, packet_t *txbuf, packet_t *rxbuf, 
		uint8_t (*callback)(int8_t channelno, int8_t errnum, packet_t *packet)) {

	if (channelno < 0) {
		debug_printf("!!!! submit with channelno=%d\n", channelno);
	}
	if (txbuf->chan < 0) {
		debug_printf("!!!! submit with packet->chan=%d\n", txbuf->chan);
	}

	// check rx slot
	// wait / loop until receive buffer is being freed by interrupt routine
	int8_t channelpos = -1;
	while (channelpos < 0) {
		for (uint8_t i = 0; i < NUMBER_OF_SLOTS; i++) {
			// note: take either a free one or overwrite an existing one
			// the latter case is only used for rtconfig_pullconfig()
			// sending a new request
			if (rx_channels[i].channelno < 0
				|| rx_channels[i].channelno == channelno) {
				channelpos = i;
				break;
			}
		}
		serial_delay();
	}

	rx_channels[channelpos].channelno = channelno;
	rx_channels[channelpos].rxpacket = rxbuf;
	rx_channels[channelpos].callback = callback;

	// send request
	serial_submit(epdata, txbuf);
}

/*****************************************************************************
* initialize the UART code
*/
provider_t *serial_init() {
	slots_used = 0;
	serial_lock = 0;

	for (int8_t i = NUMBER_OF_SLOTS-1; i >= 0; i--) {
		rx_channels[i].channelno = -1;
	}

	rxstate = RX_IDLE;
	txstate = TX_IDLE;

	// we should not register (at least for now, as when we are being
	// selected, the remote end does not know about it currently, as we
	// do not notify it. So open requests will be returned with an error
	//provider_register("FS", &serial_provider);

	return &serial_provider;
}

/*****************************************************************************
 * sync with the server
 */
void serial_sync() {

	// sync with the pc server
	// by sending 128 FS_SYNC bytes
	for (uint8_t cnt = 128; cnt > 0; cnt--) {
		while (!uarthw_can_send());
		uarthw_send(FS_SYNC);
	}
}


