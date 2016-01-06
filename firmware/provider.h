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
 * definitions for a backend provider (of which currently only a UART provider
 * exists.
 */

#ifndef PROVIDER_H
#define	PROVIDER_H

#include <inttypes.h>

#include "packet.h"
#include "errormsg.h"
#include "charconvert.h"

// all 10 drives can be used
#define	MAX_DRIVES	10

typedef struct {
	// get a new void data pointer to be given to submit() for each ASSIGN
	void *(*prov_assign) (uint8_t drive, const char *parameter);
	// free the ASSIGN-related data structure
	void (*prov_free) (void *);
	// get the (currently used) character set for name conversion
	// use the same void* from assign to distinguish different channels (endpoints)
	 charset_t(*charset) (void *);
	// set a new charset
	void (*set_charset) (void *, charset_t new_charset);
	// submit a fire-and-forget packet (log_*, terminal)
	void (*submit) (void *pdata, packet_t * buf);
	// submit a request/response packet; call the callback function when the 
	// response is received; If callback returns != 0 then the call is kept open,
	// and further responses can be received
	void (*submit_call) (void *pdata, int8_t channelno, packet_t * txbuf,
			     packet_t * rxbuf,
			     uint8_t(*callback) (int8_t channelno,
						 int8_t errnum,
						 packet_t * packet));
	// convert the directory entry from the provider to the CBM codepage
	// return -1 if packet is too small to hold converted value
	// note: first pointer argument is endpoint_t* (!)
	 int8_t(*directory_converter) (void *ep, packet_t * p, uint8_t);
	// channel_get shortcut into provider (where applicable)
	 int8_t(*channel_get) (void *pdata, int8_t channelno,
			       uint8_t * data, uint8_t * iseof, int8_t * err,
			       uint8_t preload);
	// channel_put shortcut into provider (where applicable)
	 int8_t(*channel_put) (void *pdata, int8_t channelno,
			       char c, uint8_t forceflush);
} provider_t;

typedef struct {
	provider_t *provider;
	void *provdata;
} endpoint_t;

int8_t provider_assign(uint8_t drive, const char *name, const char *assign_to);

endpoint_t *provider_lookup(uint8_t drive, const char *drivename);

uint8_t provider_register(const char *name, provider_t * provider);

void provider_set_default(provider_t * prov, void *epdata);

void provider_init(void);

#endif
