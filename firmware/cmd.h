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
#include "wireformat.h"
#include "config.h"


typedef struct {
	uint8_t command_length;
	// command buffer; allow for null-termination
	uint8_t command_buffer[CONFIG_COMMAND_BUFFER_SIZE + 2];
//      errormsg_t      *errormsg;
} cmd_t;

#endif
