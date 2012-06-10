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
 * conversion between PETSCII and ASCII
 */

#ifndef PETSCII_H
#define PETSCII_H

/**
 * simple conversion
 */
static inline uint8_t petscii_to_ascii(uint8_t v) {
	if (v < 0x41) return v;	
	if (v < 0x5b) return v+0x20;	// lower PETSCII to lower ASCII
	if (v < 0x61) return v;
	if (v < 0x7b) return v-0x20;	// upper C64 PETSCII to upper ASCII
	if (v < 0xc1) return v;
	if (v < 0xdb) return v & 0x7f;	// upper PET PETSCII to upper ASCII
	return v;
}

/**
 * simple conversion
 */
static inline uint8_t ascii_to_petscii(uint8_t v) {
	if (v < 0x41) return v;	
	if (v < 0x5b) return v+0x80;	// upper ASCII to upper PETSCII
	if (v < 0x61) return v;
	if (v < 0x7b) return v-0x20;	// lower ASCII to lower C64/PET PETSCII
	return v;
}


#endif

