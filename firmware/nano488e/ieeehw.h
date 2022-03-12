/**************************************************************************

    ieeehw.h  -- IEEE-488 bus routines

    This file is part of XD-2031 -- Serial line filesystem server for CBMs

    Copyright (C) 2012 Andre Fachat <afachat@gmx.de>
    Copyrifht (C) 2012 Nils Eilers  <nils.eilers@gmx.de>

    XD-2031 is free software: you can redistribute it and/or
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

**************************************************************************/

#ifndef IEEEHW_H
#define IEEEHW_H

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "device.h"
#include "hwdefines.h"

#include "debug.h"

// IEEE hw code error codes

#define E_OK        0
#define E_ATN       1
#define E_EOI       2
#define E_TIME      4
#define E_BRK       5
#define E_NODEV     6

// ATN handling (input only)

static inline uint8_t atnislo(void)
{
	return !(IEEE_INPUT_ATN & _BV(IEEE_PIN_ATN));
}

static inline uint8_t atnishi(void)
{
	return (IEEE_INPUT_ATN & _BV(IEEE_PIN_ATN));
}

// NDAC & NRFD handling
// Note the order of method definition in this file depends on dependencies

static inline void ndaclo(void)
{
	IEEE_PORT_NDAC &= (uint8_t) ~ _BV(IEEE_PIN_NDAC);	// NDAC low
}

static inline void nrfdlo(void)
{
	IEEE_PORT_NRFD &= (uint8_t) ~ _BV(IEEE_PIN_NRFD);	// NRFD low
}

static inline void ndachi_raw(void)
{
	IEEE_PORT_NDAC |= _BV(IEEE_PIN_NDAC);	// NDAC high
}
static inline void ndachi(void)
{
	ndachi_raw();
}

static inline void nrfdhi_raw(void)
{
	IEEE_PORT_NRFD |= _BV(IEEE_PIN_NRFD);	// NRFD high
}
static inline void nrfdhi(void)
{
	nrfdhi_raw();
}

static inline uint8_t ndacislo(void)
{
	return !(IEEE_INPUT_NDAC & _BV(IEEE_PIN_NDAC));
}

static inline uint8_t ndacishi(void)
{
	return (IEEE_INPUT_NDAC & _BV(IEEE_PIN_NDAC));
}

static inline uint8_t nrfdislo(void)
{
	return !(IEEE_INPUT_NRFD & _BV(IEEE_PIN_NRFD));
}

static inline uint8_t nrfdishi(void)
{
	return (IEEE_INPUT_NRFD & _BV(IEEE_PIN_NRFD));
}

// DAV handling

static inline void davlo(void)
{
	IEEE_PORT_DAV &= (uint8_t) ~ _BV(IEEE_PIN_DAV);	// DAV low
}

static inline void davhi(void)
{
	IEEE_PORT_DAV |= _BV(IEEE_PIN_DAV);	// DAV high
}

static inline uint8_t davislo(void)
{
	return !(IEEE_INPUT_DAV & _BV(IEEE_PIN_DAV));
}

static inline uint8_t davishi(void)
{
	return (IEEE_INPUT_DAV & _BV(IEEE_PIN_DAV));
}

// EOI handling
// Don't drive EOI active high because 75161 changes the
// direction on ATN

static inline void eoilo(void)
{
	IEEE_PORT_EOI &= (uint8_t) ~ _BV(IEEE_PIN_EOI);	// EOI low
	IEEE_DDR_EOI |= (uint8_t) _BV(IEEE_PIN_EOI);	// EOI as output
}

static inline void eoihi(void)
{
	IEEE_DDR_EOI &= (uint8_t) ~ _BV(IEEE_PIN_EOI);	// EOI as input
	IEEE_PORT_EOI |= (uint8_t) _BV(IEEE_PIN_EOI);	// Enable pull-up
}

static inline uint8_t eoiislo(void)
{
	return !(IEEE_INPUT_EOI & _BV(IEEE_PIN_EOI));
}

static inline uint8_t eoiishi(void)
{
	return (IEEE_INPUT_EOI & _BV(IEEE_PIN_EOI));
}

// TE handling

static inline void telo(void)
{
	IEEE_PORT_TE &= (uint8_t) ~ _BV(IEEE_PIN_TE);	// TE low (listen)
}

static inline void tehi(void)
{
	IEEE_PORT_TE |= _BV(IEEE_PIN_TE);	// TE high (talk)
}

// ATNA handling
// (ATN acknowledge logic)
// ATNA "follows" ATN. If different, hardware pulls NRFD+NDAC low

static inline void atnahi(void)
{
	IEEE_PORT_ATNA |= _BV(IEEE_PIN_ATNA);	// DC high (device)
}

// acknowledge ATN
static inline void atnalo(void)
{
	IEEE_PORT_ATNA &= (uint8_t) ~ _BV(IEEE_PIN_ATNA);	// DC low (controller)
}

// data handling

static inline void clrd(void)
{
	IEEE_DA_PORT |= IEEE_DA_PINS;
	IEEE_DB_PORT |= IEEE_DB_PINS;
	IEEE_DC_PORT |= IEEE_DC_PINS;
	IEEE_DD_PORT |= IEEE_DD_PINS;
}

static inline uint8_t reorder4(uint8_t in) {
	uint8_t out = 
		((in & 0x01) ? 0x08 : 0)
		| ((in & 0x02) ? 0x04 : 0)
		| ((in & 0x04) ? 0x02 : 0)
		| ((in & 0x08) ? 0x01 : 0)
		;
	return out;	
}

static inline void wrd(uint8_t data)
{
	IEEE_DA_PORT = (IEEE_DA_PIN & ~IEEE_DA_PINS) | (IEEE_DA_PINS & (uint8_t) ~ (data >> IEEE_DA_SHIFTL));
	IEEE_DB_PORT = (IEEE_DB_PIN & ~IEEE_DB_PINS) | (IEEE_DB_PINS & (uint8_t) ~ (data >> IEEE_DB_SHIFTL));
	IEEE_DC_PORT = (IEEE_DC_PIN & ~IEEE_DC_PINS) | (IEEE_DC_PINS & (uint8_t) ~ (data >> IEEE_DC_SHIFTL));
	IEEE_DD_PORT = (IEEE_DD_PIN & ~IEEE_DD_PINS) | (IEEE_DD_PINS & (uint8_t) ~ (reorder4(data)));
}

static inline uint8_t rdd(void)
{
	return (uint8_t) ~ (
		((IEEE_DA_PINS & IEEE_DA_PIN) << IEEE_DA_SHIFTL)
		| ((IEEE_DB_PINS & IEEE_DB_PIN) << IEEE_DB_SHIFTL)
		| ((IEEE_DC_PINS & IEEE_DC_PIN) << IEEE_DC_SHIFTL)
		| (reorder4(IEEE_DD_PINS & IEEE_DD_PIN))
		);
}

// general functions

void ieeehw_init(void);

// resets the IEEE hardware after a transfer
void ieeehw_setup(void);

// switch hardware from receive to transmit (to talk)
static inline void settx(void)
{
	IEEE_DDR_NDAC &= (uint8_t) ~ _BV(IEEE_PIN_NDAC);	// NDAC as input
	IEEE_DDR_NRFD &= (uint8_t) ~ _BV(IEEE_PIN_NRFD);	// NRFD as input
	//IEEE_PORT_NDAC |= _BV(IEEE_PIN_NDAC);	// Enable NDAC pull-up
	//IEEE_PORT_NRFD |= _BV(IEEE_PIN_NRFD);	// Enable NRFD pull-up
	davhi();		// Release DAV
	eoihi();		// EOI high
	wrd(0);
	tehi();			// Bus driver => TALK
	IEEE_DDR_DAV |= _BV(IEEE_PIN_DAV);	// DAV as output (high)
	IEEE_DA_DDR |= IEEE_DA_PINS;	// Data as output (high)
	IEEE_DB_DDR |= IEEE_DB_PINS;	// Data as output (high)
	IEEE_DC_DDR |= IEEE_DC_PINS;	// Data as output (high)
	IEEE_DD_DDR |= IEEE_DD_PINS;	// Data as output (high)
}

// switch hardware from transmit to receive (after talk)
// this happens after ATN, so nrfd and ndac are already low
static inline void setrx(void)
{
	IEEE_DDR_DAV &= (uint8_t) ~ _BV(IEEE_PIN_DAV);	// DAV as input
	IEEE_DA_DDR &= ~IEEE_DA_PINS;		// Data as input
	IEEE_DB_DDR &= ~IEEE_DB_PINS;		// Data as input
	IEEE_DC_DDR &= ~IEEE_DC_PINS;		// Data as input
	IEEE_DD_DDR &= ~IEEE_DD_PINS;		// Data as input
	//IEEE_PORT_DAV |= _BV(IEEE_PIN_DAV);	// Enable DAV pull-up 
	wrd(0);			// Enable data pull-ups
	eoihi();		// Release EOI
	nrfdlo();		// Release NRFD
	ndaclo();		// Release NDAC
	telo();			// Bus driver => LISTEN
	IEEE_DDR_NDAC |= _BV(IEEE_PIN_NDAC);	// NDAC as output
	IEEE_DDR_NRFD |= (uint8_t) _BV(IEEE_PIN_NRFD);	// NRFD as output
}

// switch hardware to idle (same as settx here, but maybe different
// with different hardware
#define setidle() setrx()
#define setidle_listen() setrx()

#endif
