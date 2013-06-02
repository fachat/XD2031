/**************************************************************************

    device.h -- device dependant definitions

    This file is part of XD-2031 -- Serial line filesystem server for CBMs

    Copyright (C) 2012 Andre Fachat <afachat@gmx.de>
    Copyrifht (C) 2012 Nils Eilers  <nils.eilers@gmx.de>

    XD-2031 is free software: you can redistribute it and/or
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


#ifndef DEVICE_H
#define DEVICE_H

// bus definitions
#  define IEEE_PORT_TE          PORTC   /* TE */
#  define IEEE_DDR_TE           DDRC
#  define IEEE_PIN_TE           PC3
// DC is controlled by slave MCU

#  define IEEE_INPUT_ATN        PIND    /* ATN */
#  define IEEE_PORT_ATN         PORTD
#  define IEEE_DDR_ATN          DDRD
#  define IEEE_PIN_ATN          PD2
#  define IEEE_INPUT_NDAC       PINC    /* NDAC */
#  define IEEE_PORT_NDAC        PORTC
#  define IEEE_DDR_NDAC         DDRC
#  define IEEE_PIN_NDAC         PC6
#  define IEEE_INPUT_NRFD       PINC    /* NRFD */
#  define IEEE_PORT_NRFD        PORTC
#  define IEEE_DDR_NRFD         DDRC
#  define IEEE_PIN_NRFD         PC7
#  define IEEE_INPUT_DAV        PINC    /* DAV */
#  define IEEE_PORT_DAV         PORTC
#  define IEEE_DDR_DAV          DDRC
#  define IEEE_PIN_DAV          PC5
#  define IEEE_INPUT_EOI        PINC    /* EOI */
#  define IEEE_PORT_EOI         PORTC
#  define IEEE_DDR_EOI          DDRC
#  define IEEE_PIN_EOI          PC4

#  define IEEE_D_PIN            PINA    /* Data */
#  define IEEE_D_PORT           PORTA
#  define IEEE_D_DDR            DDRA

#define IEEE_ATN_HANDLER        ISR(IEEE_ATN_INT_VECT)
#define IEEE_ATN_INT            INT0

#define HW_NAME			"PETSD-II"

#define	IEEE_SECADDR_OFFSET	0
#define	IEC_SECADDR_OFFSET	16

void device_init(void);

#endif
