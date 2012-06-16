/* 
 * XD-2031 - Serial line filesystem server for CBMs
   Copyright (C) 2012  Andre Fachat <afachat@gmx.de>

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


   file.h: Definitions for the file handling

*/

#ifndef FILE_H
#define FILE_H

#include <stdint.h>

#include "cmd.h"
#include "channel.h"

// init file data structures
void file_init(void);

// opens the file, registers an error code in command->error if necessary
// If the open was successful, setup a channel for the given channel number
// (note: explicitely not secondary device number, as IEEE and IEC in parallel
// use overlapping numbers, so they may be shifted or similar to avoid clashes)
//
// returns -1 on error, with an error message in cmd_t->error
int8_t file_open(uint8_t channel_no, cmd_t *command, void (*callback)(int8_t errnum), uint8_t is_save);


#endif
