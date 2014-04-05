/**************************************************************************

  XD-2031 - Serial line filesystem server for CBMs
  Copyright (C) 2014 Andre Fachat, Nils Eilers

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

 ***************************************************************************/


#include <inttypes.h>
#include <stdbool.h>

#include <avr/eeprom.h>

#include "nvconfig.h"
#include "version.h"
#include "packet.h"
#include "debug.h"


uint8_t nv_read_byte(uint16_t addr) {
	return eeprom_read_byte((const uint8_t *) addr);
}

bool nv_write_byte(uint16_t addr, uint8_t byte) {
	uint8_t byte_read_back;

	eeprom_update_byte((uint8_t *) addr, byte);
	byte_read_back = eeprom_read_byte((const uint8_t *) addr);
	if (byte == byte_read_back) {
		return false;	// verify ok
	} else {
		debug_printf("EEPROM ERR @%04X: %02X!=%02X\n", addr, byte_read_back, byte);
		return true;	// verify error
	}
}
