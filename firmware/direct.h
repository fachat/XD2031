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

#ifndef DIRECT_H
#define DIRECT_H

int8_t cmd_user(bus_t * bus, char *cmdbuf, uint8_t * err_trk, uint8_t * err_sec,
		uint8_t * err_drv);
int8_t cmd_block(bus_t * bus, char *cmdbuf, uint8_t * err_trk,
		 uint8_t * err_sec, uint8_t * err_drv);

void direct_init();

endpoint_t *direct_provider(void);

int8_t direct_open(uint8_t channel_no, bus_t * bus, errormsg_t * errormsg,
		   void (*callback) (int8_t errnum, uint8_t * rxdata),
		   uint8_t * name);

#endif
