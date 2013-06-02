/**************************************************************************

    ieeehw.c  -- IEEE-488 bus routines

    This file is part of XD-2031 -- Serial line filesystem server for CBMs

    Copyright (C) 2013 Andre Fachat <afachat@gmx.de>
    Copyrifht (C) 2013 Nils Eilers  <nils.eilers@gmx.de>

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


#include "ieeehw.h"


/* ----------------------------------------------------------------------- 
 * Local variables
 */

// when set, disable ATN acknowledgement
uint8_t is_atna = 0;
uint8_t is_ndacout = 0;
uint8_t is_nrfdout = 0;


/* ----------------------------------------------------------------------- 
 * Interrupt handling
 */

static void ieee_interrupts_init(void)  {
    IEEE_DDR_ATN &= ~_BV(IEEE_PIN_ATN);    // define ATN as input
    IEEE_PORT_ATN |= _BV(IEEE_PIN_ATN);    // enable pull-up

    EIMSK &= (uint8_t) ~_BV(INT0);         // disable interrupt
    EICRA &= (uint8_t) ~_BV(ISC00);        // configure interrupt on
    EICRA |= _BV(ISC01);                   // falling edge of ATN
    EIMSK |= _BV(INT0);                    // enable interrupt
    //debug_putps("Done init ieee ints"); debug_putcrlf();
}


// IEEE-488 ATN interrupt using INT0
static void set_atn_irq(uint8_t x) {
#if DEBUG
    debug_putps("ATN_IRQ:"); debug_puthex(x); debug_putcrlf();
#endif
    if (x)
        EIMSK |= _BV(IEEE_ATN_INT);
    else
        EIMSK &= (uint8_t) ~_BV(IEEE_ATN_INT);
}


/* ----------------------------------------------------------------------- 
 * General functions
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

    IEEE_DDR_TE |= _BV(IEEE_PIN_TE);                // Define TE as output
    IEEE_PORT_TE &= (uint8_t) ~ _BV(IEEE_PIN_TE);   // TE=0

    // TODO: let slave MCU set DC=1 (device)

    // disable ATN interrupt
    set_atn_irq(0);

    // clear IEEE lines
    ieeehw_setup();

    // start ATN interrupt handling
    ieee_interrupts_init();
}
