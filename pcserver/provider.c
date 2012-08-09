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

#include "log.h"
#include "provider.h"
#include "errors.h"


// TODO: this is ... awkward
extern provider_t http_provider;
extern provider_t ftp_provider;
extern provider_t fs_provider;

//------------------------------------------------------------------------------------
// Mapping from drive number, which is given on open and commands, to endpoint
// provider

struct {
        provider_t      *provider;
} providers[MAX_NUMBER_OF_PROVIDERS];

struct {
        int             epno;
        endpoint_t      *ep;
} eptable[MAX_NUMBER_OF_ENDPOINTS];


int provider_register(provider_t *provider) {
        int i;
        for(i=0;i<MAX_NUMBER_OF_ENDPOINTS;i++) {
          	if (providers[i].provider == NULL) {
			providers[i].provider = provider;
			return 0;
		}
        }
	log_error("No space left in provider table for provider %s\n", provider->name);
	return -22;
}

/**
 * drive is the endpoint number to assign the new provider to.
 * name denotes the actual provider for the given drive/endpoint
 */
int provider_assign(int drive, const char *name) {

	log_info("assign '%s' to drive %d\n", name, drive);

	// find end of name
	int p = 0;
	while (name[p] != 0 && name[p] != ':') {
		p++;
	}

	endpoint_t *parent = NULL;
	provider_t *provider = NULL;

	// check if it is a drive
	if ((isdigit(name[0])) && (p == 1)) {
		// we have a drive number
		int drv = name[0] & 0x0f;
		parent = provider_lookup(drv);
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
				int j = 0;
				for (j = 0; j < p; j++) {
					if (name[j] != pname[j]) {
						// provider and assign name do not match
						break;
					}
				}
				if ((j == p) && (pname[p] == 0)) {
					// got one
					provider = providers[i].provider;
					log_info("Found provider named '%s'\n", provider->name);
					break;
				}
			}
		}
	}

	endpoint_t *newep = NULL;
	if (provider != NULL) {
		// get new endpoint
		newep = provider->newep(parent, (name[p] == 0) ? name + p : name + p + 1);
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
		return ERROR_OK;
	}
	return ERROR_FAULT;
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

endpoint_t *provider_lookup(int drive) {
        int i;

        for(i=0;i<MAX_NUMBER_OF_ENDPOINTS;i++) {
                if (eptable[i].epno == drive) {
                        return eptable[i].ep;
                }
        }
        return NULL;
}


