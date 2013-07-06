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


#ifndef CMD_H
#define CMD_H

#include <stdint.h>

#include "errormsg.h"
#include "rtconfig.h"
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
        CMD_INITIALIZE,
        CMD_RENAME = FS_MOVE,
        CMD_SCRATCH = FS_DELETE,
	CMD_POSITION,
	//
        // unsupported
        //
        // CMD_VALIDATE,
        // CMD_COPY,
        // CMD_DUPLICATE,
        // CMD_NEW,
        CMD_BLOCK = FS_BLOCK,
        CMD_UX,
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
	CMD_EXT
} command_t;

typedef struct {
	uint8_t 		command_length;
	// command buffer
	uint8_t 		command_buffer[CONFIG_COMMAND_BUFFER_SIZE+2];
//	errormsg_t	*errormsg;
} cmd_t;

command_t command_find(uint8_t *buf);

const char* command_to_name(command_t cmd);


#endif
