/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2012 Andre Fachat

    Derived from:
    OS/A65 Version 1.3.12
    Multitasking Operating System for 6502 Computers
    Copyright (C) 1989-1997 Andre Fachat

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

****************************************************************************/

/*
 * rs232 interface handling
 */

#ifndef SERIAL_H
#define	SERIAL_H

/**
 * See http://en.wikibooks.org/wiki/Serial_Programming:Unix/termios
 */
int config_ser(serial_port_t serport);

/* search /dev for a virtual serial port 
   Change "device" to it, if exactly one found
   If none found or more than one, exit(1) with error msg */
void guess_device(char** device);

/* open a device (as returned by guess_device or given on the cmdline */
serial_port_t device_open(char *name);

#endif

