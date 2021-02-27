/*
    XD-2031 - Serial line filesystem server for CBMs 
    Copyright (C) 2012  Andre Fachat (afachat@gmx.de)

    XS-1541 specific device initialization

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

#include "hwdefines.h"
#include "device.h"
#include "iechw.h"
#include "iec.h"
#include "ieeehw.h"
#include "ieee.h"
#include "debug.h"

static void disable_ints(void) {
	/* IEEE-488 ATN interrupt using PCINT */
#ifdef HAS_IEEE
    	XS1541_PCMSK &= (uint8_t) ~_BV(IEEE_PCINT);
#endif
#ifdef HAS_IEC
    	XS1541_PCMSK &= (uint8_t) ~_BV(IEC_PCINT);
#endif
}


static void enable_ints(void) {

  	/* clear interrupt flag */
  	PCIFR |= _BV(PCIF3);

  	/* enable ATN in pin change enable mask
  	* translates to
  	*   PCMSK3 |= _BV(PCINT27) 
  	* which is ok for PD3
  	*/

	// disable interrupts
  	XS1541_PCMSK = 0;

	// both edges trigger an interrupt
	//EICRA |= (1 << ISC11);
	//EICRA &= ~(1 << ISC10);
	// falling edge on ATN
	//EICRA |= (1 << ISC10);
	//EICRA &= ~(1 << ISC11);

#ifdef HAS_IEEE
	// PD3
  	XS1541_PCMSK |= _BV(IEEE_PCINT);
#endif
#ifdef HAS_IEC
	// PD2
// PD2 and PD3 end up in the same change flag interrupt, without any
// easy way to determine which one has changed ...
//  	XS1541_PCMSK |= _BV(IEC_PCINT);
#endif

  	/* Enable pin change interrupt 3 (PCINT31..24) */
  	PCICR |= _BV(PCIE3);

  	debug_puts("Done init ieee ints"); debug_putcrlf();
}


void device_init(void) {

	// make sure nothing unexpected happens
	disable_ints();

#ifdef HAS_IEEE
        // IEEE488 bus
        ieeehw_init();                  // hardware
        ieee_init(8);                   // hardware-independent part; registers as bus
#endif
#ifdef HAS_IEC
        // IEC bus
        iechw_init();                   // hardware
        iec_init(8);                    // hardware-independent part; registers as bus
#endif
	
	// enable interrupts - one interrupt shared by IEEE and IEC
	enable_ints();
}

void device_loop(void) {
#ifdef HAS_IEEE
        // handle IEEE488 bus
        ieee_mainloop_iteration();
#endif
#ifdef HAS_IEC
        // handle IEC bus
        iec_mainloop_iteration();
#endif
}

