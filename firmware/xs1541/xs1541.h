
/*
 * XD-2031 - Serial line filesystem server for CBMs 
   Copyright (C) 2012  Andre Fachat (afachat@gmx.de)

   XS-1541 specific defines to adapt sd2iec code

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License only.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#ifndef XS1541_H
#define XS1541_H

// we don't have the 75160/75161 pairs
#undef HAVE_7516X

// bus definitions
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

#  define IEEE_ATN_INT          PCINT3
#  define IEEE_PCMSK            PCMSK3
#  define IEEE_PCINT            PCINT27
#  define IEEE_ATN_INT_VECT     PCINT3_vect

//#define IEC_ATN_HANDLER   ISR(IEC_ATN_INT_VECT)
#define IEEE_ATN_HANDLER  ISR(IEEE_ATN_INT_VECT)

#define HW_NAME			"XS-1541"

#define	IEEE_SECADDR_OFFSET	0
#define	IEC_SECADDR_OFFSET	16

#endif
