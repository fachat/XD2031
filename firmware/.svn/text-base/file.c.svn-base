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

#include <inttypes.h>

#include "packet.h"
#include "channel.h"
#include "file.h"
#include "name.h"
#include "uart2.h"
#include "oa1fs.h"

#include "debug.h"
#include "led.h"

#define	MAX_ACTIVE_OPEN		2
#define	OPEN_RX_DATA_LEN	2

static void _file_open_callback(int8_t channelno, int8_t errnum);

typedef struct {
	int8_t 		channel_no;
	packet_t	txbuf;
	packet_t	rxbuf;
	uint8_t		rxdata[OPEN_RX_DATA_LEN];
	void		(*callback)(int8_t errnum);
} open_t;

static volatile open_t active[MAX_ACTIVE_OPEN];
static nameinfo_t nameinfo;

void file_init(void) {
	for (uint8_t i = 0; i < MAX_ACTIVE_OPEN; i++) {
		active[i].channel_no = -1;
	}
}

// opens the file, registers an error code in command->error if necessary
// If the open was successful, setup a channel for the given channel number
// (note: explicitely not secondary device number, as IEEE and IEC in parallel
// use overlapping numbers, so they may be shifted or similar to avoid clashes)
//
// The command buffer is used as transmit buffer, so it must not be overwritten
// until the open has been sent.
//
int8_t file_open(uint8_t channel_no, cmd_t *command, void (*callback)(int8_t errnum)) {


#if DEBUG
	debug_putps("OPEN FILE: FOR CHAN:");
	debug_puthex(channel_no);
	debug_putps(" WITH NAME: ");
	debug_puts((char*)&(command->command_buffer));
	debug_putcrlf();
#endif

	// note: in a preemtive env, the following would have to be protected
	// to be atomic as we modify static variables

	parse_filename(command, &nameinfo);
	// check filename
	if (nameinfo.cmd != 0 && nameinfo.cmd != '$') {
		// command name during open
		// this is in fact ignored by CBM DOS as checked with VICE's true drive emulation
		debug_puts("NO CORRECT CMD: "); debug_putc(nameinfo.cmd); debug_putcrlf();
		nameinfo.cmd = 0;
	}
	if (nameinfo.drive != 0) {
		// here would be a place to handle multiple drives in a device..
		debug_puts("ILLEGAL DRIVE: "); debug_putc(0x30+nameinfo.drive); debug_putcrlf();
		set_error(command->errormsg, ERROR_DRIVE_NOT_READY);
		return -1;
	}
	if (nameinfo.type != 0 && nameinfo.type != 'S' && nameinfo.type != 'P') {
		// not set, or set as not sequential and not program
		debug_puts("UNKOWN FILE TYPE: "); debug_putc(nameinfo.type); debug_putcrlf();
		set_error(command->errormsg, ERROR_FILE_TYPE_MISMATCH);
		return -1;
	}
	if (nameinfo.access != 0 && nameinfo.access != 'W' && nameinfo.access != 'R'
			&& nameinfo.access != 'A') {
		debug_puts("UNKOWN FILE ACCESS TYPE "); debug_putc(nameinfo.access); debug_putcrlf();
		// not set, or set as not read, write, or append
		set_error(command->errormsg, ERROR_SYNTAX_UNKNOWN);
		return -1;
	}
	if (nameinfo.cmd == '$' && (nameinfo.access != 0 && nameinfo.access != 'R')) {
		// trying to write to a directory
		debug_puts("WRITE TO DIRECTORY!"); debug_putcrlf();
		set_error(command->errormsg, ERROR_FILE_EXISTS);
		return -1;
	}

	// find open slot
	//int8_t slot = -1;
	open_t *activeslot = NULL;
	for (uint8_t i = 0; i < MAX_ACTIVE_OPEN; i++) {
		if (active[i].channel_no < 0) {
			//slot = i;
			activeslot = &active[i];
			break;
		}
	}
	//if (slot < 0) {
	if (activeslot == NULL) {
		debug_puts("NO OPEN SLOT FOR OPEN!");
		debug_putcrlf();
		set_error(command->errormsg, ERROR_NO_CHANNEL);
		return -1;
	}

	uint8_t type = FS_OPEN_RD;
	if (nameinfo.access == 'W') type = FS_OPEN_WR;
	if (nameinfo.access == 'A') type = FS_OPEN_AP;
	if (nameinfo.cmd == '$') type = FS_OPEN_DR;

	// here is the place to plug in other file system providers,
	// like SD-Card, or even an outgoing IEC or IEEE, to convert between
	// the two bus systems
	provider_t *provider = &uart_provider;

	// prepare request data
	packet_init(&activeslot->txbuf, nameinfo.namelen, nameinfo.name);
	packet_set_filled(&activeslot->txbuf, channel_no, type, nameinfo.namelen);

	if (provider->to_provider != NULL && provider->to_provider(&activeslot->txbuf) < 0) {
		// converting the file name to the provider exceeded the buffer space
		debug_puts("NAME CONVERSION EXCEEDS BUFFER!");
		debug_putcrlf();
		set_error(command->errormsg, ERROR_SYNTAX_NONAME);
		return -1;
	}

	// open channel
	uint8_t writetype = (type == FS_OPEN_RD || type == FS_OPEN_DR) ? 0 : 1;
	int8_t (*converter)(volatile packet_t*) = (type == FS_OPEN_DR) ? (provider->directory_converter) : NULL;

	int8_t e = channel_open(channel_no, writetype, provider, converter);
	if (e < 0) {
		debug_puts("E="); debug_puthex(e); debug_putcrlf();
		set_error(command->errormsg, ERROR_NO_CHANNEL);
		return -1;
	}

	activeslot->callback = callback;

	// no more error here
		
	activeslot->channel_no = channel_no;

	// prepare response buffer
	packet_init(&activeslot->rxbuf, OPEN_RX_DATA_LEN, activeslot->rxdata);
	
	provider->submit_call(channel_no, &activeslot->txbuf, &activeslot->rxbuf, _file_open_callback);

	return 0;
}

static void _file_open_callback(int8_t channelno, int8_t errnum) {

	// callback to opener
	// free data structure for next open	
	for (uint8_t i = 0; i < MAX_ACTIVE_OPEN; i++) {
		if (active[i].channel_no == channelno) {
			if (errnum < 0) {
				// we did not receive the packet!
				active[i].callback(ERROR_DRIVE_NOT_READY);
			} else {
				// we did receive the reply packet
				// TODO: translate into errormsg code
				// NOTE: rxdata[0] is actually rxdata[FSP_DATA], as first
				// byte of reply packet is the error number
				active[i].callback(active[i].rxdata[0]);
			}
			active[i].channel_no = -1;
			break;
		}
	}
}



