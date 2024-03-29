
/* 
    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2012  Andre Fachat (afachat@gmx.de)

    XD-2031 specific defines 

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

#ifndef VERSION_H
#define VERSION_H

#define SW_NAME			"XD2031"
#define SW_NAME_LOWER		"xd2031"

#define	VERSION			"0.9"	/* <--- update here     */
#define	LONGVERSION		".3"	/* <--- update here     */

// BCD encoded version number, last two digits are microsteps to allow
// introducing new non volatile values without the need to increase the
// x.y.z version
#define VERSION_U32		0x00090201UL	/* <--- update here     */

#endif
