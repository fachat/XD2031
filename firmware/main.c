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
#include "version.h"
#include "timer.h"
#include "main.h"
#include "packet.h"
#include "serial.h"
#include "uarthw.h"
#include "ieeehw.h"
#include "ieee.h"
#include "term.h"
#include "file.h"
#include "channel.h"
#include "bus.h"

#include "led.h"
//#include "iec.h"

// STRUCTS

// STATICS


// GLOBALS

// EXTERNALS


//---------------------------
// LIST XD-2031 VERSIONSTRING
void ListVersion()
{
	term_putcrlf();
	term_putc('.');

	term_putcrlf();
	
	term_puts("### "HW_NAME"/"SW_NAME" v"VERSION" ###");
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



/////////////////////////////////////////////////////////////////////////////
// Main-Funktion
/////////////////////////////////////////////////////////////////////////////
int main()
{
	// Initialisierungen
	//
	// first some basic hardware infrastructure
	
	//timer_init();			// Timer Interrupt initialisieren
	led_init();

	provider_init();		// needs to be in the beginning, as other
					// modules like serial register here

	// server communication
	uarthw_init();			// first hardware
	serial_init(1);			// then logic layer

	// debug output via "terminal"
	term_init();

	// init file handling (active open calls)
	file_init();
	// init main channel handling
	channel_init();

	// bus init	
	bus_init();			// first the general bus (with bus counter)
	// IEEE488 bus
	ieeehw_init();			// hardware
	ieeehwi_init(8);		// hardware-independent part; registers as bus

	// enable interrupts
	sei();

	// sync with the server
	serial_sync();		

	// show our version...
  	ListVersion();
	// ... and some system info
	term_printf((" %u Bytes free"), BytesFree());
	term_printf((", %d kHz"), (int32_t)(F_CPU/1000));
	term_putcrlf();
	term_putcrlf();

	while (1)  			// Mainloop-Begin
	{
		// keep data flowing on the serial line
		serial_delay();

		// handle IEEE488 bus
		ieee_mainloop_iteration();
	}
}
//---------------------------------------------------------------------------

