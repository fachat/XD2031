/**************************************************************************

    This file is part of XD-2031 -- Serial line filesystem server for CBMs

    Copyright (C) 2012 Andre Fachat <afachat@gmx.de>
    Copyrifht (C) 2012 Nils Eilers  <nils.eilers@gmx.de>

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

#include <stdint.h>
#include <avr/wdt.h>

#if 0
/* Disable watchdog during program startup */
void get_mcusr(void) \
	__attribute__((naked)) \
	__attribute__((section(".init3")));

void get_mcusr(void)
{
  MCUSR = 0;
  wdt_disable();
}
#endif

void reset_mcu(void) {
  //wdt_enable(WDTO_15MS);
  for(;;);
}

