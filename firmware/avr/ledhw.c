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

#include <avr/io.h>
#include <avr/interrupt.h>
#include <inttypes.h>

#include "config.h"
#include "mem.h"
#include "led.h"


// -----------------------------------------------------------------------------
// communication with timer interrupt routine for led interrupt program

// current program start point for repeats
volatile uint8_t led_program = 0;

// current program pointer
volatile uint8_t led_current = 0;

// current interval counter
volatile uint8_t led_counter = 0;

// bit counter
volatile uint8_t led_bit = 0;

// bit pattern register
volatile uint8_t led_pattern = 0;

// bit shift register
volatile uint8_t led_shift = 0;

// led program code
uint8_t led_code[] = {
	// wink program - soft blink, needs 200 Hz timer irq
	//1, 0, 1, 0x10, 2, 0x12, 2, 0x55, 2, 0x77, 2, 0xff, 2, 0x77, 2, 0x55, 2, 0x12, 2, 0x10, 0
	// hard blink - as cbm floppy
	5, 0, 5, 255, 0
};

static void init_prg(uint8_t ptr) {
	cli();
	led_shift = 0;
	led_bit = 0;
	led_pattern = 0;
	led_counter = 0;
	led_current = ptr+1;
	led_program = ptr+1;
	sei();
}

// -----------------------------------------------------------------------------

void led_init() {
	// set data direction
	LED_DDR  |= _BV(LED_BIT);
	// switch LED off
	LED_PORT &= ~_BV(LED_BIT);

	// switch off led interrupt program
	led_program = 0;
	led_current = 0;
}

static inline void _led_on() {
	led_program = 0;
	LED_PORT |= _BV(LED_BIT);
}

static inline void _led_off() {
	led_program = 0;
	LED_PORT &= ~_BV(LED_BIT);
}

void led_set(led_t mode) {
	switch(mode) {
	case IDLE:
	case OFF:
		_led_off();
		break;
	case ACTIVE:
	case ON:
		_led_on();
		break;
	case ERROR:
	default:
		init_prg(0);
		break;
	}
}




