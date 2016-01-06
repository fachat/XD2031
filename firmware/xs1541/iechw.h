
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

#ifndef IECHW_H
#define IECHW_H

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "device.h"
#include "hwdefines.h"

// IEEE hw code error codes

#define         E_OK            0
#define         E_ATN           1
#define         E_EOI           2
#define         E_TIME          4
#define         E_BRK           5
#define         E_NODEV         6

// Prototypes

// output of ATNA
extern uint8_t is_satna;
// last output of DATA before ATNA handling
extern uint8_t is_dataout;

// ATN handling
// (input only)

static inline void atn_init()
{
	IEC_DDR_ATN &= (uint8_t) ~ _BV(IEC_PIN_ATN);	// ATN as input
	IEC_PORT_ATN |= _BV(IEC_PIN_ATN);	// Enable pull-up
}

static inline uint8_t satnislo()
{
	return !(IEC_INPUT_ATN & _BV(IEC_PIN_ATN));
}

static inline uint8_t satnishi()
{
	return (IEC_INPUT_ATN & _BV(IEC_PIN_ATN));
}

// DATA & CLK handling

static inline void dataforcelo()
{
	IEC_PORT &= (uint8_t) ~ _BV(IEC_PIN_DATA);	// DATA low
	IEC_DDR |= _BV(IEC_PIN_DATA);	// DATA as output
}

static inline void datalo()
{
	dataforcelo();
	is_dataout = 0;
}

static inline void clklo()
{
	IEC_PORT &= (uint8_t) ~ _BV(IEC_PIN_CLK);	// CLK low
	IEC_DDR |= (uint8_t) _BV(IEC_PIN_CLK);	// CLK as output
}

static inline void datahi()
{
	// note: do not fiddle with ints, as iecout and iecin
	// alread run interrupt-protected

	// setting DATA hi
	if (satnishi() || is_satna) {
		IEC_DDR &= (uint8_t) ~ _BV(IEC_PIN_DATA);	// DATA as input
		IEC_PORT |= _BV(IEC_PIN_DATA);	// Enable pull-up
	}
	is_dataout = 1;
}

static inline void clkhi()
{
	IEC_DDR &= (uint8_t) ~ _BV(IEC_PIN_CLK);	// CLK as input
	IEC_PORT |= _BV(IEC_PIN_CLK);	// Enable pull-up
}

static inline uint8_t dataislo()
{
	return !(IEC_INPUT & _BV(IEC_PIN_DATA));
}

static inline uint8_t dataishi()
{
	return (IEC_INPUT & _BV(IEC_PIN_DATA));
}

static inline uint8_t clkislo()
{
	return !(IEC_INPUT & _BV(IEC_PIN_CLK));
}

static inline uint8_t clkishi()
{
	return (IEC_INPUT & _BV(IEC_PIN_CLK));
}

// returns a debounced port byte, to be checked with 
// the methods is_port_*(port_byte)
// only check for changes in the actual IEC bus lines
static inline uint8_t read_debounced()
{
	uint8_t port;

	do {
		port = IEC_INPUT;
	} while ((port ^ IEC_INPUT) & (_BV(IEC_PIN_CLK) | _BV(IEC_PIN_DATA)));

	return port;
}

static inline uint8_t is_port_clklo(uint8_t port)
{
	return !(port & _BV(IEC_PIN_CLK));
}

static inline uint8_t is_port_clkhi(uint8_t port)
{
	return port & _BV(IEC_PIN_CLK);
}

static inline uint8_t is_port_datahi(uint8_t port)
{
	return port & _BV(IEC_PIN_DATA);
}

static inline uint8_t is_port_datalo(uint8_t port)
{
	return !(port & _BV(IEC_PIN_DATA));
}

// ATNA handling
// (ATN acknowledge logic)

// acknowledge ATN
static inline void satnahi()
{
	is_satna = 0;
}

// disarm ATN acknowledge handling
static inline void satnalo()
{
	is_satna = 1;
	if (is_dataout) {
		datahi();
	}
}

static inline uint8_t satna()
{
	return !is_satna;
}

// general functions

void iechw_init();

// resets the IEEE hardware after a transfer
void iechw_setup();

#endif
