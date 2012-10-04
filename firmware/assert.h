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

//    assertion macros


#ifndef ASSERT_H
#define ASSERT_H

#include "debug.h"
#include "mem.h"

#define	assert_not_null(p,m)	 				\
	do {							\
		if ((p) == NULL) {				\
			debug_puts((m));			\
		}						\
	} while(0)
			

#endif
	