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
 * high level serial line driver - adapting the packets to send and receive
 * to the hardware-specific uart drivers
 */

#ifndef SERIAL_H
#define	SERIAL_H

#include "provider.h"

/***********************************************************************************
 * UART stuff
 */

#define  BAUD    115200

/**
 * initialize the UART 
 */
void serial_init(uint8_t is_default_provider);

/**
 * sync with the server
 */
void serial_sync();

/**
 * flush the data out the UART 
 */
void serial_flush();

/**
 * send/receive data while waiting for some delay
 */
void serial_delay();

/**
 * submit the contents of a buffer to the UART
 * If buffer slot is available, return immediately.
 * Otherwise wait until slot is available
 *
 * Note: submitter must check buf_is_empty() for true or buf_wait_free() 
 * before reuse or freeing the memory!
 */
//void serial_submit(volatile packet_t *buf);

/*****************************************************************************
 * submit a channel rpc call to the UART
 * If buffer slot is available, return immediately.
 * Otherwise wait until slot is available, then return
 * (while data is transferred in the background)
 *
 * Note: submitter must check buf_is_empty() for true or buf_wait_free()
 * before reuse or freeing the memory!
 *
 * callback is called from interrupt context when the response has been
 * received
 */
//void serial_submit_call(int8_t channelno, packet_t *txbuf, packet_t *rxbuf,
//                void (*callback)(int8_t channelno, int8_t errnum));


extern provider_t serial_provider;

#endif

