/****************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2012 Andre Fachat

    Inspired by uart.c from XS-1541, but rewritten in the end.

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
 * UART backend provider for the channels
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <util/delay.h>

#include "version.h"
#include "compat.h"
#include "packet.h"
#include "uart2.h"
#include "fscmd.h"
#include "oa1fs.h"
#include "provider.h"
#include "petscii.h"

#include "led.h"

/***********************************************************************************
 * UART stuff
 */

static int8_t directory_converter(volatile packet_t *p);
static int8_t to_provider(packet_t *p);

provider_t uart_provider  = {
        uart_submit,
        uart_submit_call,
	directory_converter,
	to_provider
};

#define	NUMBER_OF_SLOTS		4

// ----------------------------------
// send variables
volatile static packet_t	*slots[NUMBER_OF_SLOTS];
volatile static uint8_t		slots_used = 0;

volatile static int8_t		txstate;

#define	TX_TYPE		0
#define	TX_LEN		1
#define	TX_CHANNEL	2
#define	TX_DATA		3
#define	TX_IDLE		4

// ----------------------------------
// receive variables
volatile static struct {
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
static uint8_t			current_data_left;
static uint8_t			current_is_eoi;

/*****************************************************************************
 * conversion routines between wire format and packet format
 */

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

static int8_t directory_converter(volatile packet_t *p) {
	volatile uint8_t *inp = NULL;
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

//return -1;

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
//return -1;

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

//return -1;
	// add file type
	if (type == FS_DIR_MOD_NAM) {
		// file name entry
		strcpy(outp, SW_NAME_LOWER);
		outp += strlen(SW_NAME_LOWER)+1;	// includes ending 0-byte
	} else
	if (type == FS_DIR_MOD_DIR) {
		strcpy(outp, "dir");
		outp += 4;	// includes ending 0-byte
	} else
	if (type == FS_DIR_MOD_FIL) {
		strcpy(outp, "prg");
		outp += 4;	// includes ending 0-byte
	} else
	if (type == FS_DIR_MOD_FRE) {
		strcpy(outp, "bytes free");
		outp += 11;	// includes ending 0-byte

		*outp = 0; outp++;	// BASIC end marker (zero link address)
		*outp = 0; outp++;	// BASIC end marker (zero link address)
	}

	uint8_t len = outp - out;
	if (len > packet_get_capacity(p)) {
		debug_puts("CONVERSION NOT POSSIBLE!"); debug_puthex(len); debug_putcrlf();
		return -1;	// conversion not possible
	}
//return -1;

#if DEBUG
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
	uint8_t *buf = p->buffer;
	uint8_t len = p->wp;
	do {
//debug_puts("CONVERT: "); debug_putc(*buf);debug_puthex(*buf);debug_puts("->");
		*buf = petscii_to_ascii(*buf);
//debug_putc(*buf);debug_puthex(*buf);debug_putcrlf();
		buf++;
		len--;
	} while (len > 0);
	return 0;
}

/*****************************************************************************
 * Interrupt routines
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

static void _uart_advance_slots() {
	uint8_t i = 0;
	slots_used--;
	while (i < slots_used) {
		slots[i] = slots[i+1];
		i++;
	}
	txstate = TX_TYPE;
}

static void _uart_send(void) {

	while (slots_used > 0) {
		// read data
		int16_t data = read_char_from_packet(slots[0]);
		if (data >= 0) {
			// send it
			UDR = data;
			return;
		}
		// packet empty, so get next slot
		_uart_advance_slots();
	}

	// no slot left, i.e. no data left to be sent
	// Disable ISR
	UCSRB &= ~_BV(UDRIE);
}

static void _uart_enable_sendisr() {
        UCSRB |= (1<<UDRIE);          // UDRE Interrupt ein
}

/**
 * interrupt for data send register empty
 */
ISR(USART_UDRE_vect) {
	_uart_send();
}

/**
 * interrupt for received data
 */
ISR(USART_RXC_vect)
{
        int8_t rxdata = UDR;

	switch(rxstate) {
	case RX_IDLE:
		// no current packet
		if (rxdata == FS_REPLY || rxdata == FS_WRITE || rxdata == FS_EOF) {
			// note EOI flag
			current_is_eoi = rxdata;
			// start a reply handling
			rxstate = RX_LEN;
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
//if (current_data_left > 1) led_on();
			if (rx_channels[i].channelno == current_channelno) {
				current_channelpos = i;
				current_rxpacket = rx_channels[current_channelpos].rxpacket;
				if (packet_set_write(current_rxpacket, current_channelno,
						current_is_eoi, current_data_left) >= 0) {
					rxstate = RX_DATA;
				}
				break;
			}
		}
		// well, FS_IGNORE should not happen, but we have no means of telling anyone here
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
 * wait until everything has been flushed out - for debugging, to make
 * sure all messages have been sent
 */
void uart_flush() {
	while (slots_used > 0) {
		_delay_ms(1);
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
void uart_submit(volatile packet_t *buf) {
	
	// wait for slot free
	while (slots_used >= (NUMBER_OF_SLOTS-1)) {
		_delay_ms(1);
	}


	// protect slot* by disabling the interrupt
	cli();
	// note: slots_used can only decrease until here, as this is the
	// only place to increase it, so there is no race from the while()
	// above to setting it here.	
	slots[slots_used] = buf;
	slots_used++;
	if (slots_used == 1) {
		// no packet before, so need to start sending
		_uart_enable_sendisr();
		txstate = TX_TYPE;
		_uart_send();
	}
	// enable interrupts again
	sei();
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
void uart_submit_call(int8_t channelno, packet_t *txbuf, packet_t *rxbuf, 
		void (*callback)(int8_t channelno, int8_t errnum)) {

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
		_delay_ms(1);
	}

	rx_channels[channelpos].channelno = channelno;
	rx_channels[channelpos].rxpacket = rxbuf;
	rx_channels[channelpos].callback = callback;

	// send request
	uart_submit(txbuf);
}

/*****************************************************************************
 * initialize the UART code
 */   
void uart_init() {
	slots_used = 0;

	for (int8_t i = NUMBER_OF_SLOTS-1; i >= 0; i--) {
		rx_channels[i].channelno = -1;
	}

	rxstate = RX_IDLE;
	txstate = TX_IDLE;

	// init UART
        UCSRB = _BV(TXEN);                      // TX aktiv
        UCSRB |= _BV(RXEN);             // RX aktivieren

        UBRRL = (uint8_t)(F_CPU/(BAUD*16L))-1;          // Baudrate festlegen
        UBRRH = (uint8_t)((F_CPU/(BAUD*16L))-1)>>8;     // Baudrate festlegen

        UCSRB |= _BV(RXCIE);            // UART Interrupt bei Datenempfang komplett
        //UCSRB |= _BV(UDRIE);          // UART Interrupt bei Senderegister leer
        //UCSRB |= _BV(TXCIE);          // UART Interrupt bei Sendevorgang beendet
}


