/***************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2012 Andre Fachat
    Copyright (C) 2012 Nils Eilers

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
 * high level serial line driver - adapting the packets to send and receive
 * to the hardware-specific uart drivers
 */

#ifndef FAT_PROVIDER_H
#define	FAT_PROVIDER_H

#include "provider.h"

extern provider_t fat_provider;

#if _USE_LFN
	char Lfname[_MAX_LFN+1];
#endif

#endif // FAT_PROVIDER_H

