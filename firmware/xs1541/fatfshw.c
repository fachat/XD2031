/**************************************************************************

    fatfshw.c -- hardware dependant routines for FatFs

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
#include "fatfshw.h"

inline __attribute__((always_inline)) void slow_spi_clk(void) {
    /* Set SPI clock to 18.432 MHz / 64 = 288 kHz  during card init */
    /* have a look at doc/avr_spi_freq.ods */
    SPSR = _BV(SPI2X);
    SPCR |= _BV(SPR1) | _BV(SPR0);
}

inline __attribute__((always_inline)) void fast_spi_clk(void) {
    /* Set SPI clock to 18.432 kHz / 8 = 2.304 MHz after card init */
    /* have a look at doc/avr_spi_freq.ods */
    SPSR = _BV(SPI2X);
    SPCR &= ~_BV(SPR1);
    SPCR |= _BV(SPR0);
}

/*-----------------------------------------------------------------------*/
/* Power Control  (Platform dependent)       */
/*-----------------------------------------------------------------------*/
/* When the target system does not support socket power control, there   */
/* is nothing to do in these functions and chk_power always returns 1.   */

#define power_status(x) 1

inline __attribute__((always_inline)) void power_on() {                                     
    SPI_PORT |= _BV(SPI_PIN_SCK) | _BV(SPI_PIN_MOSI);   // SCK/MOSI as output
    SPI_DDR |= _BV(SPI_PIN_SCK) | _BV(SPI_PIN_MOSI);

    PORT_SD_CS |= _BV(PIN_SD_CS);               // SD chip select = high
    DDR_SD_CS |= _BV(PIN_SD_CS);                // SD_CS as output
                                      
    SPCR = 0x52;            /* Enable SPI function in mode 0 */ 
    SPSR = 0x01;            /* SPI 2x mode */           
}

inline __attribute__((always_inline)) void power_off(void)
{
#if 0
    SPCR = 0;       /* Disable SPI function */

    DDRB  &= ~0b00110111;   /* Set SCK/MOSI/CS as hi-z, INS#/WP as pull-up */
    PORTB &= ~0b00000111;
    PORTB |=  0b00110000;
#endif
}

/*-----------------------------------------------------------------------*/
/* Transmit/Receive data from/to MMC via SPI  (Platform dependent)   */
/*-----------------------------------------------------------------------*/

/* Exchange a byte */
inline __attribute__((always_inline)) 
uint8_t xchg_spi (      /* Returns received data */
    uint8_t dat         /* Data to be sent */
)
{
    SPDR = dat;
    loop_until_bit_is_set(SPSR, SPIF);
    return SPDR;
}

/* Send a data block */
inline __attribute__((always_inline)) 
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
inline __attribute__((always_inline))   
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

