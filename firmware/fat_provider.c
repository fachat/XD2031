/****************************************************************************

    Serial line filesystem server
    Copyright (C) 2012 Andre Fachat
    Copyright (C) 2012 Nils Eilers

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

#include "version.h"
#include "packet.h"
#include "provider.h"
#include "wireformat.h"
#include "fat_provider.h"
#include "petscii.h"
#include "main.h"

#include "debug.h"
#include "led.h"


#define  DEBUG_FAT

static void *prov_assign(const char *name);
static void prov_free(void *epdata);
void fat_submit(void *epdata, packet_t *buf);
void fat_submit_call(void *epdata, int8_t channelno, packet_t *txbuf, packet_t *rxbuf,
                uint8_t (*callback)(int8_t channelno, int8_t errnum));

static int8_t directory_converter(packet_t *p, uint8_t drive);
static int8_t to_provider(packet_t *p);

// dummy
static void *prov_assign(const char *name) {
	return NULL;
}

// dummy
static void prov_free(void *epdata) {
	return;
}

void fat_submit(void *epdata, packet_t *buf) {
}

void fat_submit_call(void *epdata, int8_t channelno, packet_t *txbuf, packet_t *rxbuf,
                uint8_t (*callback)(int8_t channelno, int8_t errnum)) 
{
}

static int8_t directory_converter(packet_t *p, uint8_t drive)
{
  return 0;
}

static int8_t to_provider(packet_t *p)
{
  return 0;
}

provider_t fat_provider  = {
	prov_assign,
	prov_free,
        fat_submit,
        fat_submit_call,
	directory_converter,
	to_provider
};
