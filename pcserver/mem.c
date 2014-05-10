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

/*
 * basically a malloc/free wrapper
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef __APPLE__
#include <malloc.h>
#endif

#include "types.h"
#include "log.h"
#include "mem.h"

void mem_init (void) {
}

#define check_alloc(ptr, file, line) check_alloc_(ptr, file, line)

static void check_alloc_(void *ptr, char *file, int line) {
	if(!ptr) {
		fprintf(stderr, "Could not allocate memory, "
		"file: %s line: %d\n", file, line);
		exit(EXIT_FAILURE);
	}
}

// allocate memory and copy given string up to n chars
//#define mem_alloc_strn(s,n) mem_alloc_str_(s, n, __FILE__, __LINE__)
char *mem_alloc_strn_(const char *orig, size_t n, char *file, int line) {

	size_t len = strlen(orig);
	if (len > n) {
		len = n;
	}

	char *ptr = malloc(len+1);

	check_alloc(ptr, file, line);		
	
	strncpy(ptr, orig, len);

	ptr[len] = 0;

	return ptr;
}

// allocate memory and copy given string
//#define mem_alloc_str(s) mem_alloc_str_(s, __FILE__, __LINE__)
char *mem_alloc_str_(const char *orig, char *file, int line) {

	int len = strlen(orig);

	char *ptr = malloc(len+1);

	check_alloc(ptr, file, line);		
	
	strcpy(ptr, orig);

	return ptr;
}

#define mem_alloc(t) mem_alloc_(t, __FILE__, __LINE__)
void *mem_alloc_(const type_t *type, char *file, int line) {
	// for now just malloc()

	void *ptr = malloc(type->sizeoftype);

	check_alloc(ptr, file, line);

	// malloc returns "non-initialized" memory! 
	memset(ptr, 0, type->sizeoftype);

	if (type->constructor != NULL) {
		type->constructor(type, ptr);
	}

	return ptr;
}

//#define mem_alloc_c(size, name) mem_alloc_c_(size, name, __FILE__, __LINE__)
void *mem_alloc_c_(size_t n, const char *name, char *file, int line) {
	// for now just malloc()

	(void) name; // name not used at the moment, silence warning

	void *ptr = malloc(n);

	check_alloc(ptr, file, line);

	return ptr;
}

// NOTE: does not handle padding, this must be fixed in the type->sizeofstruct value!
void *mem_alloc_n_(const size_t n, const type_t *type, char *file, int line) {
	// for now just malloc()

	void *ptr = calloc(n, type->sizeoftype);

	check_alloc(ptr, file, line);

	return ptr;
}

// NOTE: does not handle padding, this must be fixed in the type->sizeofstruct value!
// NOTE: this does currently not zero-fill the added area when the array size is increased
void *mem_realloc_n_(const size_t n, const type_t *type, void *ptr, char *file, int line) {

	ptr = realloc(ptr, n * type->sizeoftype);

	check_alloc(ptr, file, line);

	return ptr;
}

void mem_free(const void* ptr) {

	free((void*)ptr);

}

