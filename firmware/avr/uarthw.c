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

#include "config.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <util/delay.h>

#include "compat.h"
#include "uarthw.h"
#include "led.h"

/***********************************************************************************
 * UART stuff
 */

#define	BUFFER_SIZE		128
#define	BUFFER_SIZE_MASK	0x7f
#define	BUFFER_NEXT(p)		((p + 1) & BUFFER_SIZE_MASK)

// prototypes
static void _uart_enable_sendisr();

// send ring buffer
volatile int8_t 	txbuf[BUFFER_SIZE];
volatile uint8_t 	tx_wp;
volatile uint8_t 	tx_rp;

// receive ring buffer
volatile int8_t 	rxbuf[BUFFER_SIZE];
volatile uint8_t 	rx_wp;
volatile uint8_t 	rx_rp;
volatile uint8_t 	rx_err;


/*****************************************************************************
 * interface code
 */


/**
 * when returns true, there is space in the uart send buffer,
 * so uarthw_send(...) can be called with a byte to send
 */
int8_t uarthw_can_send() {
	// check for wrap-around to read pointer
	uint8_t tmp = BUFFER_NEXT(tx_wp);
	return (tmp != tx_rp) ? 1 : 0;
}

/**
 * submit a byte to the send buffer
 */
void uarthw_send(int8_t data) {
	txbuf[tx_wp] = data;
	tx_wp = BUFFER_NEXT(tx_wp);
	_uart_enable_sendisr();
}

/**
 * receive a byte from the receive buffer
 * Returns -1 when no byte available
 */
int16_t uarthw_receive() {
	if (rx_wp == rx_rp) {
		// no data available
		return -1;
	}
	int16_t data = 0xff & rxbuf[rx_rp];
	rx_rp = BUFFER_NEXT(rx_rp);
	return data;
}


/*****************************************************************************
 * Interrupt routines
 */


static void _uart_enable_sendisr() {
        UCSRB |= (1<<UDRIE);          // UDRE Interrupt ein
}

/**
 * interrupt for data send register empty
 *
 * Note that it does not block other interrupts - most specifically
 * the one we need for ATN handling.
 *
 * That means that this interrupt, plus all other interrupt routines
 * must be short enough to be done between two of the UDRE interrupts
 *
ISR(USART_UDRE_vect) {
	if (tx_wp != tx_rp) {
		UDR = txbuf[tx_rp];

		tx_rp = BUFFER_NEXT(tx_rp);

		// allow other interrupts from now,
		// esp. the ATN interrupt
		sei();
	}

	if (tx_wp == tx_rp) {
		// no more data available
		// Disable ISR
		UCSRB &= ~_BV(UDRIE);
	}
}
*/

/**
 * interrupt for received data
 *
 * Again note that this interrupt routine runs without other interrupts
 * blocked. 
 *
ISR(USART_RXC_vect)
{
	rxbuf[rx_wp] = UDR;

	// allow other interrupts from now,
	// esp. the ATN interrupt
	sei();

	uint8_t tmp = BUFFER_NEXT(rx_wp);
	if (tmp == rx_rp) {
		// buffer overflow
		rx_err++;
		led_on();
	} else {
		rx_wp = tmp;
	}
}
*/

/*****************************************************************************
 * initialize the UART code
 */   
void uarthw_init() {

	// tx ring buffer init
	tx_wp = 0;
	tx_rp = 0;

	// rx ring buffer init
	rx_wp = 0;
	rx_rp = 0;

	// init UART
        UCSRB = _BV(TXEN);              // TX aktiv
        UCSRB |= _BV(RXEN);             // RX aktivieren

        UBRRL = (uint8_t)(F_CPU/(BAUD*16L))-1;          // Baudrate festlegen
        UBRRH = (uint8_t)((F_CPU/(BAUD*16L))-1)>>8;     // Baudrate festlegen

        UCSRB |= _BV(RXCIE);            // UART Interrupt bei Datenempfang komplett
        //UCSRB |= _BV(UDRIE);          // UART Interrupt bei Senderegister leer
        //UCSRB |= _BV(TXCIE);          // UART Interrupt bei Sendevorgang beendet
}


