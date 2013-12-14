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

#ifndef DELAYHW_H
#define DELAYHW_H

#include "config.h"         // F_CPU
#define __DELAY_BACKWARD_COMPATIBLE__
#include <util/delay.h>

// note that using the static inline versions below breaks the timing!
#define	delayhw_ms(a)	_delay_ms(a)
#define	delayhw_us(a)	_delay_us(a)

//static inline void delayms(uint16_t len) {
//	_delay_ms(len);
//}

//static inline void delayus(uint16_t len) {
//	_delay_us(len);
//}

#endif
