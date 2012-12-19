/* 
    XD-2031 - Serial line file server for CBMs
    Copyright (C) 2012  Andre Fachat

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

*/

#include <errno.h>
#include <stdlib.h>

#include "bus.h"
#include "errormsg.h"
#include "channel.h"
#include "provider.h"

#include "debug.h"


#define	DEBUG_USER
#define	DEBUG_BLOCK


static struct {
	int8_t 		channel;
	int8_t		drive;
	int8_t		track;
	int8_t		sector;
} cmdinfo;

// place for command, drive, track sector; channel is part of packet header
#define	CMD_BUFFER_LENGTH	4

static char buf[CMD_BUFFER_LENGTH];
static packet_t cmdpack;
static uint8_t cbstat;
static int8_t cberr;

static uint8_t parse_cmdinfo(char *buf);

/**
 * command callback
 */
static uint8_t callback(int8_t channelno, int8_t errnum) {
	cberr = errnum;
	cbstat = 1;
	return 0;
}

/**
 * user commands
 *
 * U1, U2
 *
 * cmdbuf pointer includes the full command, i.e. with the "U" in front.
 * 
 * returns ERROR_OK/ERROR_* on direct ok/error, or -1 if a callback has been submitted
 */
uint8_t cmd_user(bus_t *bus, char *cmdbuf, errormsg_t *error) { 
	char *pars;	
	uint8_t er = ERROR_OK;
	uint8_t cmd = cmdbuf[1];

#ifdef DEBUG_USER
	debug_printf("cmd_user (%02x): %s\n", cmd, cmdbuf);
#endif

	// find start of parameter string into pars
	pars = cmdbuf+2;
	while (*pars && (*pars != ':')) {
		pars++;
	}
	if (!*pars) {
		return ERROR_SYNTAX_INVAL;
	}
	pars++;
	
	switch(cmd & 0x0f) {
	case 1:		// U1
		er = parse_cmdinfo(pars);
		if (er == ERROR_OK) {
			// read sector
#ifdef DEBUG_USER
			debug_printf("U1: read sector ch=%d, dr=%d, tr=%d, se=%d\n", 
				cmdinfo.channel, cmdinfo.drive, cmdinfo.track, cmdinfo.sector);
#endif
			cmdinfo.channel = bus_secaddr_adjust(bus, cmdinfo.channel);
			
			channel_flush(cmdinfo.channel);

			buf[0] = FS_BLOCK_U1;
			buf[1] = cmdinfo.drive;
			buf[2] = cmdinfo.track;
			buf[3] = cmdinfo.sector;
			packet_init(&cmdpack, CMD_BUFFER_LENGTH, (uint8_t*) buf);
			packet_set_filled(&cmdpack, cmdinfo.channel, FS_BLOCK, 4);

			endpoint_t *endpoint = provider_lookup(cmdinfo.drive, NULL);
		
			if (endpoint != NULL) {	
				cbstat = 0;
				endpoint->provider->submit_call(NULL, cmdinfo.channel, 
								&cmdpack, &cmdpack, callback);

				while (cbstat == 0) {
					delayms(1);
					main_delay();
				}

				return cberr;
			} else {
				return ERROR_DRIVE_NOT_READY;
			}
		}
		
		
		break;
	case 2:		// U2
		break;
	default:
		break;
	}

	return ERROR_SYNTAX_UNKNOWN;
}


/**
 * block commands
 *
 * B-R, B-W, B-P
 *
 * cmdbuf pointer includes the full command, i.e. with the "U" in front.
 * 
 * returns ERROR_OK/ERROR_* on direct ok/error, or -1 if a callback has been submitted
 */
uint8_t cmd_block(bus_t *bus, char *cmdbuf, errormsg_t *error) {

	return ERROR_SYNTAX_UNKNOWN;
}

/**
 * parse the B-R/W/E, U1/2 "channel drive track sector" parameters
 *
 * return ERROR_* on error (short string, format, ...)
 * or 0 on success.
 */
static uint8_t parse_cmdinfo(char *buf) {
	char *next;
	unsigned long int val;

#ifdef DEBUG_USER
	debug_printf("parse_cmdinfo: %s\n", buf);
#endif

	cmdinfo.channel = -1;

	errno = 0;	
	val = strtoul(buf, &next, 10);
	cmdinfo.channel = val;

	if (next != NULL) {
		val = strtoul(next, &next, 10);
		cmdinfo.drive = val;

		if (next != NULL) {
			val = strtoul(next, &next, 10);
			cmdinfo.track = val;

			if (next != NULL) {
				val = strtoul(next, &next, 10);
				cmdinfo.sector = val;
			} else {
				return ERROR_SYNTAX_INVAL;
			}
		} else {
			return ERROR_SYNTAX_INVAL;
		}
	} else {
		return ERROR_SYNTAX_INVAL;
	}

	if (errno != 0) {
		return ERROR_SYNTAX_INVAL;
	}
	return ERROR_OK;
}

