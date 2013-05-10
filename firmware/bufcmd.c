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
	cberr = buf[0];
	cbstat = 1;
	return 0;
}

static uint8_t cmd_wait_cb() {
	// TODO: check if not just returning -1 would suffice
	while (cbstat == 0) {
		delayms(1);
		main_delay();
	}

debug_printf("cb result: %d\n", cberr);
	return cberr;
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

				return cmd_wait_cb();
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
 */
uint8_t cmd_block_allocfree(bus_t *bus, char *cmdbuf, uint8_t fscmd, errormsg_t *error) {
	
	int drive;
	int track;
	int sector;

	uint8_t channel = bus_secaddr_adjust(bus, 15);
	
	uint8_t rv;

	rv = sscanf(cmdbuf, "%d %d %d", &drive, &track, &sector);
	if (rv != 3) {
		return ERROR_SYNTAX_INVAL;
	}

        buf[0] = drive;		// comes first similar to other FS_* cmds
        buf[1] = fscmd;
        buf[2] = track;
        buf[3] = sector;

	packet_init(&cmdpack, CMD_BUFFER_LENGTH, (uint8_t*) buf);
	packet_set_filled(&cmdpack, channel, FS_DIRECT, 4);

        endpoint_t *endpoint = provider_lookup(drive, NULL);

        if (endpoint != NULL) {
        	cbstat = 0;
                endpoint->provider->submit_call(NULL, channel,
                                            &cmdpack, &cmdpack, callback);

		rv = cmd_wait_cb();

		// TODO: buf[1]/buf[2] contain the T&S - need to get that into the error
		debug_printf("block_allocfree: t&s=%d, %d\n", buf[1], buf[2]);

		set_error_ts(error, rv, buf[1], buf[2]);

		// means: don't wait, error is already set
		return -1;
	}
        return ERROR_DRIVE_NOT_READY;
}

/*
 *
 * B-R, B-W, B-P, B-A, B-F
 *
 * cmdbuf pointer includes the full command, i.e. with the "U" in front.
 * 
 * returns ERROR_OK/ERROR_* on direct ok/error, or -1 if a callback has been submitted
 */
uint8_t cmd_block(bus_t *bus, char *cmdbuf, errormsg_t *error) {

#ifdef DEBUG_BLOCK
	debug_printf("cbm_block: %s\n", cmdbuf);
#endif
	// identify command - just look for the '-' and take the following char
	while (*cmdbuf != 0 && *cmdbuf != '-') {
		cmdbuf++;
	}
	if (*cmdbuf == 0) {
		return ERROR_SYNTAX_UNKNOWN;
	}
	cmdbuf++;

	char cchar = *cmdbuf;
	
	while (*cmdbuf != 0 && *cmdbuf != ':') {
		cmdbuf++;
	}
	if (*cmdbuf == 0) {
		return ERROR_SYNTAX_UNKNOWN;
	}
	// char after the ':'
	cmdbuf++;	
		
	switch(cchar) {
	case 'A':
		return cmd_block_allocfree(bus, cmdbuf, FS_BLOCK_BA, error);
		break;
	case 'F':
		return cmd_block_allocfree(bus, cmdbuf, FS_BLOCK_BF, error);
		break;
	default:
		break;
	}

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

