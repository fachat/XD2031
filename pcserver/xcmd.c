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

/**
 manages the -X command line options, which are sent to the device on
 start and when the device resets.
 Note that the registry does not copy the options, just stores the pointers
 into the argv[] array.
*/

#include "types.h"
#include "mem.h"
#include "xcmd.h"
#include "log.h"

#define	INITIAL_CAPACITY	10

static type_t mem_type = {
	"x-option-string-pointer",
	sizeof(const char *),
	NULL
};

// array of pointers to option strings
static const char **optsarray = NULL;

// capacity of array
static int capacity;
// number of entries in the array
static int length;

// init the command line option registry
// TODO: move to registry_t / array_list_t
void xcmd_init() {

	length = 0;
	capacity = INITIAL_CAPACITY;

	optsarray = mem_alloc_n(capacity, &mem_type);
}

void xcmd_free() {

	for (int i = 0; i < length; i++) {
		mem_free(optsarray[i]);
	}
	mem_free(optsarray);
}


// register a new -X cmdline option
void xcmd_register(const char *option) {

	log_info("Registering -X command line option '%s'\n", option);

	if (length >= capacity) {
		// need to realloc
		capacity = capacity * 2;
		optsarray = mem_realloc_n(capacity, &mem_type, optsarray);
	}

	optsarray[length] = option;

	length ++;
}


// get the number of registered options
int xcmd_num_options() {

	return length;
}

// get the Nth option
// pos from zero to xcmd_num_options()-1
const char *xcmd_option(int pos) {

	return optsarray[pos];
}

 

