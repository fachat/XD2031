/* 
    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2012  Andre Fachat <afachat@gmx.de>

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
*/

#ifndef OPENPARS_H
#define OPENPARS_H

// ************
// process options from the optional OPEN parameter string
// ************

// further possible options:
// - ignore wrapper, so we see real names (not x00 names for example)
typedef struct {
	uint8_t filetype;
	uint16_t recordlen;
} openpars_t;

/**
 * process options and fill parameter struct
 */
void openpars_process_options(const uint8_t * opts, openpars_t * pars);

/**
 * fill in default values 
 */
void openpars_init_options(openpars_t * pars);

#endif
