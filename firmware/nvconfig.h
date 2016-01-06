/************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2014 Andre Fachat, Nils Eilers

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

*************************************************************************/

#ifndef NVCONFIG_H
#define NVCONFIG_H

#include <inttypes.h>
#include <stdbool.h>
#include "rtconfig.h"

// Those hw-dependent functions should get defined in
// <arch>/nvconfighw.c || <device>/nvconfighw.c
uint8_t nv_read_byte(uint16_t addr);
bool nv_write_byte(uint16_t addr, uint8_t byte);

#ifdef HAS_EEPROM

bool nv_save_config(const rtconfig_t * rtc);
bool nv_restore_config(rtconfig_t * rtc);
bool nv_save_common_config(void);
bool nv_restore_common_config(void);

#else

static bool nv_save_config(const rtconfig_t * rtc)
{
	return true;
}

static bool nv_restore_config(rtconfig_t * rtc)
{
	return true;
}

static bool nv_save_common_config(void)
{
	return true;
}

static bool nv_restore_common_config(void)
{
	return true;
}

#endif				// HAS_EEPROM

#endif				// NVCONFIG_H
