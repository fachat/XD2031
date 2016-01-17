/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2012 Andre Fachat

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
#include <ctype.h>
#include <string.h>

#include "log.h"
#include "provider.h"
#include "errors.h"
#include "wireformat.h"
#include "types.h"
#include "registry.h"
#include "mem.h"
#include "handler.h"

#include "charconvert.h"


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
	charconv_t	to_provider;
	charconv_t	from_provider;
} providers_t;

static void providers_init(const type_t *type, void *obj) {
	(void)type; // silence unused warning

	providers_t *p = (providers_t*) obj;

	p->provider = NULL;
	p->to_provider = NULL;
	p->from_provider = NULL;
}

static type_t providers_type = {
	"providers",
	sizeof(providers_t),
	providers_init
};

static registry_t providers;

int provider_register(provider_t *provider) {

	providers_t *p = mem_alloc(&providers_type);

	p->provider = provider;
	if (provider->native_charset != NULL) {
		p->native_cset_idx = cconv_getcharset(provider->native_charset);
	} else {
		p->native_cset_idx = -1;
	}
	p->to_provider = cconv_identity;
	p->from_provider = cconv_identity;

	reg_append(&providers, p);

	return 0;
}

// return the index of the given provider in the providers[] table
static int provider_index(provider_t *prov) {

	for (int i = 0; ; i++) {
		providers_t *p = reg_get(&providers, i);
		if (p == NULL) {
			return -1;
		}
		if (p->provider == prov) {
			return i;
		}
	}
	return -1;
}

// -----------------------------------------------------------------

typedef struct {
        int             drive;
        endpoint_t      *ep;
	const char	*cdpath;
} ept_t;

static void endpoints_init(const type_t *type, void *obj) {
	(void)type; // silence unused warning

	ept_t *p = (ept_t*) obj;

	p->drive = -1;
	p->ep = NULL;
	p->cdpath = NULL;	
}

static type_t endpoints_type = {
	"endpoints",
	sizeof(ept_t),
	endpoints_init
};

static registry_t endpoints;

static int unassign(int drive) {
	int rv = CBM_ERROR_DRIVE_NOT_READY;
	ept_t *ept = NULL;
        for(int i=0;(ept = reg_get(&endpoints, i)) != NULL;i++) {
               	if (ept->drive == drive) {
			// remove from list
			reg_remove(&endpoints, ept);
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

// -----------------------------------------------------------------
// character set handling

static char *ext_charset_name = NULL;

// get the character set for the external communication (i.e. the wireformat)
const char *provider_get_ext_charset() {
	return ext_charset_name;
}

// set the character set for the external communication (i.e. the wireformat)
// caches the to_provider and from_provider values in the providers[] table
void provider_set_ext_charset(const char *charsetname) {

	log_info("Setting filename communication charset to '%s'\n", charsetname);

	if (ext_charset_name != NULL) {
		mem_free(ext_charset_name );
	}
	ext_charset_name = mem_alloc_str(charsetname);

	charset_t ext_cset_idx = cconv_getcharset(charsetname);

	int i;
	for (i = 0; ; i++) {
		providers_t *p = reg_get(&providers, i);
		if (p == NULL) {
			break;
		}
		if (p->provider != NULL) {
			p->to_provider = cconv_converter(ext_cset_idx, p->native_cset_idx);
			p->from_provider = cconv_converter(p->native_cset_idx, ext_cset_idx);
		}
	}
}

charconv_t provider_convto(provider_t *prov) {
	int idx = provider_index(prov);
	if (idx >= 0) {
		return ((providers_t*)reg_get(&providers, idx))->to_provider;
	} else {
		log_error("Could not find provider %p\n", prov);
	}
	// fallback
	return cconv_identity;
}

charconv_t provider_convfrom(provider_t *prov) {
	int idx = provider_index(prov);
	if (idx >= 0) {
		return ((providers_t*)reg_get(&providers, idx))->from_provider;
	} else {
		log_error("Could not find provider %p\n", prov);
	}
	// fallback
	return cconv_identity;
}

//------------------------------------------------------------------------------------
// wrap a given (raw) file into a container file_t (i.e. a directory), when
// it can be identified by one of the providers - like a d64 file, or a ZIP file
file_t *provider_wrap(file_t *file) {

	for (int i = 0; ; i++) {
		providers_t *p = reg_get(&providers, i);
		if (p == NULL) {
			return NULL;
		}
		if (p->provider->wrap != NULL) {
			file_t *outfile = NULL;
			int err = p->provider->wrap(file, &outfile);
			if (err == CBM_ERROR_OK) {
				return outfile;
			}
		}
	}
	return NULL;
}


//------------------------------------------------------------------------------------
/**
 * drive is the endpoint number to assign the new provider to.
 * name denotes the actual provider for the given drive/endpoint
 * 
 * of the "A0:fs=foo/bar" the "0" becomes the drive, "fs" becomes the wirename,
 * and "foo/bar" becomes the assign_to.
 */
int provider_assign(int drive, const char *wirename, const char *assign_to, int from_cmdline) {

	int err = CBM_ERROR_FAULT;

	log_info("Assign provider '%s' with '%s' to drive %d\n", wirename, assign_to, drive);

	endpoint_t *parent = NULL;
	provider_t *provider = NULL;
	endpoint_t *newep = NULL;

	if (assign_to == NULL) {
		return unassign(drive);
	}

	int len = strlen(wirename);

	if (len == 0) {
		char *assign_to_rooted = NULL;
		// we don't actually have a provider name - use the default provider
		parent = fs_root_endpoint(assign_to, &assign_to_rooted, from_cmdline);
		if (parent != NULL) {
			provider = parent->ptype;
			log_debug("Got default provider %p ('%s')\n", provider, provider->name);

			err = handler_resolve_assign(parent, &newep, assign_to);
	                if (err != CBM_ERROR_OK || newep == NULL) {
        	                log_error("resolve path returned err=%d, p=%p\n", err, newep);
                	        return err;
                	}
		}
		if (assign_to_rooted != NULL) {
			mem_free(assign_to_rooted);
		}
		if (newep != NULL) {
			if (from_cmdline) {
				newep->is_temporary = 0;
			}
		}
		// if not found, something's clearly wrong, as the default provider "fs"
		// should be there. So return FAULT...
	} else
	if ((isdigit(wirename[0])) && (len == 1)) {
		// check if it is a drive
		// (works as long as isdigit() is the same for all available char sets)
		// we have a drive number
		int drv = wirename[0] & 0x0f;
		char drvname[2];
		drvname[0] = drv;
		drvname[1] = 0;
		parent = provider_lookup(drvname, len, NULL, NAMEINFO_UNDEF_DRIVE);
		if (parent != NULL) {
			provider = parent->ptype;
			log_debug("Got drive number: %d, with provider %p\n", drv, provider);

			err = handler_resolve_assign(parent, &newep, assign_to);
	                if (err != CBM_ERROR_OK || newep == NULL) {
        	                log_error("resolve path returned err=%d, p=%p\n", err, newep);
                	        return err;
                	}
		} else {
			// did not find drive number on lookup
			err = CBM_ERROR_DRIVE_NOT_READY;
		}
	} else {

		char *ascname = mem_alloc_str(wirename);
		cconv_converter(cconv_getcharset(provider_get_ext_charset()), CHARSET_ASCII)
				(wirename, len, ascname, len);

		log_debug("Provider=%p\n", provider);

		if (provider == NULL) {
			// check each of the providers in turn
			for (int i = 0; ; i++) {
				providers_t *p = reg_get(&providers, i);
				if (p != NULL) {
					log_debug("Compare to provider '%s'\n", p->provider->name);
					const char *pname = p->provider->name;
					if (!strcmp(pname, ascname)) {
						// got one
						provider = p->provider;
						if (p->provider->newep != NULL) {
							log_debug("Found provider named '%s'\n", 
									provider->name);
						} else {
							log_warn("Ignoring assign to a non-root "
									"provider '%s'\n", provider->name);
						}
						break;
					}
				} else {
					break;
				}
			}
		}

		mem_free(ascname);
	
		if (provider != NULL) {
			if (provider->newep == NULL) {
				log_error("Tried to assign an indirect provider %s directly\n",wirename);
				return CBM_ERROR_FAULT;
			}

			// get new endpoint
			newep = provider->newep(parent, assign_to, from_cmdline);
			if (newep) {
				newep->is_temporary = 0;
			} else {
				return CBM_ERROR_FAULT;
			}
		}
	}

	if (newep != NULL) {
		// check if the drive is already in use and free it if necessary
		// NOTE: a Map construct would be nice here...

		unassign(drive);

		newep->is_assigned++;

		// build endpoint list entry
		ept_t *ept = mem_alloc(&endpoints_type);
		ept->drive = drive;
		ept->ep = newep;
		ept->cdpath = mem_alloc_str("/");

		// register new endpoint
		reg_append(&endpoints, ept);

		return CBM_ERROR_OK;
	}
	return CBM_ERROR_FAULT;
}

void provider_cleanup(endpoint_t *ep) {
	if (ep->is_temporary) {
		log_debug("Freeing temporary endpoint %p\n", ep);
		provider_t *prevprov = ep->ptype;
		prevprov->freeep(ep);
	}
}

void provider_init() {

	reg_init(&providers, "providers", 10);

	reg_init(&endpoints, "endpoints", 10);

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
 * provider_lookup uses the XD2031 name format, i.e. first byte is drive
 * (or NAMEINFO_UNDEF_DRIVE), rest until the zero-byte is file name.
 * It then identifies the drive, puts the CD path before the name if it
 * is not absolute, and allocates the new name that it returns
 */
endpoint_t *provider_lookup(const char *inname, int namelen, const char **outname, int default_drive) {

	int drive = inname[0];
	inname++;
	namelen--;

	if (namelen <= 0) {
		inname = NULL;
	}

	if (drive == NAMEINFO_LAST_DRIVE) {
		drive = default_drive;
	}

	if (drive == NAMEINFO_UNDEF_DRIVE) {
		if (inname == NULL || inname[0] == 0) {
			// no name specified, so return NULL (no provider found)
			return NULL;
		}
		// the drive is not specified by number, but by provider name
		char *p = strchr(inname, ':');
		if (p != NULL) {
			// found provider separator

			log_debug("Trying to find provider for: %s\n", inname);

			const char *provname = conv_to_name_alloc(inname, CHARSET_ASCII_NAME);
			unsigned int l = p-(inname);
			p++; // first char after ':'
			for (int i = 0; ; i++) {
				providers_t *pp = reg_get(&providers, i);
				if (pp == NULL) {
					break;
				}
				provider_t *prov = pp->provider;
				if (prov != NULL && (strlen(prov->name) == l)
					&& (strncmp(prov->name, provname, l) == 0)) {
					// we got a provider, but no endpoint yet

					log_debug("Found provider '%s', trying to create temporary endpoint for '%s'\n", 
						prov->name, p);
	
					if (prov->tempep != NULL) {
						endpoint_t *ep = prov->tempep(&p);
						if (ep != NULL) {
							if (outname != NULL) {
								*outname = mem_alloc_str(p);
							}
							log_debug("Created temporary endpoint %p\n", ep);
							ep->is_temporary = 1;
						}
						mem_free(provname);
						return ep;
					} else {
						log_error("Provider '%s' does not support temporary drives\n",
							prov->name);
					}
					mem_free(provname);
					return NULL;
				}
			}
			mem_free(provname);
			log_error("Did not find provider for %s\n", inname);
			return NULL;
		} else {
			log_info("No provider name given for undef'd drive '%s', trying default %d\n", 
									inname, default_drive);
			if (default_drive == NAMEINFO_UNDEF_DRIVE) {
				log_error("No provider found\n");
				return NULL;
			}
			// continue checking the assigned list for this drive
			drive = default_drive;
		}
	}

	log_debug("Trying to resolve drive %d with name '%s'\n", drive, inname);

	ept_t *ept = NULL;
	for(int i=0; (ept = reg_get(&endpoints, i)) != NULL;i++) {
                if (ept->drive == drive) {
			if (outname != NULL) {
				if (inname != NULL) {
					*outname = malloc_path(ept->cdpath, inname);
				} else {
					*outname = NULL;
				}
			}
                        return ept->ep;
                }
        }
	log_warn("Drive %d is not assigned!\n", drive);

        return NULL;
}


/**
 * provider_chdir uses the XD2031 name format, i.e. first byte is drive
 * (or NAMEINFO_UNDEF_DRIVE), rest until the zero-byte is file name.
 * It then identifies the drive, puts the CD path before the name if it
 * is not absolute, and allocates the new name that it returns
 */
int provider_chdir(const char *inname, int namelen) {

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

	ept_t *ept = NULL;
	for(int i=0; (ept = reg_get(&endpoints, i)) != NULL;i++) {
                if (ept->drive == drive) {
			break;	// found it
                }
        }

	if (ept == NULL) {
		// drive number not found
		return CBM_ERROR_DRIVE_NOT_READY;
	}

	const char *newpath = malloc_path(ept->cdpath, inname);
	const char *path = NULL;

	int rv = handler_resolve_path(ept->ep, newpath, &path);

	if (rv == CBM_ERROR_OK) {
		mem_free(ept->cdpath);
		ept->cdpath = path;
	}
	return rv;
}

/*
 * dump the in-memory structures (for analysis / debug)
 */
void provider_dump() {
	int indent = 1;
	const char *prefix = dump_indent(indent);
	const char *eppref = dump_indent(indent+1);

	for (int i = 0; ; i++) {
		ept_t *ept = reg_get(&endpoints, i);
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

