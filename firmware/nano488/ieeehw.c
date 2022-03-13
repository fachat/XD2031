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
#include "debug.h"


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

    //debug_puts("ieeehw_init()");debug_flush();

    IEEE_DDR_TE |= _BV(IEEE_PIN_TE);                // Define TE as output
    telo();

    IEEE_DDR_ATNA |= _BV(IEEE_PIN_ATNA);

    // clear IEEE lines
    ieeehw_setup();
}
