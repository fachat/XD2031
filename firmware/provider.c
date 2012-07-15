/****************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2012 Andre Fachat

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation;
    version 2 of the License ONLY.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

****************************************************************************/

#include <stdio.h>

#include "provider.h"

// currently planned serial, sdcard, iec, ieee
#define	MAX_PROV	4
// all 10 drives can be used
#define	MAX_DRIVES	10

static provider_t 	*default_provider;

static struct {
	char		*name;
	provider_t	*provider;
	uint8_t		is_default;
} provs[MAX_PROV];

static struct {
	int8_t		drive;
	provider_t	*provider;
} drives[MAX_DRIVES];

int8_t provider_assign(uint8_t drive, const char *name) {
	// TODO not implemented yet
	return -1;
}

provider_t* provider_lookup(uint8_t drive) {

	for (int8_t i = MAX_DRIVES-1; i >= 0; i--) {
		if (drives[i].drive == drive) {
			return drives[i].provider;
		}
	}
	return default_provider;
}

uint8_t provider_register(const char *name, provider_t *provider, uint8_t is_default) {
	for (int8_t i = MAX_PROV-1; i >= 0; i--) {
		if (provs[i].name == NULL) {
			provs[i].name = name;
			provs[i].provider = provider;

			if (is_default) {
				default_provider = provider;
			}
			break;
		}
	}
	return 0;
}

void provider_init(void) {
	for (int8_t i = MAX_PROV-1; i >= 0; i--) {
		provs[i].provider = NULL;
		provs[i].name = NULL;
	}
	for (int8_t i = MAX_DRIVES-1; i >= 0; i--) {
		drives[i].provider = NULL;
		drives[i].drive = -1;
	}
}



