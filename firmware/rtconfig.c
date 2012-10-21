
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
 * This file implements the central communication channels between the
 * IEEE layer and the provider layers
 */

#include <ctype.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "rtconfig.h"
#include "errors.h"
#include "provider.h"	// MAX_DRIVES
#include "nvconfig.h"
#include "bus.h"	// get_default_device_address()
#include "system.h"	// reset_mcu()

#include "debug.h"

// initialize a runtime config block
void rtconfig_init(rtconfig_t *rtc, uint8_t devaddr) {
	// Default values
	rtc->device_address = devaddr;
	rtc->last_used_drive = 0;

	if(nv_restore_config(rtc)) nv_save_config(rtc);
}

// set from an X command
errno_t rtconfig_set(rtconfig_t *rtc, const char *cmd) {

	debug_printf("CMD:'%s'\n", cmd);

	errno_t er = ERROR_SYNTAX_UNKNOWN;

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
	case 'U':
		// look for "U=<unit number in ascii || binary>"
		ptr++;
		if (*ptr == '=') {
			ptr++;
			uint8_t devaddr = (*ptr);
			if (isdigit(*ptr)) devaddr = atoi(ptr);
			if (devaddr >= 4 && devaddr <= 30) {
				rtc->device_address = devaddr;
				er = ERROR_OK;
				debug_printf("SETTING UNIT# TO %d\n", devaddr);
			} else {
				er = ERROR_SYNTAX_INVAL;
				debug_printf("ERROR SETTING UNIT# TO %d\n", devaddr);
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
				er = ERROR_OK;
				debug_printf("SETTING DRIVE# TO %d\n", drv);
			}
		}
		break;
	case 'I':
		// INIT: restore default values
		rtconfig_init(rtc, get_default_device_address());
		er = ERROR_OK;
		debug_puts("RUNTIME CONFIG INITIALIZED\n");
	case 'W':
		// write runtime config to EEPROM
		nv_save_config(rtc);
		er = ERROR_OK;
		break;
	case 'R':
		if(!strcmp(ptr, "RESET")) {
			// reset everything
			reset_mcu();
		}
	default:
		break;
	}

	return er;
}


