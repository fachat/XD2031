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
 *
 * note that we have a problem when the rename command does not
 * use a drive parameter in the from name like this:
 *
 *	R0:newname=oldname
 *
 * In this case the 'name' member points to "newname", with 'namelen'=7,
 * and 'drive'=0. The "=" will be replaced by a zero byte.
 * Then 'name2' points to "oldname", with 'drive2'=UNUSED.
 * Here we cannot just insert the old drive number "in place" - we have to 
 * move the old name one char to the back.
 * 
 * Also if there is a drive number, the old name needs to be moved to the left,
 * as "0:" is more than the single drive byte.
 */


typedef struct {
	uint8_t	drive;		// starts from 0 (real zero, not $30 = ASCII "0")
	command_t cmd;		// command, "$" for directory open
	uint8_t	type;		// file type requested ("S", "P", ...)
	uint8_t access;		// access type requested ("R", "W", "A", or "X" for r/w)
	uint8_t options;	// access options, as bit mask
	uint8_t *name;		// pointer to the actual name
	uint8_t	namelen;	// length of file name
	uint8_t	drive2;		// starts from 0 (real zero, not $30 = ASCII "0")
	uint8_t *name2;		// pointer to the actual name after the '='
	uint8_t	namelen2;	// length of remaining file name
	uint16_t recordlen;	// length of / position in record from opening 'L' file (REL) / P cmd
	uint16_t recordno;	// record number from P command
} nameinfo_t;

// nameinfo option bits
#define	NAMEOPT_NONBLOCKING	0x01	// use non-blocking access

// shared global variable to be used in parse_filename, as long as it's threadsafe
extern nameinfo_t nameinfo;

/*
 * parse a CBM file name or command argument (is_command != 0 for commands), and
 * fill the nameinfo global var with the result.
 * Copies the content of the command_buffer to the end of the buffer, so that it
 * can be re-assembled at the beginning without having to worry about moving all parts
 * in the right direction.
 */
void parse_filename(cmd_t *in, nameinfo_t *result, uint8_t parsehint);

#define	PARSEHINT_COMMAND	1	// when called from command handler
#define	PARSEHINT_LOAD		2	// when called from file handler and secaddr=0

/**
 * assembles the filename packet from nameinfo into the target buffer.
 * For this it is essential, that nameinfo content does not overlap
 * (at least in the order of assembly) with the target buffer.
 * That is why the command_buffer content is moved to the end of the
 * command_buffer in parse_filename - so it can be re-assembled in the
 * beginning of the command_buffer.
 * 
 * it returns the number of bytes in the buffer
 */
uint8_t assemble_filename_packet(uint8_t *trg, nameinfo_t *nameinfo);

#endif

