/**************************************************************************

    atn.S  -- ATN acknowledge (software, interrupt driven)

    This file is part of XD-2031 -- Serial line filesystem server for CBMs

    Copyright (C) 2012 Andre Fachat <afachat@gmx.de>
    Copyright (C) 2015 Nils Eilers  <nils.eilers@gmx.de>

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

; ATN interrupt handler

.global INT0_vect                                             ; Cycles in IRQ
INT0_vect:

; Save registers

        push    r2                    ; Make place for status register  2
        in      r2, _SFR_IO_ADDR(SREG); Save status register            1
        push    r16                   ; Save working register           2

; Pull NDAC and NRFD low to acknowledge ATN
;
; If the bus drivers should be in talk state, these lines are inputs
; and clearing these bits in the port registers disables the internal
; pull-ups without doing harm. The lines get then pulled low later
; when the bus drivers are switched to listen mode by setting TE to 0.

        in      r16, 8                ;                                 1
        andi    r16, 255 - _BV(PC6) - _BV(PC7);                         1
        out     8, r16                ; NDAC low, NRFD low              1
                                      ;                               ---
                                      ;                                 8

        clr     r16                   ; switch data lines to input      1
        out     1, r16                ; DDRA = 0                        1

        cbi     4, 2                  ; DAV->input (clear DDB2)         2
        cbi     10, 7                 ; EOI->input (clear DDD7)         2

; Switch bus drivers to listen mode
        cbi     5, 0                  ; TE->0 (clear PORTB0)            2

        in      r16, 7                ; NDAC + NRFD as outputs          1
        ori     r16, _BV(PC6) | _BV(PC7);                               1
        out     7, r16                ;                                 1

                                      ;                               ===
                                      ;                                19

; End of interrupt routine, restore registers
        pop     r16
        out     _SFR_IO_ADDR(SREG), r2
        pop     r2
        reti

.end

; Response time:
;
; Five cyles are required for pushing the Program Counter (PC) value into
; the stack, updating the stack pointer and clearing the Global interrupt
; enable (I) bit.
;
; Cycles if bus drivers were in listen mode: 5 +  8 = 13 cylces
; Cycles if bus drivers were in talk   mode: 5 + 19 = 24 cylces


