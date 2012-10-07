/****************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2012 Andre Fachat

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation;
    version 2 of the License ONLY.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

****************************************************************************/

#include <avr/io.h>
#include <avr/interrupt.h>
#include "config.h"		// LED definitions

/**
 * LED interrupt handler
 */

.extern led_program
.extern led_current
.extern led_counter
.extern led_bit
.extern led_shift
.extern led_code


;---------------------------------------------------------------------------
; TIMER underflow interrupt (timer 1)
.global TIMER1_COMPA_vect							; cycles in IRQ:
TIMER1_COMPA_vect:
	push 	r2		; 	make place for status register		; 2
	in	r2, _SFR_IO_ADDR(SREG)	; save status register			; 1

	sei			; 	allow other nested interrupts, esp. ATN	; 1
	push	r16		; 	save working register			
	push	r17		; 	save working register			
	push 	YL
	push 	YH

	;---------------------------------------------------------
	; LED handling
	lds 	r16, led_program
	cpi	r16, 0		; 	is zero?
	brne	led		;	if yes, then no LED irq handling
	jmp	ledend
led:
	; bit counter
	lds 	r16, led_bit	; 	bit counter
	cpi	r16, 0		;	done?
	brne	ledbit		;	no, next bit
	; interval counter
	lds	r16, led_counter
	cpi	r16, 0
	brne	endcnt
	; program counter - get next prg byte
	lds	r16, led_current
retry:	ldi	YL, lo8(led_code-1)
	ldi	YH, hi8(led_code-1)
	add	YL, r16
	brcc	nocarry
	inc	YH
nocarry:
	ld	r17, Y+		; 	load byte from address pointed to by Y, post-autoinc Y
	cpi 	r17, 0		; 	reached end of prg?
	brne	notend
	; end of prg, re-init program counter and retry
	lds	r16, led_program
	jmp 	retry
notend:
	sts	led_counter, r17 ;	interval counter
	ld	r17, Y		; 	is now pointing to shift byte
	sts	led_pattern, r17
	inc 	r16
	inc	r16
	sts	led_current, r16
	jmp 	endcnt2
endcnt:	
	dec	r16
	sts	led_counter, r16
endcnt2:
	; re-init shift register
	lds	r16, led_pattern
	sts	led_shift, r16
	ldi	r16, 8
ledbit:
	dec	r16
	sts	led_bit, r16

	; shift one bit
	lds 	r16, led_shift
	ror	r16
	sts	led_shift, r16
	brcc 	ledoff
ledon:	
	lds 	r16, LED_PORT
	ori	r16, _BV(LED_BIT)
	sts	LED_PORT, r16
	jmp	ledend
ledoff:
	lds 	r16, LED_PORT
	andi	r16, 255-_BV(LED_BIT)
	sts	LED_PORT, r16
ledend:
	;---------------------------------------------------------

	; end of interrupt
	pop	YH
	pop	YL
	pop	r17
	pop	r16
	out	_SFR_IO_ADDR(SREG), r2
	pop	r2
	reti
.end
	
