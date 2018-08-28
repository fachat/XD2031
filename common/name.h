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

#include "command.h"

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
 * Then 'file[0].name' points to "oldname", with 'file[0].drive'=UNUSED.
 * Here we cannot just insert the old drive number "in place" - we have to 
 * move the old name one char to the back.
 * 
 * Also if there is a drive number, the old name needs to be moved to the left,
 * as "0:" is more than the single drive byte.
 */

// MAX_NAMEINFO_FILES is defined in the wireformat.

typedef struct {
	uint8_t drive;		// starts from 0 (real zero, not $30 = ASCII "0")
	uint8_t *drivename;	// name of drive ("FTP", ...)
	uint8_t *name;		// pointer to the actual name
	uint8_t namelen;	// length of file name - needed for RELfiles, as zero can be included in "name"
} drive_and_name_t;

// the parameter actually used for an OPEN on the server
typedef struct {
        uint8_t filetype;
        uint16_t recordlen;
} openpars_t;

typedef struct {
	command_t cmd;		// command, "$" for directory open
	//uint8_t type;		// file type requested ("S", "P", ...)
	uint8_t access;		// access type requested ("R", "W", "A", or "X" for r/w)
	uint8_t options;	// access options, as bit mask
	//uint16_t recordlen;	// length of / position in record from opening 'L' file (REL) / P cmd
	openpars_t pars;
	uint8_t num_files;	// number of secondary (source) drive_filenames
	drive_and_name_t trg;	// target file pattern
	drive_and_name_t file[MAX_NAMEINFO_FILES];	// optional source drive_name info
} nameinfo_t;

static inline void nameinfo_init(nameinfo_t *ninfo) {
	memset(ninfo, 0, sizeof(nameinfo_t));
	ninfo->cmd = CMD_NONE;
}

// nameinfo option bits
#define	NAMEOPT_NONBLOCKING	0x01	// use non-blocking access

// shared global variable to be used in parse_filename, as long as it's threadsafe
//extern nameinfo_t nameinfo;

/*
 * parse a CBM file name or command argument (is_command != 0 for commands), and
 * fill the nameinfo global var with the result.
 * Copies the content of the command_buffer to the end of the buffer, so that it
 * can be re-assembled at the beginning without having to worry about moving all parts
 * in the right direction.
 */
void parse_filename(uint8_t *in, uint8_t dlen, uint8_t inlen, nameinfo_t *result, uint8_t parsehint);


#define	PARSEHINT_COMMAND	1	// when called from command handler, including command
#define	PARSEHINT_LOAD		2	// when called from file handler and secaddr=0

/**
 * Parse a command file parameter
 */
void parse_cmd_pars (uint8_t *cmdstr, uint8_t len, nameinfo_t *result);

/**
 * The following two methods assemble a command packet with the filenames from a 
 * nameinfo struct (firmware), and dis-assembles a packet back into the struct (on the server).
 *
 * Note that access type is not in the packet, but encoded in the command (like FS_OPEN_AP for
 * append). Also some options may not appear in the packet, as they are firmware-relevant only
 * (e.g. NONBLOCKING I/O).
 *
 * The packet has the following structure:
 * 
 * 	DRV	- single byte drive, either 0-9 or NAMEINFO_* for unused, undefined 
 * 		  or reused drive (see wireformat)
 *	PATTERN	- path and filename or pattern, zero-terminated
 *		  Note that for named providers, the path is preceded by the
 *		  provider name and ':' as separator, like
 *		  "ftp:ftp.zimmers.net/pub/cbm"
 *	OPTIONS	- option string (see below), zero-terminated
 * 	[
 * 	DRV	- single drive or NAMEINFO_*_DRIVE
 *	PATTERN	- path and filename or pattern, zero-terminated. Format as above.
 *	] repeated 0 to 4 times
 *
 * The options string is a comma-separated list of "<option-char>=<value>".
 * Currently defined options are:
 *
 * 	T	defines the requested file type, where the R-file adds
 * 		the record length (in decimal), like
 *		"T=R64"
 */

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
uint8_t assemble_filename_packet(uint8_t * trg, nameinfo_t * nameinfo);

#ifdef SERVER

/**
 * dis-assembles a filename packet back into a nameinfo struct.
 * (Note: only available on the server).
 *
 * Returns error code, most specifically SYNTAX codes if the packet
 * cannot be parsed.
 */
cbm_errno_t parse_filename_packet(uint8_t * src, uint8_t len, openpars_t *pars, drive_and_name_t * names, int *num_files);

#endif

#endif
