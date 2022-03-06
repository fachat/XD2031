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

/*
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
*/
#include <inttypes.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <alloca.h>

#include "config.h"
#include "hwdefines.h"
#include "arch.h"
#include "version.h"
#include "main.h"
#include "packet.h"
#include "serial.h"
#include "uarthw.h"
#include "device.h"
#include "rtconfig.h"
#include "rtconfig2.h"

#ifdef HAS_EEPROM
#include "nvconfig.h"
#endif

#ifdef USE_FAT
#include "fat_provider.h"
#endif

#include "term.h"
#include "file.h"
#include "channel.h"
#include "bus.h"
#include "archcompat.h"

#include "buffer.h"
#include "direct.h"
#include "relfile.h"

#include "timer.h"
#include "led.h"


#ifdef __AVR__
static FILE term_stdout = FDEV_SETUP_STREAM(term_putchar, NULL, _FDEV_SETUP_WRITE);
#endif

//---------------------------
// LIST XD-2031 VERSIONSTRING
void ListVersion()
{
	term_putcrlf();
	
	term_rom_puts(IN_ROM_STR("### "HW_NAME"/"SW_NAME" v"VERSION LONGVERSION" ###"));
	term_putcrlf();
}



static endpoint_t term_endpoint;

static uint8_t is_locked = 1;

void device_unlock(void) {

	term_rom_puts(IN_ROM_STR("Unlocking devices!\n"));
	is_locked = 0;
}

// -------------------------
// delay loop, to keep all maintenance running while
// waiting for a response from the server

void main_delay() {
	serial_delay();
}

/////////////////////////////////////////////////////////////////////////////
// Main-Funktion
/////////////////////////////////////////////////////////////////////////////
int main(int argc, const char *argv[])
{

	// Initializations
	//
	// first some basic hardware infrastructure
	
	// initialize Timer Interrupt
	timer_init();			

	// enable interrupts
	enable_interrupts();		

	// this blinks the LED already
	// note: needs interrupts as delayhw uses timerhw
	led_init();			

	provider_init();		// needs to be in the beginning, as other
					// modules like serial register here

	term_init();			// does not need endpoint/provider yet
					// but can take up to a buffer of text

#ifdef __AVR__
	stdout = &term_stdout;          // redirect stdout
#else
	device_setup(argc, argv);
#endif

	// server communication
	uarthw_init();			// first hardware

	const provider_t *serial = serial_init();	// then logic layer

	term_rom_printf(IN_ROM_STR("Starting...\n"));

	// sync with the server
	serial_sync();		

	// now prepare for terminal etc
	// (note: in the future the assign parameter could be used
	// to distinguish different UARTs for example)

	void *epdata = serial->prov_assign ? serial->prov_assign(NAMEINFO_UNUSED_DRIVE, NULL) : NULL;

	term_endpoint.provider = serial;
	term_endpoint.provdata = epdata;

	// and set as default
	provider_set_default(serial, epdata);

	// debug output via "terminal"
	term_set_endpoint(&term_endpoint);

	// init file handling (active open calls)
	file_init();

#ifdef HAS_BUFFERS
	// buffer structures
	buffer_init();
	// direct buffer handling
	direct_init();
	// relfile handling
	relfile_init();
#endif
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

#ifdef HAS_EEPROM
	// read bus-independent settings from non volatile memory
	nv_restore_common_config();
#endif

	// pull in command line config options from server
	// also send directory charset
	rtconfig_pullconfig(argc, argv);

#ifdef USE_FAT
	// register fat provider
	provider_register("FAT", &fat_provider);
	//provider_assign(0, "FAT", "/");		// might be overwritten when fetching X-commands
	//provider_assign(1, "FAT", "/");		// from the server, but useful for standalone-mode
#endif


	// show our version...
  	ListVersion();
	// ... and some system info
	term_rom_printf(IN_ROM_STR(" %u Bytes free"), BytesFree());
	term_rom_printf(IN_ROM_STR(", %d kHz"), FreqKHz());
#ifdef __AVR__
	fuse_info();
#endif
	term_putcrlf();
	term_putcrlf();

	while (1)  			// Mainloop-Begin
	{
		// keep data flowing on the serial line
		main_delay();

		if (!is_locked) 
			device_loop();

		// send out log messages
		term_flush();
	}
}
//---------------------------------------------------------------------------

