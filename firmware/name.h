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

#ifndef NAME_H
#define NAME_H

#include "cmd.h"		// for command_t

/**
 * Structures and definitions for the file name handler
 */


typedef struct {
	uint8_t	drive;		// starts from 0 (real zero, not $30 = ASCII "0")
	command_t cmd;		// command, "$" for directory open
	uint8_t	type;		// file type requested ("S", "P", ...)
	uint8_t access;		// access type requested ("R", "W", "A", or "X" for r/w)
	uint8_t options;	// access options, as bit mask
	uint8_t *name;		// pointer to the actual name
	uint8_t	namelen;	// length of remaining file name
} nameinfo_t;

#define	NAMEINFO_UNUSED_DRIVE	0xff

// nameinfo option bits
#define	NAMEOPT_NONBLOCKING	0x01	// use non-blocking access

// shared global variable to be used in parse_filename, as long as it's threadsafe
extern nameinfo_t nameinfo;

void parse_filename(cmd_t *in, nameinfo_t *result, uint8_t is_command);

#endif

