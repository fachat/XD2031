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
 * definitions for a backend provider (of which currently only a UART provider
 * exists.
 */


#ifndef PROVIDER_H
#define	PROVIDER_H

#include <inttypes.h>
#include "packet.h"

typedef struct {
	void (*submit)(packet_t *buf);
	void (*submit_call)(int8_t channelno, packet_t *txbuf, packet_t *rxbuf,
                void (*callback)(int8_t channelno, int8_t errnum));
	// convert the directory entry from the provider to the CBM codepage
	// return -1 if packet is too small to hold converted value
	int8_t (*directory_converter)(packet_t *p);
	// convert a packet from CBM codepage to provider
	// used only for open and commands!
	// return -1 if packet is too small to hold converted value
	int8_t (*to_provider)(packet_t *p);
} provider_t;

#endif

