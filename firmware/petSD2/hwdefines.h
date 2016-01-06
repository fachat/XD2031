/**************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2013 Andre Fachat <afachat@gmx.de>
    Copyright (C) 2013 Nils Eilers <nils.eilers@gmx.de>

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

#include "avr/io.h"

// bus definitions
#define IEEE_PORT_TE          PORTC	/* TE */
#define IEEE_DDR_TE           DDRC
#define IEEE_PIN_TE           PC3
// DC is controlled by slave MCU

#define IEEE_INPUT_ATN        PIND	/* ATN */
#define IEEE_PORT_ATN         PORTD
#define IEEE_DDR_ATN          DDRD
#define IEEE_PIN_ATN          PD2
#define IEEE_INPUT_NDAC       PINC	/* NDAC */
#define IEEE_PORT_NDAC        PORTC
#define IEEE_DDR_NDAC         DDRC
#define IEEE_PIN_NDAC         PC6
#define IEEE_INPUT_NRFD       PINC	/* NRFD */
#define IEEE_PORT_NRFD        PORTC
#define IEEE_DDR_NRFD         DDRC
#define IEEE_PIN_NRFD         PC7
#define IEEE_INPUT_DAV        PINC	/* DAV */
#define IEEE_PORT_DAV         PORTC
#define IEEE_DDR_DAV          DDRC
#define IEEE_PIN_DAV          PC5
#define IEEE_INPUT_EOI        PINC	/* EOI */
#define IEEE_PORT_EOI         PORTC
#define IEEE_DDR_EOI          DDRC
#define IEEE_PIN_EOI          PC4

#define IEEE_D_PIN            PINA	/* Data */
#define IEEE_D_PORT           PORTA
#define IEEE_D_DDR            DDRA

#define IEEE_ATN_HANDLER        ISR(IEEE_ATN_INT_VECT)
#define IEEE_ATN_INT            INT0

#define HW_NAME			"PETSD-II"

#define	IEEE_SECADDR_OFFSET	0
#define	IEC_SECADDR_OFFSET	16

// LED configuration for single LED HW / red error LED
#define LED_DDR			DDRD
#define LED_PORT		PORTD
#define LED_BIT			PD6

// LED configuration for separate green activity LED
// Leave ACTIVE_LED_DDR undefined for HW without activity LED
#define ACTIVE_LED_DDR		DDRD
#define ACTIVE_LED_PORT		PORTD
#define ACTIVE_LED_BIT		PD5

// SPI
#define SPI_PORT 		PORTB
#define SPI_DDR 		DDRB
#define SPI_PIN_SCK 		PB7
#define SPI_PIN_MISO 		PB6
#define SPI_PIN_MOSI 		PB5

// SD card detect interrupt
#define CARD_DETECT_INT_VECT 	PCINT3_vect
#define MEDIA_CHANGE_HANDLER 	ISR(CARD_DETECT_INT_VECT)
#define SDCD_PCIF 		PCIF3
#define SDCD_PCMSK 		PCMSK3
#define SDCD_PCINT 		PCINT28
#define SDCD_PCIE 		PCIE3

// SD card select
#define PORT_SD_CS 		PORTB
#define DDR_SD_CS 		DDRB
#define PIN_SD_CS 		PB4

// SD card write protect switch
#define INPUT_SD_WP 		PIND
#define PORT_SD_WP 		PORTD
#define DDR_SD_WP 		DDRD
#define PIN_SD_WP 		PD7

// SD card detect switch
#define INPUT_SD_CD 		PIND
#define PORT_SD_CD 		PORTD
#define DDR_SD_CD 		DDRD
#define PIN_SD_CD 		PD4

// I2C bit banging ports
#define INPUT_SDA               PINC
#define DDR_SDA                 DDRC
#define PORT_SDA                PORTC
#define PIN_SDA                 PC1
#define INPUT_SCL               PINC
#define PORT_SCL                PORTC
#define DDR_SCL                 DDRC
#define PIN_SCL                 PC0

#endif
