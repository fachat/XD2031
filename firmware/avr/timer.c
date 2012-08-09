
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

void timer_init(void) {
	/* Count F_CPU/8 in timer 0 */
	TCCR0B = _BV(CS01);

	// 200 Hz timer using timer 1
	OCR1A  = F_CPU / 64 / 200 - 1;
	TCNT1  = 0;
	TCCR1A = 0;
	TCCR1B = _BV(WGM12) | _BV(CS10) | _BV(CS11);
	TIMSK1 |= _BV(OCIE1A);
}


