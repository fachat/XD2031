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

#include "os.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef __APPLE__
#include <malloc.h>
#endif

#include "types.h"
#include "log.h"
#include "mem.h"


#define	DEBUG_MEM

#ifdef DEBUG_MEM
#define	MEM_MAGIC	0xbadc0de
#define	MEM_OFFSET	(sizeof(int))
#else
#define	MEM_OFFSET	0
#endif


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

	len+=MEM_OFFSET;

	char *ptr = malloc(len+1);

	check_alloc(ptr, file, line);		

#ifdef DEBUG_MEM
	((int*)ptr)[0] = MEM_MAGIC;
#endif
	ptr+=MEM_OFFSET;

	strncpy(ptr, orig, len);

	ptr[len] = 0;

	return ptr;
}

// allocate memory and copy given string
//#define mem_alloc_str(s) mem_alloc_str_(s, __FILE__, __LINE__)
char *mem_alloc_str_(const char *orig, char *file, int line) {

	int len = strlen(orig);
	len+=MEM_OFFSET;

	char *ptr = malloc(len+1);

	check_alloc(ptr, file, line);		
#ifdef DEBUG_MEM
	((int*)ptr)[0] = MEM_MAGIC;
#endif
	ptr+=MEM_OFFSET;
	
	strcpy(ptr, orig);

	return ptr;
}

#define mem_alloc(t) mem_alloc_(t, __FILE__, __LINE__)
void *mem_alloc_(const type_t *type, char *file, int line) {
	// for now just malloc()

	void *ptr = malloc(type->sizeoftype + MEM_OFFSET);

	check_alloc(ptr, file, line);

	// malloc returns "non-initialized" memory! 
	memset(ptr, 0, type->sizeoftype);

#ifdef DEBUG_MEM
	((int*)ptr)[0] = MEM_MAGIC;
#endif
	ptr = ((char*)ptr) + MEM_OFFSET;
	
	if (type->constructor != NULL) {
		type->constructor(type, ptr);
	}

	return ptr;
}

//#define mem_alloc_c(size, name) mem_alloc_c_(size, name, __FILE__, __LINE__)
void *mem_alloc_c_(size_t n, const char *name, char *file, int line) {
	// for now just malloc()

	(void) name; // name not used at the moment, silence warning

	void *ptr = malloc(n + MEM_OFFSET);

	check_alloc(ptr, file, line);

#ifdef DEBUG_MEM
	((int*)ptr)[0] = MEM_MAGIC;
#endif
	ptr = ((char*)ptr) + MEM_OFFSET;

	return ptr;
}

// NOTE: does not handle padding, this must be fixed in the type->sizeofstruct value!
void *mem_alloc_n_(const size_t n, const type_t *type, char *file, int line) {
	// for now just malloc()

	void *ptr = calloc(n + ((MEM_OFFSET == 0) ? 0 : 1), type->sizeoftype);

	check_alloc(ptr, file, line);

#ifdef DEBUG_MEM
	((int*)ptr)[0] = MEM_MAGIC;
	ptr = ((char*)ptr) + MEM_OFFSET;
#endif

	return ptr;
}

// NOTE: does not handle padding, this must be fixed in the type->sizeofstruct value!
// NOTE: this does currently not zero-fill the added area when the array size is increased
void *mem_realloc_n_(const size_t n, const type_t *type, void *ptr, char *file, int line) {

#ifdef DEBUG_MEM
	ptr = realloc(((char*)ptr) - MEM_OFFSET, (n + 1) * type->sizeoftype);
	check_alloc(ptr, file, line);
	ptr = ((char*)ptr) + MEM_OFFSET;
#else
	ptr = realloc(ptr, n * type->sizeoftype);
	check_alloc(ptr, file, line);
#endif

	return ptr;
}

void mem_free(const void* ptr) {

#ifdef DEBUG_MEM
	if (ptr != NULL) {
		ptr = ((char*)ptr) - MEM_OFFSET;
		if ( ((int*)ptr)[0] != MEM_MAGIC ) {
			log_error("Trying to free memory at %p that is not allocated\n", ptr);
			return;
		}
	}
#endif
	free((void*)ptr);

}

/**
 * malloc a new path and copy the given base path and name, concatenating
 * them with the path separator. Ignore base for absolute paths in name.
 */
char *malloc_path(const char *base, const char *name) {

        log_debug("malloc_path: base=%s, name=%s\n", base, name);

        if(name[0] == '/' || name[0]=='\\') base=NULL;  // omit base for absolute paths
        int l = (base == NULL) ? 0 : strlen(base);
        l += (name == NULL) ? 0 : strlen(name);
        l += 3; // dir separator, terminating zero, optional "."

        char *dirpath = mem_alloc_c(l, "malloc_path");
        dirpath[0] = 0;
        if (base != NULL) {
                strcat(dirpath, base);
		if ((dirpath[0] != 0) && (dirpath[strlen(dirpath)-1] != '/')) {
			// base path does not end with dir separator
	                strcat(dirpath, "/");   // TODO dir separator char
		}
        }
        if (name != NULL) {
                strcat(dirpath, name);
        }

        log_debug("Calculated new dir path: %s\n", dirpath);

        return dirpath;
}

