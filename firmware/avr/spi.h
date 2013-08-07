/**************************************************************************

    spi.h -- AVR SPI routines

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

#ifndef SPI_H
#define SPI_H

#include <avr/io.h>
#include "config.h"
#include "device.h"
#include "spihw.h"

/*-----------------------------------------------------------------------*/
/* Transmit/Receive data from/to MMC via SPI  (Platform dependent)   */
/*-----------------------------------------------------------------------*/

/* Exchange a byte */
static inline __attribute__((always_inline)) 
uint8_t xchg_spi (      /* Returns received data */
    uint8_t dat         /* Data to be sent */
)
{
    SPDR = dat;
    loop_until_bit_is_set(SPSR, SPIF);
    return SPDR;
}

/* Send a data block */
static inline __attribute__((always_inline)) 
void xmit_spi_multi (
    const uint8_t *p,   /* Data block to be sent */
    uint16_t cnt        /* Size of data block */
)
{
    do {
        SPDR = *p++; loop_until_bit_is_set(SPSR,SPIF);
        SPDR = *p++; loop_until_bit_is_set(SPSR,SPIF);
    } while (cnt -= 2);
}

/* Receive a data block */
static inline __attribute__((always_inline))   
void rcvr_spi_multi (
    uint8_t *p,         /* Data buffer */
    uint16_t cnt        /* Size of data block */
)
{
    do {
        SPDR = 0xFF; loop_until_bit_is_set(SPSR,SPIF); *p++ = SPDR;
        SPDR = 0xFF; loop_until_bit_is_set(SPSR,SPIF); *p++ = SPDR;
    } while (cnt -= 2);

}

#endif
