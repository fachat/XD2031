/**************************************************************************

    config.h -- common configurations

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


#ifndef CONFIG_H
#define CONFIG_H

//#define DEV_ADDR	9	// default device address

#define F_CPU		18432000UL

// LED configuration for single LED HW / error LED
#define LED_DDR		DDRD
#define LED_PORT	PORTD
#define LED_BIT		PD6	// red LED (may be part of bi-color LED)

// LED configuration for separate activity LED
// Leave ACTIVE_LED_DDR undefined for HW without activity LED
#define ACTIVE_LED_DDR	DDRD
#define ACTIVE_LED_PORT	PORTD
#define ACTIVE_LED_BIT	PD5	// green LED (may be part of bi-color LED)

// buffer sizes
#define CONFIG_COMMAND_BUFFER_SIZE      120
#define CONFIG_ERROR_BUFFER_SIZE        46

#endif

