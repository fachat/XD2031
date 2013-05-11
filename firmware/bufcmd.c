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
#include "packet.h"

#include "debug.h"


#define	DEBUG_USER
#define	DEBUG_BLOCK


// place for command, channel, drive, track sector 
#define	CMD_BUFFER_LENGTH	5

static char buf[CMD_BUFFER_LENGTH];
static packet_t cmdpack;
static packet_t datapack;
static uint8_t cbstat;
static int8_t cberr;

// ----------------------------------------------------------------------------------
// buffer handling (#-file, U1/U2/B-W/B-R/B-P
// we only have a single buffer

static uint8_t current_chan = -1;
static uint8_t direct_buffer[256];
static uint8_t buffer_rptr = 0;
static uint8_t buffer_wptr = 0;

// ----------------------------------------------------------------------------------
// provider for direct files

static void submit_call(void *pdata, int8_t channelno, packet_t *txbuf, packet_t *rxbuf,
                uint8_t (*callback)(int8_t channelno, int8_t errnum)) {

	debug_printf("submit call for direct file, chan=%d, cmd=%d, len=%d\n", 
		channelno, txbuf->type, txbuf->len);

	uint8_t rtype = FS_REPLY;

	switch(txbuf->type) {
	case FS_OPEN_DIRECT:
		buffer_rptr = 0;
		buffer_wptr = 0;
		rxbuf->wp = 1;
		rxbuf->buffer[0] = ERROR_OK;
		rtype = FS_REPLY;
		break;
	case FS_READ:
		debug_printf("rptr=%d, 1st data=%d (%02x)\n", buffer_rptr, 
				direct_buffer[buffer_rptr], direct_buffer[buffer_rptr]);
		rxbuf->buffer[0] = direct_buffer[buffer_rptr];
		rxbuf->wp = 1;
		buffer_wptr = buffer_rptr;
		if (buffer_rptr == 255) {
			rtype = FS_EOF;
		} else {
			rtype = FS_WRITE;
			buffer_rptr ++;
		}
		break;
	case FS_WRITE:
	case FS_EOF:
		// TODO
		break;
	}
	rxbuf->type = rtype;
	callback(channelno, 0);	
}


static provider_t provider = {
	NULL,			// prov_assign
	NULL,			// prov_free
	NULL,			// submit
	submit_call,		// submit_call
	NULL,			// directory_converter
	NULL			// to_provider
};

static endpoint_t endpoint = {
	&provider,
	NULL
};

endpoint_t *bufcmd_provider(void) {
	return &endpoint;
}

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

// ----------------------------------------------------------------------------------
// buffer handling (#-file, U1/U2/B-W/B-R/B-P

void bufcmd_init() {
	current_chan = -1;
}

uint8_t bufcmd_read_buffer(uint8_t channel_no, endpoint_t *endpoint, uint16_t receive_nbytes) {

	uint16_t restlength = receive_nbytes;

	uint8_t ptype = FS_REPLY;

	while (ptype != FS_EOF && restlength != 0) {

                packet_init(&datapack, 128, direct_buffer + receive_nbytes - restlength);
                packet_init(&cmdpack, CMD_BUFFER_LENGTH, (uint8_t*) buf);
                packet_set_filled(&cmdpack, channel_no, FS_READ, 0);
	
		cbstat = 0;
                endpoint->provider->submit_call(NULL, channel_no, 
			&cmdpack, &datapack, callback);

		cmd_wait_cb();

		ptype = packet_get_type(&datapack);

debug_printf("Received block data packet of %d bytes (eof=%d)\n", 
					datapack.wp, datapack.type);

		if (ptype == FS_REPLY) {
			// error (should not happen though)
			return packet_get_buffer(&datapack)[0];
		} else
		if (ptype == FS_WRITE || ptype == FS_EOF) {

			restlength -= packet_get_contentlen(&datapack);
		}
	}

	if (restlength > 0) {
		// received short package
		term_printf("RECEIVED SHORT BUFFER, EXPECTED %d, GOT %d\n", 256, 256-restlength);
	}

	buffer_rptr = 0;
	buffer_wptr =(receive_nbytes - restlength) & 0xff;

	return ERROR_OK;
}

int8_t bufcmd_open_direct(uint8_t channel_no, bus_t *bus, errormsg_t *errormsg,
                        void (*callback)(int8_t errnum, uint8_t *rxdata), uint8_t *name) {

	if (current_chan >= 0 && current_chan != channel_no) {
		set_error(errormsg, ERROR_NO_CHANNEL);
		return -1;
	}

	// reserve buffer
	current_chan = channel_no;
	return -1;
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
	
        int ichan;
        int drive;
        int track;
        int sector;

	uint8_t rv = ERROR_SYNTAX_UNKNOWN;
        uint8_t channel;

	switch(cmd & 0x0f) {
	case 1:		// U1
	case 2:		// U2

        	rv = sscanf(pars, "%d %d %d %d", &ichan, &drive, &track, &sector);
        	if (rv != 4) {
                	return ERROR_SYNTAX_INVAL;
        	}
		
		channel = bus_secaddr_adjust(bus, ichan);

			// read/write sector
#ifdef DEBUG_USER
		debug_printf("U1/2: sector ch=%d, dr=%d, tr=%d, se=%d\n", 
				channel, drive, track, sector);
#endif
			
		channel_flush(channel);

		buf[0] = drive;			// first for provider dispatch on server
		buf[1] = (cmd & 0x01) ? FS_BLOCK_U1 : FS_BLOCK_U2;
		buf[2] = track;
		buf[3] = sector;
		buf[4] = channel;		// extra compared to B-A/B-F

		packet_init(&cmdpack, CMD_BUFFER_LENGTH, (uint8_t*) buf);
		// FS_DIRECT packet with 5 data bytes, sent on cmd channel
		packet_set_filled(&cmdpack, bus_secaddr_adjust(bus, CMD_SECADDR), FS_DIRECT, 5);

		endpoint_t *endpoint = provider_lookup(drive, NULL);
		
		if (endpoint != NULL) {	
			cbstat = 0;
			endpoint->provider->submit_call(NULL, bus_secaddr_adjust(bus, CMD_SECADDR), 
							&cmdpack, &cmdpack, callback);

			rv = cmd_wait_cb();
debug_printf("Sent command - got: %d\n", rv);

			if (rv == ERROR_OK) {
				if (cmd & 0x01) {
					// U1
					rv = bufcmd_read_buffer(channel, endpoint, 256);
				} else {
					// U2
					// TODO
				}
			}
		} else {
			return ERROR_DRIVE_NOT_READY;
		}
		
		break;
	default:
		break;
	}

	return rv;
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


