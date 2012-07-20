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

#include "version.h"
#include "packet.h"
#include "provider.h"
#include "wireformat.h"
#include "serial.h"
#include "uarthw.h"
#include "petscii.h"

#include "debug.h"
#include "led.h"


#undef DEBUG_SERIAL

/***********************************************************************************
 * UART stuff
 *
 * Note that using the endpoint->provdata information given to the submit methods
 * it would even be possible to use different UARTs at the same time.
 */

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
 * callback is called from interrupt context when the response has been
 * received
 */
void serial_submit_call(void *epdata, int8_t channelno, packet_t *txbuf, packet_t *rxbuf,
                void (*callback)(int8_t channelno, int8_t errnum));

static int8_t directory_converter(packet_t *p);
static int8_t to_provider(packet_t *p);

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
	void		(*callback)(int8_t channelno, int8_t errnum);
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
 * conversion routines between wire format and packet format
 */

/*
 * helper for conversion of ASCII to PETSCII
 */

static uint8_t *append(uint8_t *outp, const char *to_append) {
	while(*to_append != 0) {
		*outp = ascii_to_petscii(*to_append);
		outp++;
		to_append++;
	}
	*outp = 0;
	outp++;
	return outp;
}

/*
 * each packet from the UART contains one directory entry. 
 * The format is defined as FS_DIR_* offset definitions
 *
 * This method converts it into a Commodore BASIC line definition
 *
 * Note that it currently relies on an entry FS_DIR_MOD_NAME as first
 * and FS_DIR_MOD_FRE as last entry, to add the PET BASIC header and
 * footer!
 */
static uint8_t out[64];

static int8_t directory_converter(packet_t *p) {
	uint8_t *inp = NULL;
	uint8_t *outp = &(out[0]);

	if (p == NULL) {
		debug_puts("P IS NULL!");
		return -1;
	}

	inp = packet_get_buffer(p);
	uint8_t type = inp[FS_DIR_MODE];

	//packet_update_wp(p, 2);

	if (type == FS_DIR_MOD_NAM) {
		*outp = 1; outp++;	// load address low
		*outp = 4; outp++;	// load address high
	}

	*outp = 1; outp++;		// link address low; will be overwritten on LOAD
	*outp = 1; outp++;		// link address high

	uint16_t lineno = 0;
	// line number, derived from file length
	if (inp[FS_DIR_LEN+3] != 0 
		|| inp[FS_DIR_LEN+2] > 0xf9
		|| (inp[FS_DIR_LEN+2] == 0xf9 && inp[FS_DIR_LEN+1] == 0xff && inp[FS_DIR_LEN] != 0)) {
		// more than limit of 63999 blocks
		lineno = 63999;
	} else {
		lineno = inp[FS_DIR_LEN+1] | (inp[FS_DIR_LEN+2] << 8);
		if (inp[FS_DIR_LEN] != 0) {
			lineno++;
		}
	}
	*outp = lineno & 255; outp++;
	*outp = (lineno>>8) & 255; outp++;

	//snprintf(outp, 5, "%hd", (unsigned short)lineno);
	//outp++;
	if (lineno < 10) { *outp = ' '; outp++; }
	if (type == FS_DIR_MOD_NAM) {
		*outp = 0x12;	// reverse for disk name
		outp++;
	} else {
		if (type != FS_DIR_MOD_FRE) {
			if (lineno < 100) { *outp = ' '; outp++; }
			if (lineno < 1000) { *outp = ' '; outp++; }
			if (lineno < 10000) { *outp = ' '; outp++; }
		}
	}

	if (type != FS_DIR_MOD_FRE) {
		*outp = '"'; outp++;
		uint8_t i = FS_DIR_NAME;
		// note the check i<16 - this is buffer overflow protection
		// file names longer than 16 chars are not displayed
		while ((inp[i] != 0) && (i < (FS_DIR_NAME + 16))) {
			*outp = ascii_to_petscii(inp[i]);
			outp++;
			i++;
		}
		// note: not counted in i
		*outp = '"'; outp++;
	
		// fill up with spaces, at least one space behind filename
		while (i < FS_DIR_NAME + 16 + 1) {
			*outp = ' '; outp++;
			i++;
		}
	}

	// add file type
	if (type == FS_DIR_MOD_NAM) {
		// file name entry
		outp = append(outp, SW_NAME_LOWER);
		//strcpy(outp, SW_NAME_LOWER);
		//outp += strlen(SW_NAME_LOWER)+1;	// includes ending 0-byte
	} else
	if (type == FS_DIR_MOD_DIR) {
		outp = append(outp, "dir");
		//strcpy(outp, "dir");
		//outp += 4;	// includes ending 0-byte
	} else
	if (type == FS_DIR_MOD_FIL) {
		outp = append(outp, "prg");
		//strcpy(outp, "prg");
		//outp += 4;	// includes ending 0-byte
	} else
	if (type == FS_DIR_MOD_FRE) {
		outp = append(outp, "blocks free");
		//strcpy(outp, "bytes free");
		//outp += 11;	// includes ending 0-byte

		*outp = 0; outp++;	// BASIC end marker (zero link address)
		*outp = 0; outp++;	// BASIC end marker (zero link address)
	}

	uint8_t len = outp - out;
	if (len > packet_get_capacity(p)) {
		debug_puts("CONVERSION NOT POSSIBLE!"); debug_puthex(len); debug_putcrlf();
		return -1;	// conversion not possible
	}

#if DEBUG_SERIAL
	debug_puts("CONVERTED TO: LEN="); debug_puthex(len);
	for (uint8_t j = 0; j < len; j++) {
		debug_putc(' '); debug_puthex(out[j]);
	} 
	debug_putcrlf();
#endif
	// this should probably be combined
	memcpy(packet_get_buffer(p), &out, len);
	packet_update_wp(p, len);

	return 0;
}

/**
 * convert PETSCII names to ASCII names
 */
static int8_t to_provider(packet_t *p) {
	uint8_t *buf = packet_get_buffer(p);
	uint8_t len = packet_get_contentlen(p);
//debug_printf("CONVERT: len=%d, b=%s\n", len, buf);
	while (len > 0) {
//debug_puts("CONVERT: "); 
//debug_putc(*buf); //debug_puthex(*buf);debug_puts("->");
		*buf = petscii_to_ascii(*buf);
//debug_putc(*buf);debug_puthex(*buf);debug_putcrlf();
		buf++;
		len--;
	}
	return 0;
}

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
		if (rxdata == FS_REPLY || rxdata == FS_WRITE || rxdata == FS_EOF) {
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
					rxstate = RX_DATA;
				}
				break;
			}
		}
		// well, RX_IGNORE should not happen, but we have no means of telling anyone here
		if (current_data_left == 0) {
			// we are actually already done. do callback and set status to idle
			rx_channels[current_channelpos].callback(current_channelno, 
					(rxstate == RX_IGNORE) ? -1 : 0);
			rx_channels[current_channelpos].channelno = -1;
			rxstate = RX_IDLE;
		}
		break;
	case RX_DATA:
		packet_write_char(current_rxpacket, rxdata);
		// fallthrough
	case RX_IGNORE:
		current_data_left --;
//if (packet_get_contentlen(current_rxpacket) > 1) led_on();
		if (current_data_left <= 0) {
			rx_channels[current_channelpos].callback(current_channelno, 
					(rxstate == RX_IGNORE) ? -1 : 0);
			rx_channels[current_channelpos].channelno = -1;
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
	int16_t data = uarthw_receive();
	while (data >= 0) {
		push_data_to_packet(0xff & data);
		// try next byte
		data = uarthw_receive();
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
 * (while data is transferred in the background)
 *
 * Note: submitter must check buf_is_empty() for true or buf_wait_free() 
 * before reuse or freeing the memory!
 */
void serial_submit(void *epdata, packet_t *buf) {

	// wait for slot free
	while (slots_used >= (NUMBER_OF_SLOTS-1)) {
		serial_delay();
	}


	// protect slot* by disabling the interrupt
	//cli();	 - as slots are not handled in irq anymore
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
	// enable interrupts again
	//sei();	 - as slots are not handled in irq anymore
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
 * callback is called from interrupt context when the response has been
 * received
 */
void serial_submit_call(void *epdata, int8_t channelno, packet_t *txbuf, packet_t *rxbuf, 
		void (*callback)(int8_t channelno, int8_t errnum)) {

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
			if (rx_channels[i].channelno < 0) {
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


