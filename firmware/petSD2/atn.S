/**************************************************************************

    atn.S  -- ATN acknowledge (software, interrupt driven)

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


#include <avr/io.h>
#include <avr/interrupt.h>

/**
 * ATN interrupt handler
 */

.global INT0_vect                                         ; Cycles in IRQ
INT0_vect:
        ; Save registers
        push    r2                    ; Make place for status register  2
        in      r2, _SFR_IO_ADDR(SREG); Save status register            1
        push    r16                   ; Save working register           2

        ; ATN acknowledge
        ; pull NRFD (PC7) + NDAC (PC6) low
        ; make sure, bus drivers are in listen mode (TE (PC3) = 0)
        lds     r16, PORTC            ;                                 2
        andi    r16, 255 - _BV(PC7) - _BV(PC6) - _BV(PC3) ;             1
        sts     PORTC, r16            ;                                 2

        lds     r16, DDRC             ; NDFD + NDAC as outputs          2
        ori     r16, _BV(PC7) | _BV(PC6);                               1
        ; Prerequistes for possible change of flow direction if 
        ; ATN came when device was talker
        andi    r16, 255 - _BV(PC5) - _BV(PC4) ; DAV  + EOI as input    1
        sts     DDRC, r16             ;                                 2

                                      ;                               ===
                                      ;                                16 

        ; End of interrupt routine, restore registers
        pop     r16
        out     _SFR_IO_ADDR(SREG), r2
        pop     r2
        reti

.end
