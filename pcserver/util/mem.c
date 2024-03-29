/****************************************************************************

    Serial line filesystem server - memory handling
    Copyright (C) 2012,2018 Andre Fachat

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
#include <unistd.h>

#ifndef __APPLE__
#include <malloc.h>
#endif

#include "mem.h"
#include "log.h"


#define	DEBUG_MEM
#undef	DEBUG_MEM_VERBOSE

#ifdef DEBUG_MEM
// establish place for a MAGIC value before each payload
#define	MEM_MAGIC	0xafafafaf
#define	MEM_ERR		0x0badc0de
#define	MEM_OFFSET	(sizeof(int))
#else
#define	MEM_OFFSET	0
#endif

// --------------------------------------------------------------------------------
// min alloc size to allow for setting of int value on free

static const size_t MIN_LEN = sizeof(int);
static inline size_t min_alloc(size_t size) {
	return (size > MIN_LEN ? size : MIN_LEN) + MEM_OFFSET;
}

// --------------------------------------------------------------------------------
// memory allocation checker

static const int mem_records_initial = 1000;

typedef struct {
	void *ptr;
	const char *name;
	const char *file;
	const char *(*to_string)(void*);
	int line;
} mem_record_t;

// array to record all allocations
static mem_record_t *mem_records = NULL;
// size of record array in number of records
static int mem_cap = 0;
// behind last written record
static int mem_last = 0;
// first unused entry lies here or behind this rec#
static int mem_tag = 0;

#define check_alloc(ptr, file, line) check_alloc_(ptr, NULL, file, line, NULL)
#define check_alloc_s(ptr, file, line,to_string) check_alloc_(ptr, NULL, file, line, to_string)
#define check_alloc2(ptr, name, file, line) check_alloc_(ptr, name, file, line, NULL)
#define check_alloc_s2(ptr, name, file, line, to_string) check_alloc_(ptr, name, file, line, to_string)
#define check_free(ptr) check_free_(ptr)

const char *to_string(void *p) {
	return (const char*) p;
}

static void check_alloc_(void *ptr, const char *name, char *file, int line, const char* (*to_string)(void*)) {
	if(!ptr) {
		fprintf(stderr, "Could not allocate memory, "
		"file: %s line: %d\n", file, line);
		exit(EXIT_FAILURE);
	}

	if (mem_records == NULL) {
		size_t s = sizeof(mem_record_t) * mem_records_initial;
		mem_records = malloc(s);
		if(!mem_records) {
			fprintf(stderr, "Could not allocate memory of size %ld for alloc table!\n", s);
			exit(EXIT_FAILURE);
		}
		mem_cap = mem_records_initial;
		mem_last = 0;
		mem_tag = 0;
		memset(mem_records, 0, s); 
	}
	// search for first empty slot (from mem_tag)
	while (mem_tag < mem_last && mem_records[mem_tag].ptr != NULL) {
		mem_tag++;
	}

	// either mem_tag == mem_last, which means we just append,
	// or we found an entry
	if (mem_tag == mem_last) {
		// if we are at the last place, check against cap
		if (mem_last >= mem_cap) {
			// we are at the limit!
			size_t oldsize = sizeof(mem_record_t) * mem_cap;
			// double the limit
			mem_cap = mem_cap * 2;
			size_t newsize = sizeof(mem_record_t) * mem_cap;
			mem_records = realloc(mem_records, newsize);
			if(!mem_records) {
				fprintf(stderr, "Could not re-allocate memory of size %ld for alloc table!\n", newsize);
				exit(EXIT_FAILURE);
			}
			// clear out newly allocated memory
			memset(((char*)mem_records) + oldsize, 0, newsize - oldsize);
		}
		mem_last ++;
	}
	mem_records[mem_tag].ptr = ptr;
	mem_records[mem_tag].file = file;
	mem_records[mem_tag].line = line;
	mem_records[mem_tag].name = name;
	mem_records[mem_tag].to_string = to_string;
}

static void check_free_(const void *ptr) {

	if (!ptr) {
		// ignore NULL
		return;
	}

	for (int i = 0; i < mem_last; i++) {
		if (mem_records[i].ptr == ptr) {
#ifdef DEBUG_MEM_VERBOSE
			log_debug("Free memory at %p (from %s:%d, name=%s)\n", ptr, mem_records[i].file, mem_records[i].line, mem_records[i].name);
#endif
			// unalloc
			mem_records[i].ptr = NULL;
			if (i < mem_tag) {
				mem_tag = i;
			}
			if (i + 1 == mem_last) {
				// was last used entry
				mem_last --;
			}
			return;
		}
	}
	log_error("check_free: Trying to free memory at %p that is not allocated (not in list)\n", ptr);
	// fail fast
	exit(-1);
}

// --------------------------------------------------------------------------------

void mem_init (void) {
}

void mem_exit (void) {

	for (int i = 0; i < mem_last; i++) {

		if (mem_records[i].ptr != NULL) {
			fprintf(stderr, "Did not free memory at %p, allocated in %s:%d, name=%s, value='%s'\n", 
					mem_records[i].ptr, mem_records[i].file, mem_records[i].line, mem_records[i].name,
					mem_records[i].to_string ? mem_records[i].to_string(mem_records[i].ptr):"<null>");

		}
	}
}

// --------------------------------------------------------------------------------

// allocate memory and copy given string up to n chars
//#define mem_alloc_strn(s,n) mem_alloc_str_(s, n, __FILE__, __LINE__)
char *mem_alloc_strn_(const char *orig, size_t n, char *file, int line, const char* (*to_string)(void*)) {

	if (orig == NULL) {
		return NULL;
	}

	size_t len = strlen(orig);
	if (len > n) {
		len = n;
	}

	char *ptr = malloc(min_alloc(len+1));

	check_alloc_s(ptr, file, line, to_string);

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
char *mem_alloc_str_(const char *orig, char *name, char *file, int line, const char* (*to_string)(void*)) {

	if (orig == NULL) {
		return NULL;
	}

	int len = strlen(orig);

	char *ptr = malloc(min_alloc(len+1));

	check_alloc_s2(ptr, name, file, line, to_string);
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

	void *ptr = malloc(min_alloc(type->sizeoftype));

	check_alloc2(ptr, type->name, file, line);

	// malloc returns "non-initialized" memory! 
	memset(ptr, 0, type->sizeoftype + MEM_OFFSET);

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
void *mem_alloc_c_(size_t n, const char *name, char *file, int line, const char* (*to_string) (void*)) {
	// for now just malloc()

	//(void) name; // name not used at the moment, silence warning

	void *ptr = malloc(min_alloc(n));

	check_alloc_s2(ptr, name, file, line, to_string);

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

	check_alloc2(ptr, type->name, file, line);

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
	check_free(((char*)ptr) - MEM_OFFSET);
	ptr = realloc(((char*)ptr) - MEM_OFFSET, (n + 1) * type->sizeoftype);
	check_alloc(ptr, file, line);
	ptr = ((char*)ptr) + MEM_OFFSET;
#else
	check_free(ptr);
	ptr = realloc(ptr, n * type->sizeoftype);
	check_alloc(ptr, file, line);
#endif

	return ptr;
}

void mem_free_(const void* ptr) {

#ifdef DEBUG_MEM
	if (ptr != NULL) {

		void *ptr1 = ((char*)ptr) - MEM_OFFSET;
		if ( ((unsigned int*)ptr1)[0] != MEM_MAGIC ) {
			log_error("Trying to free memory at %p that is not allocated (no MAGIC)\n", ptr);
			// fail fast
			exit(-1);
		}
		// overwrite magic
		((int*)ptr1)[0] = 0;

		// overwrite first bytes
		((int*)ptr)[0] = MEM_ERR;

		ptr = ptr1;
	}
#endif
	check_free(ptr);
	free((void*)ptr);

}

/**
 * malloc a new path and copy the given base path and name, concatenating
 * them with the path separator. Ignore base for absolute paths in name.
 */
char *malloc_path_(const char *base, const char *name, char *file, int line) {

#ifdef DEBUG_MEM_VERBOSE
        log_debug("malloc_path: base=%s, name=%s\n", base, name);
#endif

        if(name[0] == '/' || name[0]=='\\') base=NULL;  // omit base for absolute paths
        int l = (base == NULL) ? 0 : strlen(base);
        l += (name == NULL) ? 0 : strlen(name);
        l += 3; // dir separator, terminating zero, optional "."

        char *dirpath = mem_alloc_c_(l, "malloc_path", file, line, to_string);
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

#ifdef DEBUG_MEM_VERBOSE
        log_debug("Calculated new dir path: %s\n", dirpath);
#endif

        return dirpath;
}

/**
 * take all the variable arg chars and append them to the string
 * given to in the first parameter. The string originally pointed
 * to by baseptr will be mem_free'd!
 */
void mem_append_str5(char **baseptr, const char *s1, const char *s2, const char *s3, const char *s4, const char *s5) {

	int newlen = ((*baseptr == NULL) ? 0 :strlen(*baseptr))
		+ ((s1 == NULL) ? 0 : strlen(s1))
		+ ((s2 == NULL) ? 0 : strlen(s2))
		+ ((s3 == NULL) ? 0 : strlen(s3))
		+ ((s4 == NULL) ? 0 : strlen(s4))
		+ ((s5 == NULL) ? 0 : strlen(s5))
		+ 1;

	char *base = mem_alloc_c(newlen, "mem_append");
	base[0] = 0;

	if (*baseptr != NULL) { strcpy(base, *baseptr); };
	if (s1 != NULL) { strcat(base, s1); }
	if (s2 != NULL) { strcat(base, s2); }
	if (s3 != NULL) { strcat(base, s3); }
	if (s4 != NULL) { strcat(base, s4); }
	if (s5 != NULL) { strcat(base, s5); }
	
	mem_free(*baseptr);
	*baseptr = base;
}

/**
 * take all the variable arg chars and append them to the string
 * given to in the first parameter. The string originally pointed
 * to by baseptr will be mem_free'd!
 */
void mem_append_str2(char **baseptr, const char *s1, const char *s2) {

	mem_append_str5(baseptr, s1, s2, NULL, NULL, NULL);
}




