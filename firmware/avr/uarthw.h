
/***************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2012 Andre Fachat

    Inspired by uart.c from XS-1541, but rewritten in the end.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation;
    version 2 of the License ONLY.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

****************************************************************************/

/**
 * interface from serial code to uart low level interrupt driver
 */

#ifndef UARTHW_H
#define	UARTHW_H


/***********************************************************************************
 * UART stuff
 */

#define  BAUD    115200

/**
 * initialize the UART 
 */
void uarthw_init();

/**
 * when returns true, there is space in the uart send buffer,
 * so uarthw_send(...) can be called with a byte to send
 */
int8_t uarthw_can_send();

/**
 * submit a byte to the send buffer
 */
void uarthw_send(int8_t data);

/**
 * receive a byte from the receive buffer
 * Returns -1 when no byte available
 */
int16_t uarthw_receive();

#endif

