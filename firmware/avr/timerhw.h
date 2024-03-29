
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

#ifndef TIMERHW_H
#define TIMERHW_H

#include <avr/io.h>
#include <util/atomic.h>

extern uint16_t timer10ms;

void timerhw_init(void);

static inline void timerhw_set_us(uint16_t us)
{
	// we run at 1.75MHz, so approx. 2MHz, thus divide us by 2 to get counter value
	OCR0A = 0xff & (us >> 2);
	TCNT0 = 0;
	// clear the overflow bit, so we can check for overflow
	TIFR0 |= _BV(TOV0) | _BV(OCF0A);
}

static inline uint8_t timerhw_has_timed_out()
{
	return TIFR0 & _BV(OCF0A);
	//return TIFR0 & _BV(TOV0);
}

static inline void timerhw2_set_ms(uint16_t ms)
{
	// resolution is 10 ms, thus divide by 10 to get counter value
	ATOMIC_BLOCK(ATOMIC_FORCEON) {
		timer10ms = ms / 10;
	}
}

static inline uint8_t timer2hw_has_timed_out(void)
{
	return (timer10ms ? 0 : 1);
}
#endif
