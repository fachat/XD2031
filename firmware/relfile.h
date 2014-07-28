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

/**
 * This file implements the disk drive commands
 */


#ifndef RELFILE_H
#define RELFILE_H



void relfile_init();


// proxies a relative file through the bufcmd layer
int8_t relfile_open(endpoint_t **ep, uint8_t channel_no, uint16_t reclen);

// execute a P command
int8_t relfile_position(bus_t *bus, char *cmdpars, uint8_t namelen, errormsg_t *errormsg);

// wraps the opened channel on the original real_endpoint through the
// relative file provider, when an "CBM_ERROR_OPEN_REL" is received from the 
// server.
int8_t relfile_proxy(uint8_t channel_no, endpoint_t *real_endpoint, uint16_t reclen);

#endif
