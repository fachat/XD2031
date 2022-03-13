/****************************************************************************

    Commodore filesystem server
    Copyright (C) 2012,2018 Andre Fachat

    Derived from:
    OS/A65 Version 1.3.12
    Multitasking Operating System for 6502 Computers
    Copyright (C) 1989-1997 Andre Fachat

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


#include "log.h"
#include "errors.h"
#include "mem.h"
#include "provider.h"
#include "drives.h"


// TODO: need a map and/or iterator 

// -----------------------------------------------------------------

static void ept_init(const type_t *type, void *obj) {
	(void)type; // silence unused warning

	drive_t *p = (drive_t*) obj;

	p->drive = -1;
	p->ep = NULL;
	p->cdpath = NULL;	
}

static type_t drives_type = {
	"drives",
	sizeof(drive_t),
	ept_init
};

static registry_t drives;

void drives_init() {
	reg_init(&drives, "drives", 10);
}

static void provider_free_ep(registry_t *reg, void *entry) {
	(void) reg;
	mem_free(entry);
}


void drives_free() {
	reg_free(&drives, provider_free_ep);
}

drive_t *drive_find(int drive) {
	drive_t *ept = NULL;
	for(int i=0; (ept = reg_get(&drives, i)) != NULL;i++) {
                if (ept->drive == drive) {
                        return ept;
                }
        }
	log_info("Drive %d is not assigned!\n", drive);

        return NULL;
}


int drive_unassign(int drive) {
	int rv = CBM_ERROR_DRIVE_NOT_READY;
	drive_t *ept = NULL;
        for(int i=0;(ept = reg_get(&drives, i)) != NULL;i++) {
               	if (ept->drive == drive) {
			// remove from list
			reg_remove(&drives, ept);
			// clean up
			provider_t *prevprov = ept->ep->ptype;
			prevprov->freeep(ept->ep);
			if (ept->cdpath != NULL) {
				mem_free(ept->cdpath);
			}
			// free it
			mem_free(ept);
			ept = NULL;
			rv = CBM_ERROR_OK;
			break;
               	}
       	}
	return rv;
}

void drive_assign(int drive, endpoint_t *newep) {
	newep->is_assigned++;

	// build endpoint list entry
	drive_t *ept = mem_alloc(&drives_type);
	ept->drive = drive;
	ept->ep = newep;
	ept->cdpath = mem_alloc_str2("/", "root_cd_path");

	// register new endpoint
	reg_append(&drives, ept);
}

void drives_dump(const char *prefix, const char *eppref) {
	for (int i = 0; ; i++) {
		drive_t *ept = reg_get(&drives, i);
		if (ept != NULL) {
			log_debug("%s// Dumping endpoint for drive %d\n", prefix, ept->drive);
			log_debug("%s{\n", prefix);
			log_debug("%sdrive=%d;\n", eppref, ept->drive);
			log_debug("%scdpath='%s';\n", eppref, ept->cdpath);
			log_debug("%sendpoint=%p ('%s');\n", eppref, ept->ep, 
								ept->ep->ptype->name);
			log_debug("%s}\n", prefix);
		} else {
			break;
		}
	}
}


