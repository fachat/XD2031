/***************************************************************************

    XD-2031 - Serial line filesystem server for CBMs
    Copyright (C) 2012 Andre Fachat
    Copyright (C) 2012 Nils Eilers

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

/**
 * high level serial line driver - adapting the packets to send and receive
 * to the hardware-specific uart drivers
 */

#ifndef FAT_PROVIDER_H
#define	FAT_PROVIDER_H

#include "provider.h"

extern provider_t fat_provider;

#ifdef FAT_PROVIDER_C

/* ----- Definitions ------------------------------------------------------------------------- */

#define AVAILABLE	-1

enum enum_dir_state { DIR_INACTIVE = 0, DIR_HEAD, DIR_FILES, DIR_FOOTER };

/* ----- Prototypes -------------------------------------------------------------------------- */

// Glue to firmware
static void *prov_assign(const char *name);
static void prov_free(void *epdata);
static void fat_submit(void *epdata, packet_t *buf);
static void fat_submit_call(void *epdata, int8_t channelno, packet_t *txbuf, packet_t *rxbuf,
                uint8_t (*callback)(int8_t channelno, int8_t errnum));
static int8_t directory_converter(packet_t *p, uint8_t drive);
static int8_t to_provider(packet_t *p);


// helper functions
static int8_t fs_read_dir(void *epdata, int8_t channelno, packet_t *packet);

// debug functions
static void dump_packet(packet_t *p);
static void fat_submit_dump(int8_t channelno, packet_t *txbuf, packet_t *rxbuf);
static void show_root_directory(void);

#endif // FAT_PROVIDER_C
#endif // FAT_PROVIDER_H

