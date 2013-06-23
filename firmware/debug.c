/****************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2013 Andre Fachat, Nils Eilers

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
 * Debug helpers
 */

#include <inttypes.h>
#include <ctype.h>

#include "debug.h"
#include "petscii.h"

#if DEBUG

void debug_hexdump(uint8_t *p, uint16_t len, uint8_t petscii) {
	uint16_t tot = 0;
	uint8_t line = 0;
	uint8_t x = 0;

	if(len) {
		while(tot < len) {
			debug_printf("%04X  ", tot);
			for(x=0; x<16; x++) {
				if(line+x < len) {
					tot++;
					debug_printf("%02X ", p[line+x]);
				}
				else debug_puts("   ");
				if(x == 7) debug_putc(' ');
			}
			debug_puts(" |");
			for(x=0; x<16; x++) {
				if(line+x < len) {
					uint8_t c = p[line+x];
					if (petscii) c = petscii_to_ascii(c);
					if(isprint(c)) debug_putc(c); else debug_putc(' ');
				} else debug_putc(' ');
			}
			debug_putc('|');
			debug_putcrlf();
			line = tot;
		}

	}
}

#endif
