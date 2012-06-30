/****************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2012 Andre Fachat

    Inspired by uart.c from XS-1541, but rewritten in the end.

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

/**
 * UART interrupt handler
 */

.extern rxbuf
.extern rx_wp
.extern rx_rp

.extern txbuf
.extern tx_wp
.extern tx_rp


.global USART0_UDRE_vect
USART0_UDRE_vect:
	push	r2		;	make place for status register
	in	r2, _SFR_IO_ADDR(SREG)	; 	save status register
	push	r16		;	save working register
	push	YL		;	save working register Y low
	push	YH		;	save working register Y high

	; interrupt payload
	
	lds	r16, tx_rp	;	get read pointer into r16
	lds	YL, tx_wp	; 	ring buffer write pointer to YL
	cp	r16, YL		;	compare tx_wp and tx_rp

	breq	done		; 	wp==rp, nothing to do

	ldi	YL, lo8(txbuf)	; 	compute address of actual byte
	ldi	YH, hi8(txbuf)	;	as txbuf plus read pointer

	add	YL, r16		;	low byte now in YL (note add needs no carry, but sets it)
	brcc 	nocarry		;	carry overflow?
	inc	YH	
nocarry:

	ld 	YL, Y		;	load byte from address in Y into YL (don't need Y anymore)

	sts	UDR0, YL	; 	store data to UDR

	inc	r16		;	tx_rp next address
	andi	r16, 127	;	BUFFER_SIZE_MASK

	sts	tx_rp, r16	;	store new tx_rp

	;sei			; 	allow nested interrupts for faster ATN response

	jmp 	endirq

done:				; no more data
	lds	r16, UCSR0B	; UART control register
	andi	r16, 255-_BV(UDRIE0)
	sts	UCSR0B, r16

	; cleanup
endirq:
	pop	YH
	pop	YL
	pop	r16
	out	_SFR_IO_ADDR(SREG), r2
	pop	r2
	reti

.end
	
