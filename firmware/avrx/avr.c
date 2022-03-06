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

#include "led.h"
#include "term.h"

// usualle BADISR_vect is used to grab all unknown/unintended 
// interrupts.
// The table below defines all the interrupt sources, we
// can do a binary search for an unknown interrupt source

#if 0
ISR(BADISR_vect)
{

	// note that led_set enables interrupts, so...
	led_set(PANIC);

	while (1);
}
#endif

ISR(CRCSCAN_NMI_vect) { led_set(PANIC); while(1); }
//
ISR(BOD_VLM_vect) { led_set(PANIC); while(1); }
//
ISR(RTC_CNT_vect) { led_set(PANIC); while(1); }
ISR(RTC_PIT_vect) { led_set(PANIC); while(1); }
ISR(CCL_CCL_vect) { led_set(PANIC); while(1); }
//
ISR(PORTA_PORT_vect) { led_set(PANIC); while(1); }
//ISR(TCA0_LUNF_vect) { led_set(PANIC); while(1); }
//ISR(TCA0_OVF_vectx) { led_set(PANIC); while(1); }
ISR(TCA0_HUNF_vect) { led_set(PANIC); while(1); }
ISR(TCA0_CMP0_vect) { led_set(PANIC); while(1); }
//ISR(TCA0_LCMP0_vect) { led_set(PANIC); while(1); }
ISR(TCA0_CMP1_vect) { led_set(PANIC); while(1); }
//
//ISR(TCA0_LCMP1_vect) { led_set(PANIC); while(1); }
ISR(TCA0_CMP2_vect) { led_set(PANIC); while(1); }
//ISR(TCA0_LCMP2_vect) { led_set(PANIC); while(1); }
ISR(TCB0_INT_vect) { led_set(PANIC); while(1); }
ISR(TCB1_INT_vect) { led_set(PANIC); while(1); }
ISR(TWI0_TWIS_vect) { led_set(PANIC); while(1); }
ISR(TWI0_TWIM_vect) { led_set(PANIC); while(1); }
ISR(SPI0_INT_vect) { led_set(PANIC); while(1); }
ISR(USART0_RXC_vect) { led_set(PANIC); while(1); }
//
ISR(USART0_DRE_vect) { led_set(PANIC); while(1); }
ISR(USART0_TXC_vect) { led_set(PANIC); while(1); }
ISR(PORTD_PORT_vect) { led_set(PANIC); while(1); }
ISR(AC0_AC_vect) { led_set(PANIC); while(1); }
ISR(ADC0_RESRDY_vect) { led_set(PANIC); while(1); }
ISR(ADC0_WCOMP_vect) { led_set(PANIC); while(1); }
ISR(PORTC_PORT_vect) { led_set(PANIC); while(1); }
ISR(TCB2_INT_vect) { led_set(PANIC); while(1); }
ISR(USART2_RXC_vect) { led_set(PANIC); while(1); }
ISR(USART2_DRE_vect) { led_set(PANIC); while(1); }
ISR(USART2_TXC_vect) { led_set(PANIC); while(1); }
ISR(PORTB_PORT_vect) { led_set(PANIC); while(1); }
ISR(PORTE_PORT_vect) { led_set(PANIC); while(1); }
ISR(TCB3_INT_vect) { led_set(PANIC); while(1); }
//ISR(USART3_RXC_vect) { led_set(PANIC); while(1); }
//ISR(USART3_DRE_vect) { led_set(PANIC); while(1); }
ISR(USART3_TXC_vect) { led_set(PANIC); while(1); }

void fuse_info(void) {
/*
	uint8_t lowfuse, hifuse, extfuse, lockfuse;

	cli();
	lowfuse = boot_lock_fuse_bits_get(GET_LOW_FUSE_BITS);
	hifuse = boot_lock_fuse_bits_get(GET_HIGH_FUSE_BITS);
	extfuse = boot_lock_fuse_bits_get(GET_EXTENDED_FUSE_BITS);
	lockfuse = boot_lock_fuse_bits_get(GET_LOCK_BITS);
	sei();
	term_printf("\r\nFuses: l=%02X h=%02X e=%02X l=%02X\r\n", lowfuse, hifuse, extfuse, lockfuse);
*/
}
