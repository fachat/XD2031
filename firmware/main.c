//-------------------------------------------------------------------------
// Titel:    XD-2031 firmware for the XS-1541 Adapter
// Funktion: Adapter to connect IEEE-488, IEC and RS232
//-------------------------------------------------------------------------
// Copyright (C) 2012  Andre Fachat <afachat@gmx.de>
// Copyright (C) 2008  Thomas Winkler <t.winkler@tirol.com>
//-------------------------------------------------------------------------
// Prozessor : 	ATmega644
// Takt : 		14745600 Hz
// Datum : 		11.6.2008
// Version : 	in config.h
//-------------------------------------------------------------------------
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version
// 2 of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
//-------------------------------------------------------------------------

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <alloca.h>

#include "config.h"
#include "arch.h"
#include "version.h"
#include "main.h"
#include "packet.h"
#include "serial.h"
#include "uarthw.h"
#include "device.h"
#include "rtconfig.h"

// those are currently still needed for *_mainloop_iteration
#ifdef HAS_IEC
#include "iec.h"
#endif
#ifdef HAS_IEEE
#include "ieee.h"
#endif

#ifdef USE_FAT
#include "fat_provider.h"
#endif

#include "term.h"
#include "file.h"
#include "channel.h"
#include "bus.h"
#include "mem.h"

#include "timer.h"
#include "led.h"


//---------------------------
// LIST XD-2031 VERSIONSTRING
void ListVersion()
{
	term_putcrlf();
	
	term_rom_puts(IN_ROM_STR("### "HW_NAME"/"SW_NAME" v"VERSION LONGVERSION" ###"));
	term_putcrlf();
}


//--------------------------
// CALC FREE RAM SPACE
uint16_t BytesFree()
{
	extern unsigned char __heap_start;
	uint16_t momentan_frei = SP - (uint16_t) &__heap_start;
	return momentan_frei;
}


static endpoint_t term_endpoint;

// -------------------------
// delay loop, to keep all maintenance running while
// waiting for a response from the server

void main_delay() {
	serial_delay();
}

/////////////////////////////////////////////////////////////////////////////
// Main-Funktion
/////////////////////////////////////////////////////////////////////////////
int main()
{
	// Initialisierungen
	//
	// first some basic hardware infrastructure
	
	timer_init();			// Timer Interrupt initialisieren
	led_init();

	provider_init();		// needs to be in the beginning, as other
					// modules like serial register here

	term_init();			// does not need endpoint/provider yet
					// but can take up to a buffer of text

	// server communication
	uarthw_init();			// first hardware
	provider_t *serial = serial_init();	// then logic layer

	// now prepare for terminal etc
	// (note: in the future the assign parameter could be used
	// to distinguish different UARTs for example)
	void *epdata = serial->prov_assign(NULL);
	term_endpoint.provider = serial;
	term_endpoint.provdata = epdata;

	// and set as default
	provider_set_default(serial, epdata);

	// debug output via "terminal"
	term_set_endpoint(&term_endpoint);

	// init file handling (active open calls)
	file_init();
	// init main channel handling
	channel_init();

	// before we init any busses, we init the runtime config code
	// note it gets the provider to register a listener for X command line params
	rtconfig_init(&term_endpoint);

	// bus init	
	// first the general bus (with bus counter)
	bus_init();		

	// this call initializes the device-specific hardware
	// e.g. IEEE488 and IEC busses on xs1541, plus SD card on petSD and so on
	// it also handles the interrupt initialization if necessary
	device_init();

	// enable interrupts
	enable_interrupts();

	// sync with the server
	serial_sync();		

	// pull in command line config options from server
	rtconfig_pullconfig();

#ifdef USE_FAT
	// register fat provider
	provider_register("FAT", &fat_provider);
#endif

	// show our version...
  	ListVersion();
	// ... and some system info
	term_printf((" %u Bytes free"), BytesFree());
	term_printf((", %d kHz"), (int32_t)(F_CPU/1000));
	fuse_info();
	term_putcrlf();
	term_putcrlf();

	while (1)  			// Mainloop-Begin
	{
		// keep data flowing on the serial line
		main_delay();
#ifdef HAS_IEEE
		// handle IEEE488 bus
		ieee_mainloop_iteration();
#endif
#ifdef HAS_IEC
		// handle IEC bus
		iec_mainloop_iteration();
#endif
	}
}
//---------------------------------------------------------------------------

