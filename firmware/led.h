/****************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2012 Andre Fachat

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation;
    version 2 of the License ONLY.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

****************************************************************************/

/**
 * LED hardware layer
 */

#include <avr/io.h>

// these need to go into config.h or elswhere
#define LED_DDR               DDRC
#define LED_PORT              PORTC
#define LED_BIT               PC0

static inline void led_init() {
	// set data direction
	LED_DDR  |= _BV(LED_BIT);
	// switch LED off
	LED_PORT &= ~_BV(LED_BIT);
}

static inline void led_on() {
	LED_PORT |= _BV(LED_BIT);
}

static inline void led_off() {
	LED_PORT &= ~_BV(LED_BIT);
}

static inline void led_toggle() {
	LED_PORT ^= _BV(LED_BIT);
}

