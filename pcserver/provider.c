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

#include "charconvert.h"


// TODO: this is ... awkward
extern provider_t tcp_provider;
extern provider_t http_provider;
extern provider_t ftp_provider;
extern provider_t fs_provider;
extern provider_t di_provider;

//------------------------------------------------------------------------------------
// Mapping from drive number, which is given on open and commands, to endpoint
// provider

struct {
        provider_t      *provider;
	charset_t	native_cset_idx;
	charconv_t	to_provider;
	charconv_t	from_provider;
} providers[MAX_NUMBER_OF_PROVIDERS];

struct {
        int             epno;
        endpoint_t      *ep;
} eptable[MAX_NUMBER_OF_ENDPOINTS];


int provider_register(provider_t *provider) {
        int i;
        for(i=0;i<MAX_NUMBER_OF_PROVIDERS;i++) {
          	if (providers[i].provider == NULL) {
			providers[i].provider = provider;
			if (provider->native_charset != NULL) {
				providers[i].native_cset_idx = cconv_getcharset(provider->native_charset);
			} else {
				providers[i].native_cset_idx = -1;
			}
			providers[i].to_provider = cconv_identity;
			providers[i].from_provider = cconv_identity;
			return 0;
		}
        }
	log_error("No space left in provider table for provider %s\n", provider->name);
	return -22;
}

// return the index of the given provider in the providers[] table
static int provider_index(provider_t *prov) {
	int i;
	for (i = 0; i < MAX_NUMBER_OF_PROVIDERS;i++) {
		if (providers[i].provider == prov) {
			return i;
		}
	}
	return -1;
}

//------------------------------------------------------------------------------------
// character set handling

static char *ext_charset_name = "ASCII";

// get the character set for the external communication (i.e. the wireformat)
const char *provider_get_ext_charset() {
	return ext_charset_name;
}

// set the character set for the external communication (i.e. the wireformat)
// caches the to_provider and from_provider values in the providers[] table
void provider_set_ext_charset(char *charsetname) {

	log_info("Setting filename communication charset to '%s'\n", charsetname);

	ext_charset_name = charsetname;

	charset_t ext_cset_idx = cconv_getcharset(charsetname);

	int i;
	for (i = 0; i < MAX_NUMBER_OF_ENDPOINTS; i++) {
		if (providers[i].provider != NULL) {
			providers[i].to_provider = cconv_converter(ext_cset_idx, providers[i].native_cset_idx);
			providers[i].from_provider = cconv_converter(providers[i].native_cset_idx, ext_cset_idx);
		}
	}
}

charconv_t provider_convto(provider_t *prov) {
	int idx = provider_index(prov);
	if (idx >= 0) {
		return providers[idx].to_provider;
	} else {
		log_error("Could not find provider %p\n", prov);
	}
	// fallback
	return cconv_identity;
}

charconv_t provider_convfrom(provider_t *prov) {
	int idx = provider_index(prov);
	if (idx >= 0) {
		return providers[idx].from_provider;
	} else {
		log_error("Could not find provider %p\n", prov);
	}
	// fallback
	return cconv_identity;
}


//------------------------------------------------------------------------------------
/**
 * drive is the endpoint number to assign the new provider to.
 * name denotes the actual provider for the given drive/endpoint
 */
int provider_assign(int drive, const char *name, const char *assign_to) {

	log_info("Assign provider '%s' with '%s' to drive %d\n", name, assign_to, drive);

	endpoint_t *parent = NULL;
	provider_t *provider = NULL;

	// check if it is a drive
	if ((isdigit(name[0])) && (strlen(name) == 1)) {
		// we have a drive number
		int drv = name[0] & 0x0f;
		parent = provider_lookup(drv, NULL);
		if (parent != NULL) {
			provider = parent->ptype;
			log_debug("Got drive number: %d, with provider %p\n", drv, provider);
		}
	}

	log_debug("Provider=%p\n", provider);

	if (provider == NULL) {
		// check each of the providers in turn
		for (int i = MAX_NUMBER_OF_PROVIDERS-1; i >= 0; i--) {
			if (providers[i].provider != NULL) {
				log_debug("Compare to provider %s\n", providers[i].provider->name);
				const char *pname = providers[i].provider->name;
				if (!strcmp(pname, name)) {
					// got one
					provider = providers[i].provider;
					log_debug("Found provider named '%s'\n", provider->name);
					break;
				}
			}
		}
	}

	endpoint_t *newep = NULL;
	if (provider != NULL) {
		// get new endpoint
		newep = provider->newep(parent, assign_to);
		if (newep) {
			newep->is_temporary = 0;
		} else {
			return CBM_ERROR_FAULT;
		}
	}

	if (newep != NULL) {
		// check if the drive is already in use and free it if necessary
		 int i;

	        for(i=0;i<MAX_NUMBER_OF_ENDPOINTS;i++) {
                	if (eptable[i].epno == drive) {
				provider_t *prevprov = eptable[i].ep->ptype;
				prevprov->freeep(eptable[i].ep);
                        	eptable[i].ep = NULL;
				eptable[i].epno = -1;
				break;
                	}
        	}

		// register new endpoint
	        for(i=0;i<MAX_NUMBER_OF_ENDPOINTS;i++) {
                	if (eptable[i].epno == -1) {
				eptable[i].ep = newep;
				eptable[i].epno = drive;
				break;
                	}
        	}
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
        int i;
        for(i=0;i<MAX_NUMBER_OF_ENDPOINTS;i++) {
          eptable[i].epno = -1;
        }

        // manually handle the initial provider
        fs_provider.init();
	provider_register(&fs_provider);

        ftp_provider.init();
	provider_register(&ftp_provider);

        http_provider.init();
	provider_register(&http_provider);

#ifndef _WIN32
        tcp_provider.init();
	provider_register(&tcp_provider);
#endif

        di_provider.init();
	provider_register(&di_provider);

        //eptable[0].epno = 0;            // drive 0
        //eptable[0].ep = fs_provider.newep(NULL, ".");

	//eptable[1].epno = 9;		// drive 9
	//eptable[1].ep = fs_provider.newep(NULL, "../tools");

        // test
        //eptable[4].epno = 6;            // drive 6
        //eptable[4].ep = fs_provider.newep(NULL, "..");

        // test
        //eptable[6].epno = 7;            // drive 7
        //eptable[6].ep = ftp_provider.newep(NULL, "zimmers.net/pub/cbm");
}

endpoint_t *provider_lookup(int drive, char **name) {
        int i;

	if (drive == NAMEINFO_UNDEF_DRIVE) {
		if (name == NULL) {
			// no name specified, so return NULL (no provider found)
			return NULL;
		}
		// the drive is not specified by number, but by provider name
		char *p = strchr(*name, ':');
		if (p == NULL) {
			log_error("No provider name given for undef'd drive '%s'\n", *name);
			return NULL;
		}
		log_debug("Trying to find provider for: %s\n", *name);

		unsigned int l = p-(*name);
		p++; // first char after ':'
		for (int i = MAX_NUMBER_OF_PROVIDERS-1; i >= 0; i--) {
			provider_t *prov = providers[i].provider;
			if (prov != NULL && (strlen(prov->name) == l)
				&& (strncmp(prov->name, *name, l) == 0)) {
				// we got a provider, but no endpoint yet

				log_debug("Found provider '%s', trying to create temporary endpoint for '%s'\n", 
					prov->name, p);

				if (prov->tempep != NULL) {
					endpoint_t *ep = prov->tempep(&p);
					if (ep != NULL) {
						*name = p;
						log_debug("Created temporary endpoint %p\n", ep);
						ep->is_temporary = 1;
					}
					return ep;
				} else {
					log_error("Provider '%s' does not support temporary drives\n",
						prov->name);
				}
				return NULL;
			}
		}
		log_error("Did not find provider for %s\n", *name);
		return NULL;
	}

        for(i=0;i<MAX_NUMBER_OF_ENDPOINTS;i++) {
                if (eptable[i].epno == drive) {
                        return eptable[i].ep;
                }
        }
        return NULL;
}


