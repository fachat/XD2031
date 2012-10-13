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

#ifndef BUS_H
#define	BUS_H

#include "cmd.h"
#include "channel.h"
#include "rtconfig.h"

/*
 * IEEE488 impedance layer
 *
 * provides the parallelsendbyte, parallelreceivebyte and parallelattention
 * methods called by the IEEE loop, and translates these into 
 * calls to the channel framework, open calls etc.
 */

// these are the runtime variables for a bus end point like
// the IEEE488 or the serial IEC bus.
typedef struct {
	// configuration

	uint8_t		secaddr_offset;	// offset to use on secondary address to get channel no
					// to avoid collisions with other busses
	// runtime for the bus
	channel_t       *channel;	// current open file channel

	uint8_t         device;		// primary command byte, includes dev addr and TALK/LISTEN/...
	uint8_t         secondary;	// secondary command byte, includes sec addr and OPEN/CLOSE/...

	// interrupt callback handling for commands and opens
	uint8_t         cmd_done;	// set on a callback from the irq
  	int8_t 		errnum; 	// from interrupt between BUS_CMDWAIT and BUS_CMDPROCESS
  	uint8_t		errparam; 	// from interrupt between BUS_CMDWAIT and BUS_CMDPROCESS

	// command channel
	cmd_t		command;	// command buffer
	//errormsg_t	error;		// error message - is currently shared between busses
	
	// runtime config, like unit number, last used drive etc
	rtconfig_t	rtconf;		
} bus_t;

// definitions for bus_sendbyte
#define	BUS_SYNC	PUT_SYNC		// from channel.h
#define	BUS_FLUSH	PUT_FLUSH		// from channel.h

// init
// needs to be called before any concrete bus instance init
void bus_init();

// IEEE/IEC protocol routines
int16_t bus_receivebyte(bus_t *bus, uint8_t *c, uint8_t newbyte);

int16_t bus_attention(bus_t *bus, uint8_t cmd);

int16_t bus_sendbyte(bus_t *bus, uint8_t cmd, uint8_t options);

// init the bus_t structure
void bus_init_bus(bus_t *bus);

// helper method

static inline uint8_t bus_secaddr_adjust(bus_t *bus, uint8_t secaddr) {
	return secaddr + bus->secaddr_offset;
}

#endif

