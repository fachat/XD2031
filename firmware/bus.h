/****************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2012 Andre Fachat

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

/*
 * IEEE488 impedance layer
 *
 * provides the parallelsendbyte, parallelreceivebyte and parallelattention
 * methods called by the IEEE loop, and translates these into 
 * calls to the channel framework, open calls etc.
 */

int16_t bus_receivebyte(uint8_t *c, uint8_t newbyte);

int16_t bus_attention(uint8_t cmd);

int16_t bus_sendbyte(uint8_t cmd, uint8_t with_eoi);


