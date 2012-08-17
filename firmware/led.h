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
 * LED hardware layer
 *
 * Takes the LED_PORT, LED_DDR and LED_BIT definitions from config.h
 */

#ifndef LED_H
#define	LED_H

typedef enum {
	IDLE	= 0,
	OFF	= 1,
	ON	= 2,
	ACTIVE	= 3,
	ERROR	= 4,
	PANIC	= 5
} led_t;

void led_set(led_t signal);

void led_init();

static inline void led_on() {
	led_set(ON);
}

static inline void led_off() {
	led_set(OFF);
}

void led_toggle();

#endif

