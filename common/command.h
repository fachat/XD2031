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
 * This file implements the disk drive commands
 */

#ifndef COMMAND_H
#define COMMAND_H

#include "wireformat.h"
#include "config.h"

// Note the value definitions are such that no mapping is necessary between internal
// command codes and wireformat values
typedef enum {
	// Default
	//
	CMD_NONE,
	CMD_SYNTAX,
	CMD_OVERWRITE = FS_OPEN_OW,
	CMD_DIR = FS_OPEN_DR,
	//
	// CBM DOS commands
	//
	CMD_INITIALIZE = FS_INITIALIZE,
	CMD_RENAME = FS_MOVE,
	CMD_SCRATCH = FS_DELETE,
	CMD_POSITION = FS_POSITION,
	CMD_VALIDATE = FS_CHKDSK,
	CMD_COPY = FS_COPY,
	CMD_DUPLICATE = FS_DUPLICATE,
	CMD_NEW = FS_FORMAT,
	CMD_BLOCK = FS_BLOCK,
	CMD_UX = FS_SYNC,	// makes no sense but doesn't conflict
	//
	// unsupported
	//
	// CMD_MEM_READ,
	// CMD_MEM_WRITE,
	// CMD_MEM_EXEC,
	//
	// new commands
	//
	CMD_CD = FS_CHDIR,
	CMD_MKDIR = FS_MKDIR,
	CMD_RMDIR = FS_RMDIR,
	CMD_ASSIGN = FS_ASSIGN,
	// configuration extension
	CMD_EXT,
	// date/time commands
	CMD_TIME = FS_GETDATIM
} command_t;

#endif
