/* 
 * XD-2031 - Serial line file server for CBMs
   Copyright (C) 2012  Andre Fachat

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License only.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#include "cmd.h"
#include "debug.h"


//struct cmd_t {
//	uint8_t 		command_length;
//	// command buffer
//	uint8_t 		command_buffer[CONFIG_COMMAND_BUFFER_SIZE+2];
//	struct errormsg_t	*errormsg;
//};

void doscommand(cmd_t *command) {

	debug_putps("COMMAND: ");
	debug_puts((char*)&(command->command_buffer));
	debug_putcrlf();
}

