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
#include "timer.h"
#include "main.h"
#include "packet.h"
#include "uart2.h"
#include "ieee.h"
#include "led.h"
#include "term.h"
//#include "iec.h"

// STRUCTS

// STATICS


// GLOBALS
uint8_t	fDevice;								// 0=IEEE, 1=IEC
//uint8_t iec_device = 8;						// current device#


// EXTERNALS
extern uint8_t		rcvBcr;				// CR flag


//---------------------------
// LIST XS-1541 VERSIONSTRING
void ListVersion()
{
	term_putcrlf();
	term_putc('.');

	term_putcrlf();
	
	term_puts("### "HW_NAME" v"VERSION_STR" ###");
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


//------------------------------------------------------------------------
// Initialisierungen
//------------------------------------------------------------------------
void init()
{
	//TIMSK0 = 0;			// maskiere alle INT

	// Ports initialisieren

	//GLOBALS
	fDevice	= 0;
}


/////////////////////////////////////////////////////////////////////////////
// Main-Funktion
/////////////////////////////////////////////////////////////////////////////
int main()
{
	init(); 				// Initialisierungen
	led_init();
	uart_init();
	term_init();
	file_init();
	channel_init();
	//timer_init();			// Timer Interrupt initialisieren
	ieee_init();

	sei();

  	ListVersion();

	term_printf((" %u Bytes free"), BytesFree());
	term_printf((", %d kHz"), (int32_t)(F_CPU/1000));
	term_putcrlf();
	term_putcrlf();

	ieee_mainloop_init();

	while (1)  			// Mainloop-Begin
	{
		ieee_mainloop_iteration();
	}
}
//---------------------------------------------------------------------------

