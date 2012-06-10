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
 * Structures and definitions for the file name handler
 */

typedef struct {
	uint8_t	drive;		// starts from 0 (real zero, not $30 = ASCII "0")
	uint8_t cmd;		// command, "$" for directory open
	uint8_t	type;		// file type requested ("S", "P", ...)
	uint8_t access;		// access type requested ("R", "W", ...)
	uint8_t *name;		// pointer to the actual name
	uint8_t	namelen;	// length of remaining file name
} nameinfo_t;

void parse_filename(cmd_t *in, nameinfo_t *result);
