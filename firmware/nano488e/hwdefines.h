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
//#include "avr/iom4809.h"

// bus definitions

/* TE = D6 = PF4 */
#define IEEE_PORT_TE          PORTF_OUT
#define IEEE_DDR_TE           PORTF_DIR
#define IEEE_PIN_TE           4

/* DC = D13 = PB5 
#define IEEE_PORT_DC          PORTB	
#define IEEE_DDR_DC           DDRB
#define IEEE_PIN_DC           PB5
*/

/* ATNA = D13 = PE2 */
#define IEEE_PORT_ATNA        PORTE_OUT
#define IEEE_DDR_ATNA         PORTE_DIR
#define IEEE_PIN_ATNA         2

/* ATN = D7 = PA1 */
#define IEEE_INPUT_ATN        PORTA_IN
#define IEEE_PORT_ATN         PORTA_OUT
#define IEEE_DDR_ATN          PORTA_DIR
#define IEEE_PIN_ATN          1

/* NDAC = D9 = PB0 */
#define IEEE_INPUT_NDAC       PORTB_IN
#define IEEE_PORT_NDAC        PORTB_OUT
#define IEEE_DDR_NDAC         PORTB_DIR
#define IEEE_PIN_NDAC         0

/* NRFD = D10 = PB1 */
#define IEEE_INPUT_NRFD       PORTB_IN
#define IEEE_PORT_NRFD        PORTB_OUT
#define IEEE_DDR_NRFD         PORTB_DIR
#define IEEE_PIN_NRFD         1

/* DAV = D11 = PE0 */
#define IEEE_INPUT_DAV        PORTE_IN
#define IEEE_PORT_DAV         PORTE_OUT
#define IEEE_DDR_DAV          PORTE_DIR
#define IEEE_PIN_DAV          0

/* EOI = D12 = PE1 */
#define IEEE_INPUT_EOI        PORTE_IN
#define IEEE_PORT_EOI         PORTE_OUT
#define IEEE_DDR_EOI          PORTE_DIR
#define IEEE_PIN_EOI          1

/* IFC = D8 = PE3 */
#define IEEE_INPUT_IFC        PORTE_IN
#define IEEE_PORT_IFC         PORTE_OUT
#define IEEE_DDR_IFC          PORTE_DIR
#define IEEE_PIN_IFC          3

/* REN = D3 = PD3 
#define IEEE_INPUT_REN        PIND
#define IEEE_PORT_REN         PORTD
#define IEEE_DDR_REN          DDRD
#define IEEE_PIN_REN          PD3
*/

/* SRQ = D2 = PA0 */
#define IEEE_INPUT_SRQ        PORTA_IN
#define IEEE_PORT_SRQ         PORTA_OUT
#define IEEE_DDR_SRQ          PORTA_DIR
#define IEEE_PIN_SRQ          0

/* Data = A0-5,D4,D5 = PD3..PD0,PA2,PA3, PC6, PB2 */
#define IEEE_DA_PIN            PORTA_IN
#define IEEE_DA_PORT           PORTA_OUT
#define IEEE_DA_DDR            PORTA_DIR
#define IEEE_DA_PINS           0x0c
#define IEEE_DA_SHIFTL         2

#define IEEE_DB_PIN            PORTB_IN
#define IEEE_DB_PORT           PORTB_OUT
#define IEEE_DB_DDR            PORTB_DIR
#define IEEE_DB_PINS           0x04
#define IEEE_DB_SHIFTL         5

#define IEEE_DC_PIN            PORTC_IN
#define IEEE_DC_PORT           PORTC_OUT
#define IEEE_DC_DDR            PORTC_DIR
#define IEEE_DC_PINS           0x40
#define IEEE_DC_SHIFTL         0

#define IEEE_DD_PIN            PORTD_IN
#define IEEE_DD_PORT           PORTD_OUT
#define IEEE_DD_DDR            PORTD_DIR
#define IEEE_DD_PINS           0x0f
#define IEEE_DD_REORDER
#define IEEE_DD_SHIFTR         4


#define HW_NAME			"NANO488E"

#define	IEEE_SECADDR_OFFSET	0

// LED configuration for single LED HW / red error LED
/* LED = D3 = PF5 */
#define LED_DDR			PORTF_DIR
#define LED_PORT		PORTF_OUT
#define LED_BIT			5
#define	LED_BIT_bm		_BV(LED_BIT)

#endif
