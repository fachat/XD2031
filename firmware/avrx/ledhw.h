/****************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2012 Andre Fachat <afachat@gmx.de>
    Copyright (C) 2012 Nils Eilers <nils.eilers@gmx.de>

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

****************************************************************************/

#ifndef LEDHW_H
#define	LEDHW_H

// -----------------------------------------------------------------------------
// communication with timer interrupt routine for led interrupt program

// current program start point for repeats
extern volatile uint8_t led_program;

// current program pointer
extern volatile uint8_t led_current;

// current interval counter
extern volatile uint8_t led_counter;

// bit counter
extern volatile uint8_t led_bit;

// bit pattern register
extern volatile uint8_t led_pattern;

// bit shift register
extern volatile uint8_t led_shift;

extern uint8_t led_code[];

#endif


