/****************************************************************************

    Serial line filesystem server
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

#ifndef REGISTRY_H
#define REGISTRY_H

#include <stdio.h>

#include "types.h"

/*
 * This code wraps a registry for structs (of the same type).
 * The structs are allocated by this code, and filled with zero.
 */

typedef struct _registry registry_t;

struct _registry {
        const char      *name;
        int             numentries;
        int             capacity;
        void            **entries;
};

// initialize a registry
void reg_init(registry_t *reg, const char *name, int initial_capacity);

// adds a pre-allocated struct
void reg_append(registry_t *reg, void *ptr);

// get a struct pointer back using the position as index
// returns NULL if position is behind last entry
void *reg_get(registry_t *reg, int position);

#endif
