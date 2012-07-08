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
 * This file implements the disk drive commands
 */


#ifndef CMD_H
#define CMD_H

#include <stdint.h>

#include "errormsg.h"
#include "config.h"

typedef enum {
        // Default
        //
        CMD_NONE,
        CMD_SYNTAX,
        CMD_DIR,
        //
        // CBM DOS commands
        //
        CMD_INITIALIZE,
        CMD_RENAME,
        CMD_SCRATCH,
	//
        // unsupported
        //
        // CMD_VALIDATE,
        // CMD_COPY,
        // CMD_DUPLICATE,
        // CMD_NEW,
        // CMD_POSITION,
        // CMD_BLOCKREAD,
        // CMD_BLOCKWRITE,
        // CMD_UX,
        // CMD_MEM_READ,
        // CMD_MEM_WRITE,
        // CMD_MEM_EXEC,
        //
        // new commands
        //
        CMD_CD,
        CMD_MKDIR,
        CMD_RMDIR,
        CMD_ASSIGN
} command_t;

typedef struct {
	uint8_t 		command_length;
	// command buffer
	uint8_t 		command_buffer[CONFIG_COMMAND_BUFFER_SIZE+2];
//	errormsg_t	*errormsg;
} cmd_t;

command_t command_find(uint8_t *buf);

const char* command_to_name(command_t cmd);

int8_t command_execute(uint8_t channel_no, cmd_t *command, errormsg_t *errormsg, 
						void (*callback)(int8_t errnum, uint8_t *rxdata));


#endif
