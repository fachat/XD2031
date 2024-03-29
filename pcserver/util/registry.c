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


#include <string.h>

#include "mem.h"
#include "log.h"
#include "registry.h"


static type_t entries_t = {
	"registry_entry",
	sizeof(void*),
	NULL
};

/*
 * This code wraps a registry for structs (of the same type).
 * The structs are allocated by this code, and filled with zero.
 */

// initialize a registry
void reg_init(registry_t *reg, const char *name, int initial_capacity) {
	reg->numentries = 0;
	reg->capacity = initial_capacity;
	reg->name = name;

	reg->entries = mem_alloc_n(initial_capacity, &entries_t);
}

int reg_size(registry_t *reg) {
	return reg->numentries;
}

// find a given entry
static int reg_find(registry_t *reg, void *entry) {

	for (int i = reg->numentries-1; i >= 0; i--) {
		if (reg->entries[i] == entry) {
			return i;
		}
	}
	return -1;
}


// adds a pre-allocated struct
void reg_append(registry_t *reg, void *ptr) {

	log_debug("Adding entry %p to registry %p (%s, size=%d)\n", ptr, reg, reg->name, reg->numentries);

	if (reg_find(reg, ptr) >= 0) {
		log_error("Trying to add entry %p multiple times to registry %s\n", ptr, reg->name);
		return;
	}

	if (reg->numentries >= reg->capacity) {
		int newcap = reg->capacity * 3. / 2.;
		// just in case reg->capacity is 1 and 1.5 is rounded to 1
		if (newcap <= reg->capacity) {
			newcap = 2 * reg->capacity;
		}
		void **newp = mem_realloc_n(newcap, &entries_t, reg->entries);

		if (newp == NULL) {
			log_error("Could not re-alloc to %d entries for %s\n",
				newcap, reg->name);
			return;
		}
		reg->entries = newp;
		reg->capacity = newcap;
	}
	reg->entries[reg->numentries] = ptr;
	reg->numentries++;
}


// get a struct pointer back using the position as index
// returns NULL if position is behind last entry
void *reg_get(registry_t *reg, int position) {

	if (position >= reg->numentries) {
		return NULL;
	}
	return reg->entries[position];
}

static void reg_remove_int(registry_t *reg, int i) {
	reg->numentries--;
	int size = (reg->numentries - i) * sizeof(void*);
	if (size > 0) {
		memmove(reg->entries+i, reg->entries+i+1, size);
	}
	return;
}

// remove an entry from the registry
// Note: linear with registry size
void reg_remove(registry_t *reg, void *ptr) {

	log_debug("Removing entry %p from registry %p (%s, size=%d)\n", ptr, reg, reg->name, reg->numentries);

	for (int i = reg->numentries-1; i >= 0; i--) {
		if (reg->entries[i] == ptr) {
			reg_remove_int(reg, i);
			return;
		}
	}
	log_error("Unable to remove entry %p from registry %p (%s)\n", ptr, reg, reg->name);
}

// remove an entry from the registry
void reg_remove_pos(registry_t *reg, int pos) {

	log_debug("Removing entry at pos %d from registry %p (%s, size=%d)\n", pos, reg, reg->name, reg->numentries);

	if (pos >= 0 && pos < reg->numentries) {
		reg_remove_int(reg, pos);
		return;
	}
	log_error("Unable to remove entry at pos %d from registry %p (%s)\n", pos, reg, reg->name);
}


// clear out the registry, and free all allocated memory
// Use the given function on each entry left if not NULL
void reg_free(registry_t *reg, void (*entry_free)(registry_t *reg, void *entry)) {

	if (entry_free != NULL) {
		for (int i = reg->numentries-1; i >= 0; i--) {
			if (reg->entries[i] != NULL) {
				entry_free(reg, reg->entries[i]);
				// fail fast, just in case
				reg->entries[i] = NULL;
			}
		}		
	}
	mem_free(reg->entries);
	reg->entries = NULL;
	reg->numentries = 0;
}


