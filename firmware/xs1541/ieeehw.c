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

#include "ieeehw.h"

/* ------------------------------------------------------------------------- 
 * local variables
 */

// when set, disable ATN acknowledgement
uint8_t is_atna = 0;
uint8_t is_ndacout = 0;
uint8_t is_nrfdout = 0;

/* ------------------------------------------------------------------------- */
/*  Interrupt handling                                                       */
/* ------------------------------------------------------------------------- */

static void ieee_interrupts_init(void)  {
  /* clear interrupt flag */
  PCIFR |= _BV(PCIF3);

  /* enable ATN in pin change enable mask
  * translates to
  *   PCMSK3 |= _BV(PCINT27)
  * which is ok for PD3
  */
  IEEE_PCMSK |= _BV(IEEE_PCINT);

  /* Enable pin change interrupt 3 (PCINT31..24) */
  PCICR |= _BV(PCIE3);

  //debug_putps("Done init ieee ints"); debug_putcrlf();
}

/* IEEE-488 ATN interrupt using PCINT */
static void set_atn_irq(uint8_t x) {
#if DEBUG
  debug_putps("ATN_IRQ:"); debug_puthex(x); debug_putcrlf();
#endif
  if (x)
    IEEE_PCMSK |= _BV(IEEE_PCINT);
  else
    IEEE_PCMSK &= (uint8_t) ~_BV(IEEE_PCINT);
}

/* Interrupt routine that simulates the hardware-auto-acknowledge of ATN
   at falling edge of ATN. If pin change interrupts are used, we have to
   check for rising or falling edge in software first! */
// Note: translates to ISR(PCINT3_vect)
/*
IEEE_ATN_HANDLER {
#ifdef IEEE_PCMSK
  if(!IEEE_ATN) {
#else
  {
#endif
    ddr_change_by_atn();        // Switch NDAC+NRFD to outputs
    set_ndac_state(0);          // Set NDAC and NRFD low
    set_nrfd_state(0);
  }
}
*/

/* ------------------------------------------------------------------------- 
 *  General functions
 */

void ieeehw_setup() {
	// clear IEEE lines
	atnahi();
	clrd();
	davhi();
	nrfdhi();
	ndachi();
	eoihi();
}

void ieeehw_init() {

	// disable ATN interrupt
	set_atn_irq(0);

	// clear IEEE lines
	ieeehw_setup();

	// start ATN interrupt handling
	ieee_interrupts_init();
}



