
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

#ifndef IEEEHW_H
#define IEEEHW_H

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

static void nrfdlo();

// output of ATNA
extern uint8_t is_atna;
// last output of NRFD before ATNA handling
extern uint8_t is_nrfdout;
// last output of NDAC before ATNA handling
extern uint8_t is_ndacout;

// ATN handling
// (input only)

static inline uint8_t atnislo()
{
	return !(IEEE_INPUT_ATN & _BV(IEEE_PIN_ATN));
}

static inline uint8_t atnishi()
{
	return (IEEE_INPUT_ATN & _BV(IEEE_PIN_ATN));
}

// NDAC & NRFD handling
// Note the order of method definition in this file depends on dependencies

static inline void ndaclo()
{
	IEEE_PORT_NDAC &= (uint8_t) ~ _BV(IEEE_PIN_NDAC);	// NDAC low
	IEEE_DDR_NDAC |= _BV(IEEE_PIN_NDAC);	// NDAC as output
	is_ndacout = 0;
}

static inline void nrfdlo()
{
	IEEE_PORT_NRFD &= (uint8_t) ~ _BV(IEEE_PIN_NRFD);	// NRFD low
	IEEE_DDR_NRFD |= (uint8_t) _BV(IEEE_PIN_NRFD);	// NRFD as output
	is_nrfdout = 0;
}

static inline void ndachi()
{
	// disable interrupt to avoid race condition
	// of ATN irq between the atnishi() check and
	// setting NDAC lo
	cli();
	if (atnishi() || is_atna) {
		IEEE_DDR_NDAC &= (uint8_t) ~ _BV(IEEE_PIN_NDAC);	// NDAC as input
		IEEE_PORT_NDAC |= _BV(IEEE_PIN_NDAC);	// Enable pull-up
	}
	// allow interrupt again
	sei();
	is_ndacout = 1;
}

static inline void nrfdhi()
{
	// disable interrupt to avoid race condition
	// of ATN irq between the atnishi() check and
	// setting NDAC lo
	cli();
	if (atnishi() || is_atna) {
		IEEE_DDR_NRFD &= (uint8_t) ~ _BV(IEEE_PIN_NRFD);	// NRFD as input
		IEEE_PORT_NRFD |= _BV(IEEE_PIN_NRFD);	// Enable pull-up
	}
	// allow interrupt again
	sei();
	is_nrfdout = 1;
}

static inline void atnhi()
{
	IEEE_DDR_ATN &= (uint8_t) ~ _BV(IEEE_PIN_ATN);	// ATN as input
	IEEE_PORT_ATN |= _BV(IEEE_PIN_ATN);	// Enable pull-up
}

static inline uint8_t ndacislo()
{
	return !(IEEE_INPUT_NDAC & _BV(IEEE_PIN_NDAC));
}

static inline uint8_t ndacishi()
{
	return (IEEE_INPUT_NDAC & _BV(IEEE_PIN_NDAC));
}

static inline uint8_t nrfdislo()
{
	return !(IEEE_INPUT_NRFD & _BV(IEEE_PIN_NRFD));
}

static inline uint8_t nrfdishi()
{
	return (IEEE_INPUT_NRFD & _BV(IEEE_PIN_NRFD));
}

// DAV handling

static inline void davlo()
{
	IEEE_PORT_DAV &= (uint8_t) ~ _BV(IEEE_PIN_DAV);	// DAV low
	IEEE_DDR_DAV |= _BV(IEEE_PIN_DAV);	// DAV as output
}

static inline void davhi()
{
	IEEE_DDR_DAV &= (uint8_t) ~ _BV(IEEE_PIN_DAV);	// DAV as input
	IEEE_PORT_DAV |= _BV(IEEE_PIN_DAV);	// Enable pull-up
}

static inline uint8_t davislo()
{
	return !(IEEE_INPUT_DAV & _BV(IEEE_PIN_DAV));
}

static inline uint8_t davishi()
{
	return (IEEE_INPUT_DAV & _BV(IEEE_PIN_DAV));
}

// EOI handling

static inline void eoilo()
{
	IEEE_PORT_EOI &= (uint8_t) ~ _BV(IEEE_PIN_EOI);	// EOI low
	IEEE_DDR_EOI |= (uint8_t) _BV(IEEE_PIN_EOI);	// EOI as output
}

static inline void eoihi()
{
	IEEE_DDR_EOI &= (uint8_t) ~ _BV(IEEE_PIN_EOI);	// EOI as input
	IEEE_PORT_EOI |= (uint8_t) _BV(IEEE_PIN_EOI);	// Enable pull-up
}

static inline uint8_t eoiislo()
{
	return !(IEEE_INPUT_EOI & _BV(IEEE_PIN_EOI));
}

static inline uint8_t eoiishi()
{
	return (IEEE_INPUT_EOI & _BV(IEEE_PIN_EOI));
}

// ATNA handling
// (ATN acknowledge logic)

// acknowledge ATN
static inline void atnahi()
{
	is_atna = 0;
}

// disarm ATN acknowledge handling
static inline void atnalo()
{
	if (!is_nrfdout) {
		nrfdlo();
	}
	if (!is_ndacout) {
		ndaclo();
	}
	is_atna = 1;
}

// data handling

static inline void clrd(void)
{
	IEEE_D_DDR = 0;
	IEEE_D_PORT = 0xff;
}

static inline void wrd(uint8_t data)
{
	IEEE_D_DDR = data;
	IEEE_D_PORT = (uint8_t) ~ data;
}

static inline uint8_t rdd(void)
{
	return (uint8_t) ~ IEEE_D_PIN;
}

// general functions

void ieeehw_init();

// resets the IEEE hardware after a transfer
void ieeehw_setup();

// switch hardware from receive to transmit (to talk)
static inline void settx(void)
{
	nrfdhi();
	ndachi();
	davhi();
	eoihi();
}

// switch hardware to idle (same as settx here, but maybe different
// with different hardware
static inline void setidle(void)
{
	nrfdhi();
	ndachi();
	davhi();
	eoihi();
}

// switch hardware from transmit to receive (after talk)
// this happens after ATN, so nrfd and ndac are already low
static inline void setrx(void)
{
	davhi();
	eoihi();
}

#endif
