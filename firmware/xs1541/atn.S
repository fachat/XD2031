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

#include <avr/io.h>
#include <avr/interrupt.h>

/**
 * ATN interrupt handler
 */

;---------------------------------------------------------------------------
; ATN change interrupt

.global PCINT3_vect							; cycles in IRQ
PCINT3_vect:
	push	r2		;	make place for status register		; 2
	in	r2, _SFR_IO_ADDR(SREG)	; 	save status register		; 1
	push	r16		;	save working register			; 2

	; first check if we're being called for a serial IEC ATN
	lds 	r16, PIND	;	load ATN in port			; 1
	andi	r16, _BV(PD2)	; 	mask out IEC ATN bit
	brne	checkieee	;	not low, so end
	
	; SATN acknowledge
	lds 	r16, PORTD
	andi 	r16, 255 - _BV(PD4)	; PD4 is DATA
	sts 	PORTD, r16

	lds 	r16, DDRD
	ori	r16, _BV(PD4)
	sts 	DDRD, r16

	; done with IEC, check IEEE via fall through

checkieee:
	lds	r16, PIND	; 	load ATN in port			; 1
	andi	r16, _BV(PD3)	; 	mask out ATN bit			; 1
	
	brne	noatn		;	set, then ATN is not active, end	; 1/2

	; ATN acknowledge
;	lds	r16, PORTC	;	clear port output			; 2
;	andi	r16, 255 - _BV(PC4) - _BV(PC5)	; PC4 is NDAC, PC5 is NRFD	; 1
;	sts	PORTC, r16							; 2
	
	lds 	r16, DDRC	;	set lines to output			; 2
	ori	r16, _BV(PC4) | _BV(PC5)					; 1
	sts	DDRC, r16							; 2
										;===
										;
noatn:	
	; end of interrupt routine

	pop	r16
	out	_SFR_IO_ADDR(SREG), r2
	pop	r2
	reti

.end
	
