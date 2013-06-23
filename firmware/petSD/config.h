/**************************************************************************

    config.h -- common configurations

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


#ifndef CONFIG_H
#define CONFIG_H

//#define DEV_ADDR	9	// default device address

#define FAT_MAX_FILES	4	// max. opened files for FAT provider (SD card)

#define F_CPU		18432000UL

// buffer sizes
#define CONFIG_COMMAND_BUFFER_SIZE      120
#define CONFIG_ERROR_BUFFER_SIZE        46

// number of direct buffers (for U1/U2/B-* commands)
#define CONFIG_NUM_DIRECT_BUFFERS       4


#endif

