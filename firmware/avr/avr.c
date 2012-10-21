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

#include <avr/interrupt.h>
#include <avr/boot.h>

#include "led.h"
#include "term.h"

ISR(BADISR_vect)
{

	// note that led_set enables interrupts, so...
	led_set(PANIC);

	while (1);
}

void fuse_info(void) {
	uint8_t lowfuse, hifuse, extfuse, lockfuse;

	cli();
	lowfuse = boot_lock_fuse_bits_get(GET_LOW_FUSE_BITS);
	hifuse = boot_lock_fuse_bits_get(GET_HIGH_FUSE_BITS);
	extfuse = boot_lock_fuse_bits_get(GET_EXTENDED_FUSE_BITS);
	lockfuse = boot_lock_fuse_bits_get(GET_LOCK_BITS);
	sei();
	term_printf("\r\nFuses: l=%02X h=%02X e=%02X l=%02X\r\n", lowfuse, hifuse, extfuse, lockfuse);
}
