
/****************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2012 Andre Fachat

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

/*
 * timer handling
 */
#include <avr/io.h>

#include "config.h"

void timerhw_init(void) {
	// timer configuration derived from
	// http://www.avrfreaks.net/index.php?name=PNphpBB2&file=viewtopic&t=50106

	// ---------------------------------------------------------	
	// Timer 1: 100 Hz timer 
	OCR1A  = F_CPU / 64 / 100 - 1;
	// disable timer output on port
	TCCR1A = 0;
	// prescale by 64 (CS10/11/12), set up for CTC mode (WGM12)
	TCCR1B = (TCCR1B | _BV(WGM12) | _BV(CS10) | _BV(CS11)) & (255 - _BV(CS12));
	// initialize counter
	TCNT1  = 0;
	// enable overflow interrupt
	TIMSK1 |= _BV(OCIE1A);

/*
	// ---------------------------------------------------------	
	// timer 0: IEC underflow timer, 8 bit
	TCCR0A = 0;
	// prescale by 8, so running with approx 1.75MHz; CTC mode
	TCCR0B = (TCCR0B | _BV(WGM02) | _BV(CS01)) & (255 - _BV(CS02) - _BV(CS00));
*/
}


