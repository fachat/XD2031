/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2012 Andre Fachat

    Derived from:
    OS/A65 Version 1.3.12
    Multitasking Operating System for 6502 Computers
    Copyright (C) 1989-1997 Andre Fachat

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

#ifndef DELAY_H
#define DELAY_H

#include "delayhw.h"

// this is in main.c, but as it is used for delays, it is defined here
void main_delay();

static inline void delayms(uint8_t t)
{
	uint8_t ms = t;
	do {
		main_delay();
		delayhw_ms(1);
		ms--;
	}
	while (ms > 0);
}

// currently the only one that requires the 
#ifndef TIMER_TCA
static inline void delayus(uint8_t us)
{
	delayhw_us(us);
}
#endif

#endif
