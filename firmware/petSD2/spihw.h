/**************************************************************************

    spihw.h -- hardware dependant SPI routines

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

#include <avr/io.h>
#include "config.h"
#include "device.h"

static inline __attribute__ ((always_inline))
void slow_spi_clk(void)
{
	/* Set SPI clock to 18.432 MHz / 64 = 288 kHz  during card init */
	SPSR = _BV(SPI2X);
	SPCR |= _BV(SPR1) | _BV(SPR0);
}

static inline __attribute__ ((always_inline))
void fast_spi_clk(void)
{
#if 0
	/* Set SPI clock to 18.432 kHz / 8 = 2.304 MHz after card init */
	SPSR = _BV(SPI2X);
	SPCR &= ~_BV(SPR1);
	SPCR |= _BV(SPR0);
#endif
	/* Set SPI clock to 18.432 kHz / 2 = 9.216 MHz after card init */
	SPSR = _BV(SPI2X);
	SPCR &= ~(_BV(SPR0) | _BV(SPR0));
}

static inline __attribute__ ((always_inline))
void spi_init(void)
{
	SPI_PORT |= _BV(SPI_PIN_SCK) | _BV(SPI_PIN_MOSI);	// SCK/MOSI as output
	SPI_DDR |= _BV(SPI_PIN_SCK) | _BV(SPI_PIN_MOSI);
	SPI_DDR &= ~_BV(SPI_PIN_MISO);	// MISO as input
	SPI_PORT |= _BV(SPI_PIN_MISO);	// enable pull-up

	PORT_SD_CS |= _BV(PIN_SD_CS);	// SD chip select = high
	DDR_SD_CS |= _BV(PIN_SD_CS);	// SD_CS as output

	SPCR = 0x52;		/* Enable SPI function in mode 0 */
	SPSR = 0x01;		/* SPI 2x mode */
}

/* When the target system does not support socket power control, there   */
/* is nothing to do in these functions and chk_power always returns 1.   */
static inline __attribute__ ((always_inline))
uint8_t power_status(void)
{
	return 1;
}

static inline __attribute__ ((always_inline))
void power_on(void)
{
}

static inline __attribute__ ((always_inline))
void power_off(void)
{
}
