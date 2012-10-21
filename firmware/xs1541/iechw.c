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

#include "iechw.h"

/* ------------------------------------------------------------------------- 
 * local variables
 */

// when set, disable ATN acknowledgement
uint8_t is_satna = 0;
uint8_t is_dataout = 0;

/* ------------------------------------------------------------------------- 
 *  General functions
 */

void iechw_setup() {
	// clear IEEE lines
	atn_init();
	satnahi();
	datahi();
	clkhi();
}

void iechw_init() {

	// disable ATN interrupt
	//set_atn_irq(0);

	// clear IEEE lines
	iechw_setup();

	// start ATN interrupt handling
	//iec_interrupts_init();
}



