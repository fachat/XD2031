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

#include <stdio.h>
#include <string.h>

#include "log.h"
#include "hashmap.h"
#include "provider.h"
#include "endpoints.h"
#include "resolver.h"

// TODO: this is ... awkward
extern provider_t http_provider;
extern provider_t ftp_provider;
extern provider_t di_provider;
extern provider_t fs_provider;
extern provider_t tcp_provider;

//------------------------------------------------------------------------------------
// handling the registered list of providers

typedef struct {
        provider_t      *provider;
	charset_t	native_cset_idx;
} providers_t;

static void providers_init(const type_t *type, void *obj) {
	(void)type; // silence unused warning

	providers_t *p = (providers_t*) obj;

	p->provider = NULL;
}

static type_t providers_type = {
	"providers",
	sizeof(providers_t),
	providers_init
};

static registry_t providers;
static hash_t *provider_map;

int provider_register(provider_t *provider) {

	providers_t *p = mem_alloc(&providers_type);

	p->provider = provider;
	if (provider->native_charset != NULL) {
		p->native_cset_idx = cconv_getcharset(provider->native_charset);
	} else {
		p->native_cset_idx = -1;
	}

	hash_put(provider_map, provider);
	reg_append(&providers, p);

	return 0;
}

provider_t *provider_find(const char *name) {
	return (provider_t*) hash_get(provider_map, name);
}

static const char* name_from_provider(const void *entry) {
	return ((provider_t*)entry)->name;
}


//------------------------------------------------------------------------------------
/**
 * drive is the endpoint number to assign the new provider to.
 * name denotes the actual provider for the given drive/endpoint
 * 
 * of the "A0:=fs:foo/bar" the "0" becomes the drive, "fs" becomes the wirename,
 * and "foo/bar" becomes the assign_to.
 */
int provider_assign(int drive, drive_and_name_t *to_addr, charset_t cset, int from_cmdline) {

	int err = CBM_ERROR_FAULT;

	log_info("Assign provider '%s' with '%s' to drive %d\n", to_addr->drivename, to_addr->name, drive);

	endpoint_t *target = NULL;
	endpoint_t *newep = NULL;

	if (to_addr->name == NULL || strlen((char*)to_addr->name) == 0) {
		endpoints_unassign(drive);
		return CBM_ERROR_OK;
	}

	if (to_addr->drive == NAMEINFO_UNUSED_DRIVE) { // || to_addr->drivename == NULL || strlen(to_addr->drivename) == 0) {
		to_addr->drivename = (uint8_t*) "fs";
		to_addr->drive = NAMEINFO_UNDEF_DRIVE;
	}

	err = resolve_endpoint(to_addr, cset, from_cmdline, &target);
	if (err == CBM_ERROR_OK) {

	    if (strlen((char*)to_addr->name) > 0) {
		file_t *parentdir = target->ptype->root(target);

		err = resolve_dir((const char**)&to_addr->name, cset, &parentdir);
		if (err == CBM_ERROR_OK) {

			file_t *dir = NULL;

			openpars_t pars;
			openpars_init_options(&pars);
			// got the enclosing directory, now get the dir itself
			err = resolve_open(parentdir, to_addr, cset, &pars, FS_OPEN_DR, &dir);

			if (err == CBM_ERROR_OK) {

				err = dir->endpoint->ptype->to_endpoint(dir, &newep);

				if (err != CBM_ERROR_OK) {
					dir->handler->fclose(dir, NULL, NULL);
				}
			} else {
				parentdir->handler->fclose(parentdir, NULL, NULL);
			}
		} 
		
		provider_cleanup(target);
	    } else {
		// make permanent
		target->is_temporary = 0;
		newep = target;
	    }
	}

	if (newep != NULL) {
		// check if the drive is already in use and free it if necessary
		// NOTE: a Map construct would be nice here...

		endpoints_unassign(drive);

		endpoints_assign(drive, newep);

		return CBM_ERROR_OK;
	}

	return err;
}

void provider_cleanup(endpoint_t *ep) {
	if (ep->is_temporary) {
		log_debug("Freeing temporary endpoint %p\n", ep);
		provider_t *prevprov = ep->ptype;
		prevprov->freeep(ep);
	}
}

static void provider_free_entry(registry_t *reg, void *entry) {
	(void)reg;
	((providers_t*)entry)->provider->free();
	mem_free(entry);
}

void provider_free() {

	endpoints_free();

	reg_free(&providers, provider_free_entry);
}

void provider_init() {

	reg_init(&providers, "providers", 10);

	provider_map = hash_init_stringkey_nocase(10, 7, name_from_provider);

	endpoints_init();

        // manually handle the initial provider
        fs_provider.init();

        ftp_provider.init();
	provider_register(&ftp_provider);

        http_provider.init();
	provider_register(&http_provider);

#ifndef _WIN32
        tcp_provider.init();
	provider_register(&tcp_provider);
#endif

	// registers itself
        di_provider.init();
}


/**
 * provider_chdir uses the XD2031 name format, i.e. first byte is drive
 * (or NAMEINFO_UNDEF_DRIVE), rest until the zero-byte is file name.
 * It then identifies the drive, puts the CD path before the name if it
 * is not absolute, and allocates the new name that it returns
 */
#if 0
int provider_chdir(const char *inname, int namelen, charset_t cset) {

	int drive = inname[0];
	inname++;
	namelen--;

	if (namelen <= 0) {
		inname = NULL;
	}

	if (drive == NAMEINFO_UNDEF_DRIVE) {
		return CBM_ERROR_FAULT;
	}

	log_debug("Trying to resolve drive %d with path '%s'\n", drive, inname);

	ept_t *ept = endpoints_find(drive);

	if (ept == NULL) {
		// drive number not found
		return CBM_ERROR_DRIVE_NOT_READY;
	}

	const char *newpath = malloc_path(ept->cdpath, inname);
	const char *path = NULL;

	int rv = handler_resolve_path(ept->ep, newpath, cset, &path);

	if (rv == CBM_ERROR_OK) {
		mem_free(ept->cdpath);
		ept->cdpath = path;
	}
	return rv;
}
#endif

/*
 * dump the in-memory structures (for analysis / debug)
 */
void provider_dump() {
	int indent = 1;
	const char *prefix = dump_indent(indent);
	const char *eppref = dump_indent(indent+1);

	endpoints_dump(prefix, eppref);

	for (int i = 0; ; i++) {
		providers_t *p = reg_get(&providers, i);
		if (p != NULL) {
			log_debug("%s// Dumping provider %s\n", prefix, p->provider->name);
			log_debug("%s{\n", prefix);
			log_debug("%sname='%s';\n", eppref, p->provider->name);
			log_debug("%snative_charset='%s';\n", eppref, p->provider->native_charset);
			log_debug("%sprovider={\n", eppref);
			if (p->provider->dump != NULL) {
				p->provider->dump(indent+2);
			}
			log_debug("%s}\n", eppref);
			log_debug("%s}\n", prefix);
		} else {
			// no provider left
			break;
		}
	}
}

