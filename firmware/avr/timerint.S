/****************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2012 Andre Fachat
    Copyright (C) 2012 Nils Eilers

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
 * LED interrupt handler / timer with 10ms resolution
 */

.extern led_program
.extern led_current
.extern led_counter
.extern led_bit
.extern led_shift
.extern led_code
.extern timer10ms


;---------------------------------------------------------------------------
; TIMER underflow interrupt (timer 1)
.global TIMER1_COMPA_vect							; cycles in IRQ:
TIMER1_COMPA_vect:
	push 	r2		; 	make place for status register		; 2
	in	r2, _SFR_IO_ADDR(SREG)	; save status register			; 1

	sei			; 	allow other nested interrupts, esp. ATN	; 1
	push	r24		; 	save working register			
	push	r25		; 	save working register			
	push 	YL
	push 	YH

	;---------------------------------------------------------
	; Decrement timer if nonzero
	lds	r24, timer10ms
	lds	r25, timer10ms + 1
	sbiw	r24, 0
	breq	do_led		;	don't decrement if already zero
	sbiw	r24, 1		;	decrement
	sts	timer10ms, r24
	sts	timer10ms+1,r25	;	store

	;---------------------------------------------------------
	; LED handling
do_led:	lds 	r24, led_program
	cpi	r24, 0		; 	is zero?
	brne	led		;	if yes, then no LED irq handling
	jmp	ledend
led:
	; bit counter
	lds 	r24, led_bit	; 	bit counter
	cpi	r24, 0		;	done?
	brne	ledbit		;	no, next bit
	; interval counter
	lds	r24, led_counter
	cpi	r24, 0
	brne	endcnt
	; program counter - get next prg byte
	lds	r24, led_current
retry:	ldi	YL, lo8(led_code-1)
	ldi	YH, hi8(led_code-1)
	add	YL, r24
	brcc	nocarry
	inc	YH
nocarry:
	ld	r25, Y+		; 	load byte from address pointed to by Y, post-autoinc Y
	cpi 	r25, 0		; 	reached end of prg?
	brne	notend
	; end of prg, re-init program counter and retry
	lds	r24, led_program
	jmp 	retry
notend:
	sts	led_counter, r25 ;	interval counter
	ld	r25, Y		; 	is now pointing to shift byte
	sts	led_pattern, r25
	inc 	r24
	inc	r24
	sts	led_current, r24
	jmp 	endcnt2
endcnt:	
	dec	r24
	sts	led_counter, r24
endcnt2:
	; re-init shift register
	lds	r24, led_pattern
	sts	led_shift, r24
	ldi	r24, 8
ledbit:
	dec	r24
	sts	led_bit, r24

	; shift one bit
	lds 	r24, led_shift
	ror	r24
	sts	led_shift, r24
	brcc 	ledoff
ledon:	
	lds 	r24, LED_PORT
	ori	r24, _BV(LED_BIT)
	sts	LED_PORT, r24
	jmp	ledend
ledoff:
	lds 	r24, LED_PORT
	andi	r24, 255-_BV(LED_BIT)
	sts	LED_PORT, r24
ledend:
	;---------------------------------------------------------

	; end of interrupt
	pop	YH
	pop	YL
	pop	r25
	pop	r24
	out	_SFR_IO_ADDR(SREG), r2
	pop	r2
	reti
.end
	
