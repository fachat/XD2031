//-------------------------------------------------------------------------
// Titel:    XS-1541 - controller compatibility 
// Funktion: unique definitions for different controller
//-------------------------------------------------------------------------
// Copyright (C) 2012  Andre Fachat <afachat@gmx.de>
// Copyright (C) 2007,2008  Ingo Korb <ingo@akana.de>
// Copyright (C) 2008  Thomas Winkler <t.winkler@tirol.com>
//-------------------------------------------------------------------------
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation version 2 ONLY
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//-------------------------------------------------------------------------

#ifndef COMPAT_H
#define COMPAT_H
    
#if defined __AVR_ATmega644__ || defined __AVR_ATmega644P__ || defined __AVR_ATmega1284P__
#define RXC   RXC0
#define RXEN  RXEN0
#define TXC   TXC0
#define TXEN  TXEN0
#define UBRRH UBRR0H
#define UBRRL UBRR0L
#define UCSRA UCSR0A
#define UCSRB UCSR0B
#define UCSRC UCSR0C
#define UCSZ0 UCSZ00
#define UCSZ1 UCSZ01
#define UDR   UDR0
#define UDRIE UDRIE0
#define UDRE  UDRE0
#define RXCIE	RXCIE0
#define TIFR  TIFR0
#define TCCR0 TCCR0B
//#  define TIMSK TIMSK1
#define USART_UDRE_vect	USART0_UDRE_vect
#define USART_RXC_vect	USART0_RX_vect
    
#elif defined __AVR_ATmega324__ || defined __AVR_ATmega324P__
#define RXC   RXC0
#define RXEN  RXEN0
#define TXC   TXC0
#define TXEN  TXEN0
#define UBRRH UBRR0H
#define UBRRL UBRR0L
#define UCSRA UCSR0A
#define UCSRB UCSR0B
#define UCSRC UCSR0C
#define UCSZ0 UCSZ00
#define UCSZ1 UCSZ01
#define UDR   UDR0
#define UDRIE UDRIE0
#define UDRE  UDRE0
#define RXCIE	RXCIE0
#define TIFR  TIFR0
#define TCCR0 TCCR0B
//#  define TIMSK TIMSK1
#define USART_UDRE_vect	USART0_UDRE_vect
#define USART_RXC_vect	USART0_RX_vect
    
#elif defined __AVR_ATmega328__ || defined __AVR_ATmega328P__
//#define RXC   RXC0
#define RXEN  RXEN0
//#define TXC   TXC0
#define TXEN  TXEN0
#define UBRRH UBRR0H
#define UBRRL UBRR0L
//#define UCSRA UCSR0A
#define UCSRB UCSR0B
#define UCSRC UCSR0C
//#define UCSZ0 UCSZ00
//#define UCSZ1 UCSZ01
#define UDR   UDR0
#define UDRIE UDRIE0
//#define UDRE  UDRE0
#define RXCIE	RXCIE0
//#define TIFR  TIFR0
//#define TCCR0 TCCR0B
//#  define TIMSK TIMSK1
//#define USART_UDRE_vect	USART0_UDRE_vect
#define USART_RXC_vect	USART0_RX_vect

#elif defined __AVR_ATmega32__
#define TIMER2_COMPA_vect TIMER2_COMP_vect
#define TCCR0B TCCR0
#define TCCR2A TCCR2
#define TCCR2B TCCR2
#define TIFR0  TIFR
#define OCIE2A OCIE2
#define OCR2A  OCR2
#define TIMSK0 TIMSK
#define TIMSK1 TIMSK

#elif defined __AVR_ATmega128__
#define UBRRH  UBRR0H
#define UBRRL  UBRR0L
#define UCSRA  UCSR0A
#define UCSRB  UCSR0B
#define UCSRC  UCSR0C
#define UDR    UDR0
#define USART_UDRE_vect USART0_UDRE_vect
#define TIMER2_COMPA_vect TIMER2_COMP_vect
#define TCCR0B TCCR0
#define TCCR2A TCCR2
#define TCCR2B TCCR2
#define TIFR0  TIFR
#define TIMSK1 TIMSK
#define TIMSK2 TIMSK
#define OCIE2A OCIE2
#define OCR2A  OCR2
    
#else
#error chip not defined!
#endif

#endif	/*  */
