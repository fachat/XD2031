/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2012 Andre Fachat

    Derived from:
    OS/A65 Version 1.3.12
    Multitasking Operating System for 6502 Computers
    Copyright (C) 1989-1997 Andre Fachat

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

****************************************************************************/

#ifndef CHANNEL_H
#define CHANNEL_H

#include "handler.h"

//------------------------------------------------------------------------------------
// Mapping from channel number for open files to endpoint providers
// These are set when the channel is opened

typedef struct {
       int              channo;
       file_t           *fp;
       // directory traversal info
       int              searchdrv;
       int		num_pattern;
       drive_and_name_t *searchpattern;
} chan_t;

void channel_init();
void channel_free(int channo);
void channel_set(int channo, file_t * fp);
chan_t *channel_get(int chan);

static inline file_t *channel_to_file(int chan) {
	chan_t *channel = channel_get(chan);
	return channel ? channel->fp : NULL;
}


#endif
