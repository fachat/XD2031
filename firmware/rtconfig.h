
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

#ifndef RTCONFIG_H
#define RTCONFIG_H

#include "errors.h"

typedef struct {
	uint8_t		device_address;		// current unit number
	uint8_t		last_used_drive;	// init with 0
} rtconfig_t;

// initialize a runtime config block
void rtconfig_init(rtconfig_t *rtc, uint8_t devaddr);

// set from an X command
errno_t rtconfig_set(rtconfig_t *rtc, const char *cmd);

#endif
