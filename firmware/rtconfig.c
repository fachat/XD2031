
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

#include "rtconfig.h"
#include "errors.h"

#include "debug.h"

// initialize a runtime config block
void rtconfig_init(rtconfig_t *rtc, uint8_t devaddr) {
	rtc->device_address = devaddr;
	rtc->last_used_drive = 0;
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
		// look for "U=<unit number in ascii>"
		ptr++;
		if (*ptr == '=') {
			ptr++;
			if (isdigit(*ptr)) {
				uint8_t devaddr = (*ptr) & 0x0f;
				if (devaddr >= 4 && devaddr < 15) {
					rtc->device_address = devaddr;
					er = ERROR_OK;
					debug_printf("SETTING UNIT# TO %d\n", devaddr);
				}
			}
		}
		break;
	case 'D':
		// set default drive number
		// look for "D=<drive number in ascii>"
		ptr++;
		if (*ptr == '=') {
			ptr++;
			if (isdigit(*ptr)) {
				uint8_t drv = (*ptr) & 0x0f;
				rtc->last_used_drive = drv;
				er = ERROR_OK;
				debug_printf("SETTING DRIVE# TO %d\n", drv);
			}
		}
		break;
	default:
		break;
	}

	return er;
}

