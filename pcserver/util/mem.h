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

#ifndef MEM_H
#define MEM_H

#include <stdio.h>

#include "types.h"

/*
 * basically a malloc/free wrapper
 */

void mem_init(void);

// use const type to allow freeing immutable strings
void mem_free_(const void *ptr);
static inline void mem_free(const void *ptr) {
	mem_free_(ptr);
	//*ptr = NULL;
}

// alloc single object
void *mem_alloc_(const type_t * type, char *file, int line);
#define mem_alloc(t) mem_alloc_(t, __FILE__, __LINE__)

// alloc multiple object, returning a pointer to an array
void *mem_alloc_n_(size_t n, const type_t * type, char *file, int line);
#define mem_alloc_n(n, type) mem_alloc_n_(n, type, __FILE__, __LINE__)

// re-alloc multiple object, returning a pointer to an array
void *mem_realloc_n_(size_t n, const type_t * type, void *ptr, char *file,
		     int line);
#define mem_realloc_n(n, type, ptr) mem_realloc_n_(n, type, ptr, __FILE__, __LINE__)

// alloc multiple object, returning a pointer to an array
void *mem_alloc_c_(size_t n, const char *name, char *file, int line);
#define mem_alloc_c(size, name) mem_alloc_c_(size, name, __FILE__, __LINE__)

// allocate memory and copy given string
char *mem_alloc_str_(const char *orig, char *file, int line);
#define mem_alloc_str(s) mem_alloc_str_(s, __FILE__, __LINE__)

// allocate memory and copy given string up to n chars
char *mem_alloc_strn_(const char *orig, size_t n, char *file, int line);
#define mem_alloc_strn(s,n) mem_alloc_strn_(s, n, __FILE__, __LINE__)

/**
 * malloc a new path, and copy the given base path and name to it,
 * with a separating char
 */
char *malloc_path_(const char *base, const char *name, char *file, int line);
#define malloc_path(s,n) malloc_path_(s, n, __FILE__, __LINE__)

/**
 * take all the variable arg chars and append them to the string
 * given to in the first parameter. The string originally pointed
 * to by baseptr will be mem_free'd!
 */
void mem_append_str2(char **baseptr, const char *s1, const char *s2);
void mem_append_str5(char **baseptr, const char *s1, const char *s2,
		     const char *s3, const char *s4, const char *s5);


/**
 * called on exit of program. Prints debug info resp. errors 
 * if data structs are still allocated 
 */
void mem_exit();

#endif
