/**************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2013 Andre Fachat <afachat@gmx.de>

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

**************************************************************************/

/**
 * Hardware dependent definitions
 *
 * This file is also included by the assembler, 
 * no C code allowed here!
 */

#ifndef HWDEFINES_H
#define HWDEFINES_H

#define HW_NAME			"PCSOCK"

#if 0

#include "avr/io.h"

// IEEE bus definitions
#define IEEE_DDR_NDAC		DDRC
#define	IEEE_PORT_NDAC		PORTC
#define	IEEE_INPUT_NDAC		PINC
#define	IEEE_PIN_NDAC		PC4

#define	IEEE_DDR_NRFD		DDRC
#define	IEEE_PORT_NRFD		PORTC
#define	IEEE_INPUT_NRFD		PINC
#define	IEEE_PIN_NRFD		PC5

#define IEEE_DDR_EOI		DDRC
#define	IEEE_PORT_EOI		PORTC
#define	IEEE_INPUT_EOI		PINC
#define	IEEE_PIN_EOI		PC7

#define	IEEE_DDR_DAV		DDRC
#define	IEEE_PORT_DAV		PORTC
#define	IEEE_INPUT_DAV		PINC
#define	IEEE_PIN_DAV		PC6

#define	IEEE_DDR_ATN		DDRD
#define	IEEE_PORT_ATN		PORTD
#define	IEEE_INPUT_ATN		PIND
#define	IEEE_PIN_ATN		PD3

#define	IEEE_D_DDR		DDRA
#define	IEEE_D_PORT		PORTA
#define	IEEE_D_PIN		PINA

#define	IEEE_A_DDR		DDRD
#define	IEEE_A_PORT		PORTD

/* interrupt init from sd2iec */

#define IEEE_ATN_INT          PCINT3
#define IEEE_PCINT            PCINT27

//#define IEC_ATN_HANDLER   ISR(IEC_ATN_INT_VECT)

#define HW_NAME			"XS-1541"

// DATA and CLK use the same port
#define IEC_DDR			DDRD
#define	IEC_PORT		PORTD
#define	IEC_INPUT		PIND

// IEC bus definitions
#define	IEC_PIN_DATA		PD4
#define	IEC_PIN_CLK		PD7

// ATN actually uses the very same port, but this is more portable
#define	IEC_DDR_ATN		DDRD
#define	IEC_PORT_ATN		PORTD
#define	IEC_INPUT_ATN		PIND
#define	IEC_PIN_ATN		PD2

/* interrupt init */
#define IEC_SATN_INT         	PCINT3
#define IEC_PCINT            	PCINT26

/* general pin change interrupt for serial ATN as well as parallel ATN */
#define XS1541_PCMSK         	PCMSK3
#define XS1541_ATN_INT_VECT  	PCINT3_vect
#define XS1541_ATN_HANDLER  	ISR(XS1541_ATN_INT_VECT)

/* LED configuration */
#define LED_DDR			DDRC
#define LED_PORT		PORTC
#define LED_BIT			PC0

// SPI
#define SPI_PORT 		PORTB
#define SPI_DDR 		DDRB
#define SPI_PIN_SCK 		PB7
#define SPI_PIN_MISO 		PB6
#define SPI_PIN_MOSI 		PB5

// SD card select
#define PORT_SD_CS 		PORTB
#define DDR_SD_CS 		DDRB
#define PIN_SD_CS 		PB4

// HW has no SD card detect switch, hence defined as normal function:
#define MEDIA_CHANGE_HANDLER 	void dummy_media_change_handler(void)

#if 0
// SD card write protect switch
#define INPUT_SD_WP 		PINC
#define PORT_SD_WP 		PORTC
#define DDR_SD_WP 		DDRC
#define PIN_SD_WP 		PC3

// SD card detect switch
#define INPUT_SD_CD 		PIND
#define PORT_SD_CD 		PORTD
#define DDR_SD_CD 		DDRD
#define PIN_SD_CD 		PD4
#endif

#endif
#endif
