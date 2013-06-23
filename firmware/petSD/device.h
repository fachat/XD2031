/**************************************************************************

    device.h -- device dependant definitions

    This file is part of XD-2031 -- Serial line filesystem server for CBMs

    Copyright (C) 2012 Andre Fachat <afachat@gmx.de>
    Copyrifht (C) 2012 Nils Eilers  <nils.eilers@gmx.de>

    XD-2031 is free software: you can redistribute it and/or
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

**************************************************************************/


#ifndef PETSD_H
#define PETSD_H

#include "hwdefines.h"

void device_init(void);

static inline void device_led_on (void) {
    LED_PORT |= _BV(LED_BIT);
}

static inline void device_active_led_on (void) {
    ACTIVE_LED_PORT |= _BV(ACTIVE_LED_BIT);
}

static inline void device_leds_off (void) {
    LED_PORT &= ~_BV(LED_BIT);
    ACTIVE_LED_PORT &= ~_BV(ACTIVE_LED_BIT);
}

#endif
