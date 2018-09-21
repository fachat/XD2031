/****************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2012 Andre Fachat

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
    MA  02110-1301, USA.

****************************************************************************/

#include <inttypes.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>

#include "rtconfig.h"
#include "provider.h"
#include "debug.h"

#undef	DEBUG_PROVIDER

// currently planned serial, sdcard, iec, ieee
#define	MAX_PROV	4

static endpoint_t 	default_provider;

static endpoint_t	temp_provider;

static struct {
	const char	*name;
	provider_t	*provider;
	uint8_t		is_default;
} provs[MAX_PROV];

static struct {
	int8_t		drive;
	endpoint_t	endpoint;
} drives[MAX_DRIVES];

/**
 * The assign process assigns the drive to a provider. I.e. the 
 * drive is so far unused (or will be overwritten), and the code
 * has to determine just from the name, which provider to use.
 *
 * The name starts with an alphanumeric string, followed by a colon.
 * After that, it depends on the provider. If that string is a digit,
 * it is the notation relative to an existing drive's provider.
 * Relative here means that for example a subdirectory can by used.
 * If the string is not a digit, it will be looked up in the list
 * of registered providers. If found, the whole name will be used
 * to retrieve a new endpoint provider. Note that the whole name
 * is used to allow one provider handle/use multiple endpoints e.g.
 * in terms of subdirectory. 
 *
 * If no provider is found, this code returns an error, and the 
 * calling code forwards the request to the default provider that is
 * always returned when no assignment can be found.
 *
 * Note that if an assigned drive is used to assign a new name, the
 * assignment of that drive is removed. This enables the use of the
 * default provider when the assign returns after error, to forward
 * the assign command. Also this enables the use of a server-based
 * provider that is not hidden by an old definition on this device.
 */
int8_t provider_assign(uint8_t drive, const char *name, const char *assign_to) {

	int8_t rv = -1;

#ifdef DEBUG_PROVIDER
	debug_printf("ASSIGN: drive %d to provider '%s' = '%s'\n", drive, name, assign_to); 
	debug_flush();
#endif

	// check drive
	if (drive > 10) {
		return -1;
	}

	provider_t *newprov = NULL;
	void *provdata = NULL;

	if ((isdigit(name[0])) && (name[1] == 0)) {
		// we have a drive digit
		uint8_t drv = assign_to[0] & 0x0f;
		endpoint_t *ep = provider_lookup(drv, NULL);
		if (ep == NULL) {
			//debug_printf("Drive %d not used, cannot do relative assign!\n", drv);
			return -1;
		}
		if (ep == &default_provider) {
			//debug_puts("Default is not registered, forwarding to server\n");
			return -1;
		}
		newprov = ep->provider;
		// currently a relative assignment is not supported
		provdata = newprov->prov_assign(drive, name);
	}

	if (newprov == NULL) {
	    // now check each provider in turn, if the name fits
	    for (int8_t i = MAX_PROV-1; i >= 0; i--) {
		if (provs[i].name != NULL) {
			//debug_printf("Compare with %s\n",provs[i].name);	
			if (!strcmp(provs[i].name, name)) {
				// found it
				//debug_printf("Found new provider: %s in slot %d\n", name, i);
				newprov = provs[i].provider;
				// new get the runtime data
				provdata = newprov->prov_assign(drive, assign_to);
				break;
			}
		}
	    }
	}

	// when we are going to return, remove
	// the old assign mapping
	for (int8_t i = MAX_DRIVES-1; i >= 0; i--) {
		if (drives[i].drive == drive) {
			drives[i].drive = -1;
			drives[i].endpoint.provider->prov_free(drives[i].endpoint.provdata);
			drives[i].endpoint.provider = NULL;
			break;
		}
	}

	if (newprov != NULL) {
		// got one, so register it
		for (int8_t i = MAX_DRIVES-1; i >= 0; i--) {
			if (drives[i].drive == -1) {
				drives[i].drive = drive;
				drives[i].endpoint.provider = newprov;
				drives[i].endpoint.provdata = provdata;
#ifdef DEBUG_PROVIDER
				debug_printf("Register prov %p for drive %d with data %p in slot %d\n",
					newprov, drive, provdata, i);
#endif
				rv = 0;	
				break;
			}
		}
	}
	
	return rv;
}

endpoint_t* provider_lookup(uint8_t drive, const char *name) {

#ifdef DEBUG_PROVIDER
	debug_printf("provider_lookup for drive %d\n", drive);
#endif
	if (drive == NAMEINFO_UNUSED_DRIVE) {
		return &default_provider;
	}

	if (drive == NAMEINFO_UNDEF_DRIVE) {
		if (name == NULL) {
			//debug_puts("ERROR PROVIDER LOOKUP: NAME IS NULL\n");
			return &default_provider;
		}
		char *p = strchr(name, ':');
		if (p != NULL) {
			uint8_t l = (p-name);
			for (int8_t i = MAX_PROV-1; i >= 0; i--) {
				if ((strlen(provs[i].name) == l) 
					&& (strncmp(provs[i].name, name, l) == 0)) {
					// ok, we got a provider, but not an endpoint yet
					//debug_printf("GOT A PROVIDER FOR NAME=%s\n", name);
					// create a temporary provider with NULL endpoint-specific
					// provdata. The provider must, in such cases, interpret
					// a command or open filename as if containing the provdata
					// like in an assign. For example:
					// LOAD"ftp:ftp.foo.com/dir/file",8
					temp_provider.provider = provs[i].provider;
					temp_provider.provdata = NULL;
					return &temp_provider;
				}
			}
					
		}
	}

	for (int8_t i = MAX_DRIVES-1; i >= 0; i--) {
		if ((drives[i].drive == drive) && (drives[i].endpoint.provider != NULL)) {
#ifdef DEBUG_PROVIDER
			debug_printf("found %p in slot %d (default=%p)\n", 
				drives[i].endpoint.provider, i, default_provider.provider);
#endif
			return &(drives[i].endpoint);
		}
	}

#ifdef DEBUG_PROVIDER
	debug_puts("not found, returning default\n");
#endif
	return &default_provider;
}

uint8_t provider_register(const char *name, provider_t *provider) {

#ifdef DEBUG_PROVIDER
	debug_printf("Register provider %p for '%s'\n", provider, name);
#endif

	for (int8_t i = MAX_PROV-1; i >= 0; i--) {
		if (provs[i].name == NULL) {
			provs[i].name = name;
			provs[i].provider = provider;
			break;
		}
	}
	return 0;
}

void provider_set_default(provider_t *prov, void *epdata) {
	default_provider.provider = prov;
	default_provider.provdata = epdata;
}

void provider_init(void) {
	for (int8_t i = MAX_PROV-1; i >= 0; i--) {
		provs[i].provider = NULL;
		provs[i].name = NULL;
	}
	for (int8_t i = MAX_DRIVES-1; i >= 0; i--) {
		drives[i].endpoint.provider = NULL;
		drives[i].drive = -1;
	}
}



