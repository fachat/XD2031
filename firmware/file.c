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
#include "serial.h"
#include "wireformat.h"

#include "debug.h"
#include "led.h"

#undef DEBUG_FILE

#define	MAX_ACTIVE_OPEN		2
#define	OPEN_RX_DATA_LEN	2

static void _file_open_callback(int8_t channelno, int8_t errnum);

typedef struct {
	int8_t 		channel_no;
	packet_t	txbuf;
	packet_t	rxbuf;
	uint8_t		rxdata[OPEN_RX_DATA_LEN];
	void		(*callback)(int8_t errnum, uint8_t *rxdata);
} open_t;

static volatile open_t active[MAX_ACTIVE_OPEN];

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
int8_t file_open(uint8_t channel_no, cmd_t *command, errormsg_t *errormsg, void (*callback)(int8_t errnum, uint8_t *rxdata), uint8_t is_save) {


#if DEBUG_FILE
	debug_putps("OPEN FILE: FOR CHAN:");
	debug_puthex(channel_no);
	debug_putps(" WITH NAME: ");
	debug_puts((char*)&(command->command_buffer));
	debug_putcrlf();
#endif

	// note: in a preemtive env, the following would have to be protected
	// to be atomic as we modify static variables

	parse_filename(command, &nameinfo, 0);

	// post-parse

	if (nameinfo.cmd != CMD_NONE && nameinfo.cmd != CMD_DIR) {
		// command name during open
		// this is in fact ignored by CBM DOS as checked with VICE's true drive emulation
		debug_puts("NO CORRECT CMD: "); debug_puts(command_to_name(nameinfo.cmd)); debug_putcrlf();
		nameinfo.cmd = 0;
	}
	if (nameinfo.type != 0 && nameinfo.type != 'S' && nameinfo.type != 'P') {
		// not set, or set as not sequential and not program
		debug_puts("UNKOWN FILE TYPE: "); debug_putc(nameinfo.type); debug_putcrlf();
		set_error(errormsg, ERROR_FILE_TYPE_MISMATCH);
		return -1;
	}
	if (nameinfo.access != 0 && nameinfo.access != 'W' && nameinfo.access != 'R'
			&& nameinfo.access != 'A') {
		debug_puts("UNKOWN FILE ACCESS TYPE "); debug_putc(nameinfo.access); debug_putcrlf();
		// not set, or set as not read, write, or append
		set_error(errormsg, ERROR_SYNTAX_UNKNOWN);
		return -1;
	}
	if (nameinfo.cmd == CMD_DIR && (nameinfo.access != 0 && nameinfo.access != 'R')) {
		// trying to write to a directory
		debug_puts("WRITE TO DIRECTORY!"); debug_putcrlf();
		set_error(errormsg, ERROR_FILE_EXISTS);
		return -1;
	}

	uint8_t type = FS_OPEN_RD;
	// either ",W" or secondary address is one, i.e. save
	if (nameinfo.access == 'W' || is_save) type = FS_OPEN_WR;
	if (nameinfo.access == 'A') type = FS_OPEN_AP;
	if (nameinfo.cmd == CMD_DIR) type = FS_OPEN_DR;


	return file_submit_call(channel_no, type, errormsg, callback);

}

uint8_t file_submit_call(uint8_t channel_no, uint8_t type, errormsg_t *errormsg,
		void (*callback)(int8_t errnum, uint8_t *rxdata)) {


	// check for default drive (here is the place to set the last used one)
	if (nameinfo.drive == 0xff) {
		// there currently only is a single drive, 0
		nameinfo.drive = 0;
		// TODO: fix this hack!
		nameinfo.name[0] = 0;
	}

	// check filename

	// TODO: keep a local end point registry just in case...
	// So the drive number could select the provider (which is currently fixed
	// as serial_provider below
	// For now just allow all drive numbers to be passed as endpoint numbers
//	if (nameinfo.drive != 0) {
//		// here would be a place to handle multiple drives in a device..
//		debug_puts("ILLEGAL DRIVE: "); debug_putc(0x30+nameinfo.drive); debug_putcrlf();
//		set_error(errormsg, ERROR_DRIVE_NOT_READY);
//		return -1;
//	}

	// here is the place to plug in other file system providers,
	// like SD-Card, or even an outgoing IEC or IEEE, to convert between
	// the two bus systems. This could be done depending on the drive number
	// and be managed with the ASSIGN call.
	provider_t *provider = &serial_provider;

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
		set_error(errormsg, ERROR_NO_CHANNEL);
		return -1;
	}

	packet_init(&activeslot->txbuf, nameinfo.namelen, nameinfo.name);
	packet_set_filled(&activeslot->txbuf, channel_no, type, nameinfo.namelen);

	if (provider->to_provider != NULL && provider->to_provider(&activeslot->txbuf) < 0) {
		// converting the file name to the provider exceeded the buffer space
		debug_puts("NAME CONVERSION EXCEEDS BUFFER!");
		debug_putcrlf();
		set_error(errormsg, ERROR_SYNTAX_NONAME);
		return -1;
	}

	// open channel
	uint8_t writetype = (type == FS_OPEN_WR || type == FS_OPEN_AP) ? 1 : 0;
	int8_t (*converter)(packet_t*) = (type == FS_OPEN_DR) ? (provider->directory_converter) : NULL;

	channel_t *channel = channel_find(channel_no);
	if (channel != NULL) {
		debug_puts("FILE OPEN ERROR");
		debug_putcrlf();
		set_error(errormsg, ERROR_NO_CHANNEL);
		// clean up
		channel_close(channel_no);
		return -1;
	}

	int8_t e = channel_open(channel_no, writetype, provider, converter);
	if (e < 0) {
		debug_puts("E="); debug_puthex(e); debug_putcrlf();
		set_error(errormsg, ERROR_NO_CHANNEL);
		return -1;
	}

	activeslot->callback = callback;

	// no more error here, just the submit.
	// so callers can be sure if this function returns <0, they do not need
	// to close the channel, as it has not been opened
	// If this function returns 0, a callback must be received and handled,
	// and the channel is already opened.
		
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
				active[i].callback(ERROR_DRIVE_NOT_READY, NULL);
			} else {
				// we did receive the reply packet
				// TODO: translate into errormsg code
				// NOTE: rxdata[0] is actually rxdata[FSP_DATA], as first
				// byte of reply packet is the error number
				active[i].callback(active[i].rxdata[0], active[i].rxdata);
			}
			active[i].channel_no = -1;
			break;
		}
	}
}



