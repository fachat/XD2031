/* 
 * XD2031 - Serial line file server for CBMs
 * Copyright (C) 2012 Andre Fachat <afachat@gmx.de>
 * 
 * Taken over / derived from:
 * sd2iec - SD/MMC to Commodore serial bus interface/controller
   Copyright (C) 2007-2012  Ingo Korb <ingo@akana.de>
   Inspired by MMC2IEC by Lars Pontoppidan et al.

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


   ieee.h: Definitions for the IEEE-488 handling code

*/

#ifndef IEEE_H
#define IEEE_H

#include "xs1541.h"
#include "cmd.h"
#include "file.h"
#include "channel.h"

void ieee_init(void);

void ieee_mainloop_init(void);
void ieee_mainloop_iteration(void);

/**
 * the following functions are ieee-internal actually.
 */

static inline uint8_t ieee_secaddr_to_channel(uint8_t secaddr) {
	return secaddr + IEEE_SECADDR_OFFSET;
}

static inline int8_t ieee_file_open(uint8_t secaddr, cmd_t *command, void (*callback)(int8_t errnum)) {
	return file_open(ieee_secaddr_to_channel(secaddr), command, callback, secaddr == 1);
}

static inline void ieee_channel_close_all() {
	channel_close_range(ieee_secaddr_to_channel(0), ieee_secaddr_to_channel(15));
}

#endif
