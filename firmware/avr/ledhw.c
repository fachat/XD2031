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

/**
 * LED hardware layer
 *
 * Takes the LED_PORT, LED_DDR and LED_BIT definitions from config.h
 */

#include "config.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <inttypes.h>

#include "hwdefines.h"
#include "device.h"
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

#define	PANIC_OFFSET 	5

// led program code
uint8_t led_code[] = {
	// wink program - soft blink, needs 200 Hz timer irq
	//1, 0, 1, 0x10, 2, 0x12, 2, 0x55, 2, 0x77, 2, 0xff, 2, 0x77, 2, 0x55, 2, 0x12, 2, 0x10, 0
	// hard blink - as cbm floppy 200 Hz timer
	//5, 0, 5, 255, 0
	// hard blink - as cbm floppy 100 Hz timer
	2, 0, 2, 255, 0,
	// panic blink
	1, 0x0f, 0
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

static uint8_t has_error = 0;

// -----------------------------------------------------------------------------

/* Device with two  (red and green) LEDs: red --> both   --> green --> both off
   Device with dual (red and green) LED:  red --> yellow --> green --> both off
   Device with single LED:                on  --> off    --> on    --> off
*/
static inline void led_hello_blinky(void) {
#ifdef ACTIVE_LED_DDR
	device_led_on();				_delay_ms(300);
	device_active_led_on();				_delay_ms(300);
	device_leds_off(); device_active_led_on(); 	_delay_ms(600);
#else
	device_led_on();	_delay_ms(500);
	device_leds_off();	_delay_ms(200);
	device_led_on();	_delay_ms(500);
#endif
	device_leds_off();
}

void led_init() {
	// set data direction
	LED_DDR  |= _BV(LED_BIT);
#	ifdef ACTIVE_LED_DDR
		ACTIVE_LED_DDR |= _BV(ACTIVE_LED_BIT);
#	endif

	// switch LEDs off
	device_leds_off();

	// switch off led interrupt program
	led_program = 0;
	led_current = 0;

	// save error state over ACTIVE/IDLE calls
	has_error = 0;

	led_hello_blinky();
}

static inline void _led_on() {
	led_program = 0;
	device_led_on();
}

static inline void _active_led_on() {
#	ifdef ACTIVE_LED_DDR
		device_active_led_on();
#	else
		_led_on();
#	endif
}

static inline void _leds_off() {
	led_program = 0;
	device_leds_off();
}

void led_toggle() {
	LED_PORT ^= _BV(LED_BIT);
}

void led_set(led_t mode) {
	switch(mode) {
	case IDLE:
		if (has_error) {
			init_prg(0);
		} else {
			_leds_off();
		}
		break;
	case OFF:
		has_error = 0;
		_leds_off();
		break;
	case ACTIVE:
		if(!has_error) _active_led_on();
		break;
	case ON:
		_led_on();
		break;
	case PANIC:
		init_prg(PANIC_OFFSET);
		break;
	case ERROR:
		has_error = 1;
		_leds_off();
	default:
		init_prg(0);
		break;
	}
}




