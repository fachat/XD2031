/**************************************************************************

    fatfshw.h -- hardware dependant definitions for FatFs

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


#ifndef FATFSHW_H
#define FATFSHW_H

/* ---- SPI --------------------------------------------------------------- */
#define SPI_PORT                PORTB           /* SPI port */
#define SPI_DDR                 DDRB
#define SPI_PIN_SCK             PB7
#define SPI_PIN_MISO            PB6
#define SPI_PIN_MOSI            PB5

/* ---- Ethernet ---------------------------------------------------------- */
#define PORT_ETH_CS             PORTC           /* Ethernet chip select */
#define DDR_ETH_CS              DDRC
#define PIN_ETH_CS              PC4

#define PORT_ETH_INT            PORTD           /* Ethernet interrupt */
#define DDR_ETH_INT             DDRD
#define PIN_ETH_INT             PD3

/* ---- SD card ----------------------------------------------------------- */
#define PORT_SD_CS              PORTB           /* SD card select */
#define DDR_SD_CS               DDRB
#define PIN_SD_CS               PB4

#define INPUT_SD_WP             PINC            /* SD card write protect */
#define PORT_SD_WP              PORTC
#define DDR_SD_WP               DDRC
#define PIN_SD_WP               PC3
#define SOCKWP                  (INPUT_SD_WP & _BV(PIN_SD_WP))  
/* Write protected. yes:true, no:false, default:false */

#define INPUT_SD_CD             PIND            /* SD card detect */
#define PORT_SD_CD              PORTD
#define DDR_SD_CD               DDRD
#define PIN_SD_CD               PD4
#define SOCKINS                 (!(INPUT_SD_CD & _BV(PIN_SD_CD)))       
/* Card detected?   yes:true, no:false, default:true */

/* ---- I2C bus ----------------------------------------------------------- */
#define IIC_INIT()      // DDRE &= 0xF3; PORTE &= 0xF3  /* Set SCL/SDA as hi-z */
#define SCL_LOW()       // DDRE |=      0x04                    /* SCL = LOW */
#define SCL_HIGH()      // DDRE &=      0xFB                    /* SCL = High-Z */
#define SCL_VAL         0 // ((PINE & 0x04) ? 1 : 0)    /* SCL input level */
#define SDA_LOW()       // DDRE |=      0x08                    /* SDA = LOW */
#define SDA_HIGH()      // DDRE &=      0xF7                    /* SDA = High-Z */
#define SDA_VAL         0 // ((PINE & 0x08) ? 1 : 0)    /* SDA input level */
#define I2C_BB_DELAY_US		6

/* ---- Prototypes -------------------------------------------------------- */
#define power_status(x) 1
void power_on(void);
void power_off(void);
void slow_spi_clk(void);
void fast_spi_clk(void);
uint8_t xchg_spi (uint8_t dat);
void xmit_spi_multi (const uint8_t *p, uint16_t cnt);   /* Send a data block */
void rcvr_spi_multi ( uint8_t *p, uint16_t cnt);        /* Receive a data block */

#endif // FATFSHW_H
