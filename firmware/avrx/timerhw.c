
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

/*
 * timer handling
 */
#include <avr/io.h>
#include <avr/interrupt.h>

#include "config.h"
#include "hwdefines.h"
#include "ledhw.h"

#define PERVAL_FOR_INTSECS_256PRESCALE(INT)     ((((INT)*(F_CPU))/256)-1)


volatile uint16_t timer10ms = 0;	// decremented by timer 1 if nonzero

void timerhw_init(void) {
	// timer configuration derived from
	// https://github.com/microchip-pic-avr-examples/atmega4809-getting-started-with-tca-studio/blob/master/Using_Periodic_Interrupt_Mode/main.c
	
	// Timer 1: 100Hz

        // setup 16 MHz clock clk_per
        // clk_per source is 16/20mhz osc
        _PROTECTED_WRITE((CLKCTRL.MCLKCTRLA), CLKCTRL_CLKSEL_OSC20M_gc);
        // disable pre-scaler
        _PROTECTED_WRITE((CLKCTRL.MCLKCTRLB), (0 << CLKCTRL_PEN_bp));


    /* set the period */
    TCA0.SINGLE.PER = PERVAL_FOR_INTSECS_256PRESCALE(0.01);  

    /* disable event counting */
    TCA0.SINGLE.EVCTRL &= ~(TCA_SINGLE_CNTEI_bm);

    /* Timer A pre-scaler is 256 */
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV256_gc         /* set clock source (sys_clk/256) */
                      | TCA_SINGLE_ENABLE_bm;                /* start timer */

    /* set Normal mode */
    TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_NORMAL_gc;

    /* enable overflow interrupt */
    TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;
}

ISR(TCA0_OVF_vect) {

	if (timer10ms != 0) {
		timer10ms --;
	}

	if (led_program) {
	
		// led:	
		if (led_bit == 0) {
	
			if (led_counter == 0) {
			
				do {	
					// get next program byte
					uint8_t c = led_code[led_current++];

					if (c) {
						// notend:
						led_counter = c;
						led_pattern = led_code[led_current++];
						break;		
					} else {
						led_current = led_program;
						continue;
					}
				} while (1);

			} else {
				// endcnt:
				led_counter--;
			}
	
			led_shift = led_pattern;	
			led_bit = 8;
		}

		// ledbit:
		led_bit --;

		if (led_shift & 1) {
			// LED on
			LED_PORT |= LED_BIT_bm;
		} else {
			// LED off
			LED_PORT &= ~LED_BIT_bm;
		}
		
		led_shift >>= 1;
	}

	// disable Interrupt, so it does not trigger again immediately
	TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;

}

