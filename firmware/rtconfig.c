
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
 * Runtime configuration per bus
 */

#include <ctype.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "device.h"
#include "rtconfig.h"
#include "rtconfig2.h"
#include "errors.h"
#include "provider.h"	// MAX_DRIVES
#include "nvconfig.h"
#include "bus.h"	// get_default_device_address()
#include "system.h"	// reset_mcu()

#include "debug.h"
#include "term.h"
#include "led.h"

#if HAS_EEPROM
#define	MAX_RTCONFIG	3
#else
#define	MAX_RTCONFIG	1
#endif

static void do_charset();

static endpoint_t *endpoint;

static rtconfig_t *rtcs[MAX_RTCONFIG];

// don't think this that belongs into rtconfig_t. We have multiple ports (iec vs. ieee)
// but only one server connection, which this describes.
// in the future there may be a port specific charset, e.g. to switch a directory
// output to ASCII instead of PETSCII
// TODO: save in NVRAM
static charset_t current_charset;

static int num_rtcs = 0;

// can be made smaller?
#define	OPT_BUFFER_LENGTH	16
#define	OUT_BUFFER_LENGTH	8		// place for "PETSCII"

static char buf[OPT_BUFFER_LENGTH];
static char outbuf[OUT_BUFFER_LENGTH];

static packet_t buspack;
static packet_t outpack;

void rtconfig_init(endpoint_t *_endpoint) {
	num_rtcs = 0;
	endpoint = _endpoint;
        // init the packet structs
        packet_init(&buspack, OPT_BUFFER_LENGTH, (uint8_t*)buf);
        packet_init(&outpack, OUT_BUFFER_LENGTH, (uint8_t*)outbuf);

	// initialize the server communication with PETSCII
	current_charset = CHARSET_PETSCII;
}

// initialize a runtime config block
void rtconfig_init_rtc(rtconfig_t *rtc, uint8_t devaddr) {
	// Default values
	rtc->device_address = devaddr;
	rtc->last_used_drive = 0;
	rtc->advanced_wildcards = false;
	rtc->errmsg_with_drive = true;
#ifdef HAS_EEPROM
	if(nv_restore_config(rtc)) nv_save_config(rtc);
#endif

	if (num_rtcs < MAX_RTCONFIG) {
		rtcs[num_rtcs] = rtc;
		num_rtcs++;
	} else {
		term_printf("too many rtconfigs!\n");
	}
}

/********************************************************************************/


// there shouldn't be much debug output, as sending it may invariably 
// receive the next option, triggering the option again. But it isn't
// re-entrant!
static uint8_t out_callback(int8_t channelno, int8_t errno, packet_t *rxpacket) {
	int8_t outrv;

        //debug_printf("setopt cb err=%d\n", errno);
        if (errno == CBM_ERROR_OK) {
                //debug_printf("rx command: %s\n", buf);

		uint8_t cmd = packet_get_type(rxpacket);

		switch(cmd) {
		case FS_REPLY:
			outrv = packet_get_buffer(rxpacket)[0];
			if (outrv != CBM_ERROR_OK) {
				// fallback to ASCII
				current_charset = CHARSET_ASCII;
			}	
			endpoint->provider->set_charset(endpoint->provdata, current_charset);
			break;
		}
        }
	device_unlock();
	// callback returns 1 to continue receiving on this channel
        return 0;
}

static void do_charset() {

	// set the communication charset to PETSCII
	strcpy(outbuf, cconv_charsetname(current_charset));

        // prepare FS_CHARSET packet
        packet_set_filled(&outpack, FSFD_CMD, FS_CHARSET, strlen(outbuf)+1);

	// send the FS_CHARSET packet
        endpoint->provider->submit_call_data(endpoint->provdata, FSFD_CMD, &outpack, &outpack, out_callback);

	// must not wait, as we may be in callback from FS_RESET (from server),
	// so serial_lock is set
}

static void do_setopt(char *buf, uint8_t len) {
	// find the correct rtconfig
	for (uint8_t i = 0; i < num_rtcs; i++) {
		rtconfig_t *rtc = rtcs[i];
		const char *name = rtc->name;

		uint8_t j;
		for (j = 0; j < len; j++) {
			if (name[j] == 0
				|| name[j] != buf[j]) {
				break;
			}
		}
		if ( buf[j] == ':') {
			// found it, now set the config
			buf[j] = 'X';
			rtconfig_set(rtc, buf+j);
			break;
		}
	}
}

// there shouldn't be much debug output, as sending it may invariably 
// receive the next option, triggering the option again. But it isn't
// re-entrant!
static uint8_t setopt_callback(int8_t channelno, int8_t errno, packet_t *rxpacket) {

        //debug_printf("setopt cb err=%d\n", errno);
        if (errno == CBM_ERROR_OK) {
                //debug_printf("rx command: %s\n", buf);

		uint8_t cmd = packet_get_type(rxpacket);
		uint8_t len = packet_get_contentlen(rxpacket);

		switch(cmd) {
		case FS_SETOPT:
			do_setopt(buf, len);
			break;
		case FS_RESET:
			rtconfig_pullconfig(0, NULL);
			break;
		}
        }
	// callback returns 1 to continue receiving on this channel
        return 1;
}


void rtconfig_pullconfig(int argc, const char *argv[]) {

	for (int i = 0; i < argc; i++) {
		if (argv[i][0] == '-' && argv[i][1] == 'X') {
			do_setopt((char*)argv[i]+2, strlen(argv[i]+2));
		}
	}

        // prepare FS_RESET packet
        packet_set_filled(&buspack, FSFD_SETOPT, FS_RESET, 0);

        // send request, receive in same buffer we sent from
        endpoint->provider->submit_call_data(endpoint->provdata, FSFD_SETOPT, &buspack, &buspack, setopt_callback);

        debug_printf("sent reset packet on fd %d, charset=%d\n", FSFD_SETOPT, current_charset);

	// send charset command
	do_charset();
}

/********************************************************************************/

// set from an X command
cbm_errno_t rtconfig_set(rtconfig_t *rtc, const char *cmd) {

	charset_t new_charset = -1;

	debug_printf("CMD:'%s'\n", cmd);

	cbm_errno_t er = CBM_ERROR_SYNTAX_UNKNOWN;

	const char *ptr = cmd;

	char c = *ptr;
	while (c != 0 && c !='X') {
		ptr++;
		c = *ptr;
	}

	do {
		ptr++;
		c = *ptr;
	} while (c == ' ');

	// c now contains the actual command
	switch(c) {
	case '*':
		// enable/disable the use of advanced wildcards
		// look for *=+ || *=-
		if (*++ptr == '=') {
			if (*++ptr == '+') {
				rtc->advanced_wildcards = true;
				er = CBM_ERROR_OK;
			} else if (*ptr == '-') {
				rtc->advanced_wildcards = false;
				er = CBM_ERROR_OK;
			}
			if (er == CBM_ERROR_OK) {
				debug_puts("ADVANCED WILDCARDS ");
				if (rtc->advanced_wildcards)
					debug_puts("EN");
				else
					debug_puts("DIS");
				debug_puts("ABLED\n");
			}
		}
		break;
	case 'E':
		// enable/disable the drive number on error messages
		// look for E=+ || E=-
		if (*++ptr == '=') {
			if (*++ptr == '+') {
				rtc->errmsg_with_drive = true;
				er = CBM_ERROR_OK;
			} else if (*ptr == '-') {
				rtc->errmsg_with_drive = false;
				er = CBM_ERROR_OK;
			}
			if (er == CBM_ERROR_OK) {
				debug_puts("ERRORS WITH DRIVE ");
				if (rtc->advanced_wildcards)
					debug_puts("EN");
				else
					debug_puts("DIS");
				debug_puts("ABLED\n");
			}
		}
		break;
	case 'U':
		// look for "U=<unit number in ascii || binary>"
		ptr++;
		if (*ptr == '=') {
			ptr++;
			uint8_t devaddr = (*ptr);
			if (isdigit(*ptr)) devaddr = atoi(ptr);
			if (devaddr >= 4 && devaddr <= 30) {
				rtc->device_address = devaddr;
				er = CBM_ERROR_OK;
				debug_printf("SETTING UNIT# TO %d ON %s\n", devaddr, rtc->name);
			} else {
				er = CBM_ERROR_SYNTAX_INVAL;
				debug_printf("ERROR SETTING UNIT# TO %d ON %s\n", devaddr, rtc->name);
			}
		}
		break;
	case 'D':
		// set default drive number
		// look for "D=<drive number in ascii || binary>"
		ptr++;
		if (*ptr == '=') {
			ptr++;
			uint8_t drv = (*ptr);
			if (isdigit(*ptr)) drv=atoi(ptr);
			if (drv < MAX_DRIVES) {
				rtc->last_used_drive = drv;
				er = CBM_ERROR_OK;
				debug_printf("SETTING DRIVE# TO %d ON %s\n", drv, rtc->name);
			}
		}
		break;
	case 'I':
		// INIT: restore default values
		rtconfig_init_rtc(rtc, get_default_device_address());
		er = CBM_ERROR_OK;
		debug_puts("RUNTIME CONFIG INITIALIZED\n");
		break;
#ifdef HAS_EEPROM
	case 'W':
		// write runtime config to EEPROM
		nv_save_common_config();
		er = nv_save_config(rtc);
		break;
#endif
	case 'R':
		if(!strncmp(ptr, "RESET", 5)) {		// ignore CR or whatever follows
			// reset everything
			reset_mcu();
		}
		break;
	case 'C':
		// TEST code
		// look for "C=<charsetname>"
		ptr++;
		if (*ptr == '=') {
			ptr++;
			new_charset = cconv_getcharset(ptr);
			if (new_charset >= 0) {
				current_charset = new_charset;
				do_charset();
				er = CBM_ERROR_OK;
			}
		}
		break;		
	default:
		break;
	}

	return er;
}

