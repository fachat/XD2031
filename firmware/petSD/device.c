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

#include "device.h"

void device_init(void) {

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

}


