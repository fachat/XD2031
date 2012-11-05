/*
    XD-2031 - Serial line filesystem server for CBMs 
    Copyright (C) 2012  Andre Fachat (afachat@gmx.de)
    Copyright (C) 2012 Nils Eilers (nils.eilers@gmx.de)

    petSD specific device initialization

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

*/

#include "device.h"
#include "ieeehw.h"
#include "ieee.h"
#include "rtc.h"
#include "diskio.h"
#include "sdcard.h"
#include "debug.h"

static void sdcard_init(void) {

	DDR_SD_CD |= _BV(PIN_SD_CD);	// SD CD as input
	PORT_SD_CD |= _BV(PIN_SD_CD);	// enable pull-up

	DDR_SD_WP |= _BV(PIN_SD_WP);	// SD WP as input
	PORT_SD_WP |= _BV(PIN_SD_WP);	// enable pull-up

	// enable pin change interrupt for SD card detect
	PCIFR |= _BV(SDCD_PCIF);	// clear interrupt flag
	SDCD_PCMSK |= _BV(SDCD_PCINT);	// enable SD CD in pin change enable mask
	PCICR |= _BV(SDCD_PCIE);	// enable pin change interrupt

	SD_disk_initialize(0);
}

void device_init(void) {

        // IEEE488 bus
        ieeehw_init();                  // hardware
        ieee_init(8);                   // hardware-independent part; registers as bus

	// Real time clock
	rtc_init();

	// SD card
	sdcard_init();
}
