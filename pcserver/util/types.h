/****************************************************************************

    Serial line filesystem server
    type definitions
    Copyright (C) 2012 Andre Fachat

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

#ifndef TYPES_H
#define TYPES_H

typedef struct _type type_t;

// more or less a class definition type
struct _type {
	const char *name;
	unsigned int sizeoftype;
	void (*constructor) (const type_t * type, void *obj);
	//type_t                *parent;        // super class - used in constructor call
};

typedef int bool_t;

#endif
