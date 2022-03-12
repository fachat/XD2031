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

/* TE = D6 = PD6 */
#define IEEE_PORT_TE          PORTD	/* TE */
#define IEEE_DDR_TE           DDRD
#define IEEE_PIN_TE           PD6

/* DC = D13 = PB5 
#define IEEE_PORT_DC          PORTB	
#define IEEE_DDR_DC           DDRB
#define IEEE_PIN_DC           PB5
*/

/* ATNA = D13 = PB5 */
#define IEEE_PORT_ATNA        PORTB
#define IEEE_DDR_ATNA         DDRB
#define IEEE_PIN_ATNA         PB5

/* ATN = D7 = PD7 */
#define IEEE_INPUT_ATN        PIND	/* ATN */
#define IEEE_PORT_ATN         PORTD
#define IEEE_DDR_ATN          DDRD
#define IEEE_PIN_ATN          PD7

/* NDAC = D9 = PB1 */
#define IEEE_INPUT_NDAC       PINB	/* NDAC */
#define IEEE_PORT_NDAC        PORTB
#define IEEE_DDR_NDAC         DDRB
#define IEEE_PIN_NDAC         PB1

/* NRFD = D10 = PB2 */
#define IEEE_INPUT_NRFD       PINB	/* NRFD */
#define IEEE_PORT_NRFD        PORTB
#define IEEE_DDR_NRFD         DDRB
#define IEEE_PIN_NRFD         PB2

/* DAV = D11 = PB3 */
#define IEEE_INPUT_DAV        PINB	/* DAV */
#define IEEE_PORT_DAV         PORTB
#define IEEE_DDR_DAV          DDRB
#define IEEE_PIN_DAV          PB3

/* EOI = D12 = PB4 */
#define IEEE_INPUT_EOI        PINB	/* EOI */
#define IEEE_PORT_EOI         PORTB
#define IEEE_DDR_EOI          DDRB
#define IEEE_PIN_EOI          PB4

/* IFC = D8 = PB0 */
#define IEEE_INPUT_IFC        PINB	/* IFC */
#define IEEE_PORT_IFC         PORTB
#define IEEE_DDR_IFC          DDRB
#define IEEE_PIN_IFC          PB0

/* REN = D3 = PD3 
#define IEEE_INPUT_REN        PIND
#define IEEE_PORT_REN         PORTD
#define IEEE_DDR_REN          DDRD
#define IEEE_PIN_REN          PD3
*/

/* SRQ = D2 = PD2 */
#define IEEE_INPUT_SRQ        PIND	/* SRQ */
#define IEEE_PORT_SRQ         PORTD
#define IEEE_DDR_SRQ          DDRD
#define IEEE_PIN_SRQ          PD2

/* Data = A0-5,D4,D5 = PC0..5, PD4, PD5 */
#define IEEE_D_PIN            PINC
#define IEEE_D_PORT           PORTC
#define IEEE_D_DDR            DDRC
#define IEEE_D_PINS           0x3f

#define IEEE_D2_PIN           PIND
#define IEEE_D2_PORT          PORTD
#define IEEE_D2_DDR           DDRD
#define IEEE_D2_PINS          0x30
#define IEEE_D2_SHIFT_R		2

#define	IEEE_SECADDR_OFFSET	0

// LED configuration for single LED HW / red error LED
#if 1
/* LED = D3 = PD3 */
#define LED_DDR			DDRD
#define LED_PORT		PORTD
#define LED_BIT			PD3
#else
// debug, using DC output to drive onboard LED
#define LED_DDR			DDRB
#define LED_PORT		PORTB
#define LED_BIT			PB5
#endif


#endif
