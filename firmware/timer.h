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

/**
 * timer definitions
 *
 */

#ifndef TIMER_H
#define	TIMER_H


#include "timerhw.h"


static inline void timer_init(void) {
	timerhw_init();
}


// set the timer to underflow after the given number of us
static inline void timer_set_us(uint16_t us) {
	timerhw_set_us(us);
}

// returns !=0 when the timer has underflown
static inline uint8_t timer_is_timed_out() {
	return timerhw_has_timed_out();
}

// start timer to count down to zero within the given number of us
// resolution 10 ms
static inline void timer2_set_ms(uint16_t ms) {
	timerhw2_set_ms(ms);
}

static inline uint8_t timer2_is_timed_out(void) {
	return timer2hw_has_timed_out();
}

#endif

