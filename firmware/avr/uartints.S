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

/**
 * UART interrupt handler
 */

.extern rxbuf
.extern rx_wp
.extern rx_rp
.extern rx_err

.extern txbuf
.extern tx_wp
.extern tx_rp


;---------------------------------------------------------------------------
; UART receive interrupt
.global USART0_RX_vect								; cycles in IRQ:
USART0_RX_vect:
	push 	r2		; 	make place for status register		; 2
	in	r2, _SFR_IO_ADDR(SREG)	; save status register			; 1
	push	r16		; 	save working register			; 2
	
	; first part of interrupt routine

	lds 	r16, UDR0	; 	load data from receive register		; 2

	; now we have cleared the interrupt signal
	; next receive signal is in 1215 cycles 
	; 115200 bps is 11520 bytes per second, i.e. 87us per byte,
	; i.e. 1215 cycles per byte
	; so we do not run into this routine again soon, even with
	; interrupts in between

	sei			; 	allow other nested interrupts, esp. ATN	; 1

	push 	r17		;	save working byte			; 2
	push	YL		;	save working register Y low		;===
	push	YH		;	save working register Y high		; 10

	lds	r17, rx_wp	;	receive write pointer
	ldi	YL, lo8(rxbuf)	; 	compute address of actual byte
	ldi	YH, hi8(rxbuf)	;	as txbuf plus read pointer

	add	YL, r17		;	compute rxbuf[rx_wp] address
	brcc	rxnocarry
	inc	YH
rxnocarry:
	st	Y, r16		; 	store byte in receive buffer

	; don't need Y anymore, release some stack space

	pop	YH
	pop	YL

	; now increment rx_wp, which is also still in r17

	inc	r17
	andi	r17, 127	;	BUFFER_SIZE_MASK
	
	; compare with rx_wp to check for overflow
	
	lds	r16, rx_rp	;	read pointer
	cp	r17, r16	;	compare
	breq 	rxoverflow	;	branch on overflow

	sts	rx_wp, r17	;	store updated rx_wp

	brne	rxend		;	always

	; set error flag on overflow
rxoverflow:
	lds	r16, rx_err
	inc	r16
	sts	rx_err, r16

	; end
rxend:
	pop	r17		; restore working registers
	pop	r16
	out	_SFR_IO_ADDR(SREG), r2
	pop	r2
	reti

;---------------------------------------------------------------------------
; UART send interrupt

.global USART0_UDRE_vect							; cycles in IRQ
USART0_UDRE_vect:
	push	r2		;	make place for status register		; 2
	in	r2, _SFR_IO_ADDR(SREG)	; 	save status register		; 1
	push	r16		;	save working register			; 2
	push	YL		;	save working register Y low		; 2
	push	YH		;	save working register Y high		; 2

	; interrupt payload
	
	lds	r16, tx_rp	;	get read pointer into r16		; 2
	lds	YL, tx_wp	; 	ring buffer write pointer to YL		; 2
	cp	r16, YL		;	compare tx_wp and tx_rp			; 1

	breq	done		; 	wp==rp, nothing to do			; 1/2

	ldi	YL, lo8(txbuf)	; 	compute address of actual byte		; 1
	ldi	YH, hi8(txbuf)	;	as txbuf plus read pointer		; 1

	add	YL, r16		;	low byte now in YL (note add needs no carry, but sets it); 1
	brcc 	nocarry		;	carry overflow?				; 1/2
	inc	YH								; 1
nocarry:

	ld 	YL, Y		;	load byte from address in Y into YL (don't need Y anymore) ; 2

	sts	UDR0, YL	; 	store data to UDR			; 2

	inc	r16		;	tx_rp next address			; 1
	andi	r16, 127	;	BUFFER_SIZE_MASK			; 1

	sts	tx_rp, r16	;	store new tx_rp				; 2

	sei			; 	allow nested interrupts for faster ATN response ; 1

	rjmp 	endirq								; 2
										;===
										; 33
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
	
