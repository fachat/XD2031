/************************************************************************

  XD-2031 - Serial line filesystem server for CBMs
  Copyright (C) 2014 Andre Fachat, Nils Eilers

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

 *************************************************************************/

/*

  Non-volatile memory for runtime configurations
  ==============================================


  These functions depend only on nv_read_byte() and nv_write_byte(),
  which should get defined in <arch>/nvconfighw.c or <device>/
  nvconfighw.c but not in both. A declaration of those functions
  is included in nvconfig.h.

  This code was written with such memories in mind, where a single byte can
  be written at any random address, such as EEPROM or non volatile RAM.  It
  is not suitable for memories that require block-wise access such as flash
  memory.

  NV_MEM_SIZE defines the total size of the NV MEM in bytes. It defaults
  silently for known architectures (only AVR at the moment) or with a
  warning for unknown ones. The whole NV memory is split into several
  blocks of NV_BLK_SIZE bytes in size. Each block holds the runtime
  configuration for a single bus and can update, expand or shrink
  without affecting other blocks.

  Each bus has solely access to its own runtime configuration, so it is
  impossible to store the runtime configurations for all busses at once.
  That is why the XW-command saves only the configuration for the bus that
  was used to issue the command plus the common configurations that are not
  bus dependent.

  The NV data is split into blocks of runtime configs and the selection of
  the appropiate block is made by the bus name stored in rtc->name compared
  against the name stored in each NV rtc block. Common settings use "CMN"
  as name.

  Each NV data block is secured with a CRC16 checksum.


  NV runtime configuration block format
  -------------------------------------

  Offset   Bytes   Content                   Note
  NV_CRC     2     checksum                   [1]
  NV_LINK    2     link
  NV_SIZE    2     size                       [2]
  NV_VER     4     firmware version
  NV_NAME   10     bus name                   [3]
  NV_DATA    n     data                       [4]

  [1] Calculated across <size> bytes starting at NV_SIZE
  [2] Size of all used bytes following the size entry including header data
  [3] ASCII, zero terminated
  [4] Any sequence of bytes, words, double words or strings


  Depends on external functions:
     nv_read_byte()
     nv_write_byte()

  Affected by defines:
     NV_MEM_SIZE
     NV_BLK_SIZE

*/


// Uncomment to enable debug output
//#define DEBUG_NV


#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include "nvconfig.h"
#include "version.h"
#include "rtconfig.h"
#include "config.h"
#include "errors.h"
#include "debug.h"

#ifdef DEBUG_NV
#define nv_debug_puts(s) debug_puts(s)
#define nv_debug_printf(format, ...) term_rom_printf(IN_ROM_STR(format), __VA_ARGS__)
#else
static inline void nv_debug_puts(char *c) {}
static inline void nv_debug_printf(char *format, ...) {}
#endif

#define NV_CRC	0
#define NV_LINK	2
#define NV_SIZE	4
#define NV_VER	6
#define NV_NAME	10
#define MAX_LEN_BUSNAME 10
#define NV_DATA (NV_NAME + MAX_LEN_BUSNAME)

// Name used for common settings that are not bus dependent
// This name must not conflict with any bus name
const char *nv_common_name = "CMN";

// NV_MEM_SIZE = max total size in bytes for all data stored in NV memory
// For AVR, this is E2END which is defined in avr/io.h
#if !(defined NV_MEM_SIZE) && defined(E2END) && defined(__AVR__)
#define NV_MEM_SIZE E2END
#endif

#ifndef NV_MEM_SIZE
#warning "Using default value for NV_MEM_SIZE"
#define NV_MEM_SIZE 256
#endif

// Default: split memory into 3 blocks (2 busses, 1 common)
#ifndef NV_BLK_SIZE
#define NV_BLK_SIZE (NV_MEM_SIZE / 3)
#endif


// CRC16, use with initial value 0xFFFF
#ifdef __AVR__
#include <util/crc16.h>
#else
uint16_t _crc16_update(uint16_t crc, uint8_t a) {
int i;

crc ^= a;
for (i = 0; i < 8; ++i) {
if (crc & 1)
crc = (crc >> 1) ^ 0xA001;
else
crc = (crc >> 1);
}

return crc;
}
#endif


// read little endian 16 bits unsigned value
static uint16_t nv_read_word(uint16_t addr) {
	uint16_t v = nv_read_byte(addr++);
	return (v | (nv_read_byte(addr) << 8));
}


// write little endian 16 bits unsigned value
// return true on errors
static bool nv_write_word(uint16_t addr, uint16_t word) {
	if (nv_write_byte(addr++, (uint8_t) word)) return true;
	return nv_write_byte(addr, (uint8_t) (word >> 8));
}


// read little endian 32 bits unsigned value
static uint32_t nv_read_dword(uint16_t addr) {
	uint32_t v;
	v  = nv_read_byte(addr++);
	v |= (((uint32_t) (nv_read_byte(addr++))) <<  8);
	v |= (((uint32_t) (nv_read_byte(addr++))) << 16);
	v |= (((uint32_t) (nv_read_byte(addr  ))) << 24);
	return v;
}


// write little endian 32 bits unsigned value
// return true on errors
static bool nv_write_dword(uint16_t addr, uint32_t v) {
	bool fail;
	fail  = nv_write_byte(addr++, (uint8_t) (v      ));
	fail |= nv_write_byte(addr++, (uint8_t) (v >>  8));
	fail |= nv_write_byte(addr++, (uint8_t) (v >> 16));
	fail |= nv_write_byte(addr  , (uint8_t) (v >> 24));
	return fail;
}


// write zero terminated string to NV memory
// return successfully written bytes (including zero terminator)
static uint16_t nv_write_str(uint16_t addr, const char *s) {
	uint16_t n = 0;
	uint8_t byte;
	do {
		byte = (uint8_t) *s++;
		if (nv_write_byte(addr++, byte)) break;
		n++;
	} while (byte);
	return n;
}


// read zero terminated string from NV memory
// return number of read bytes (including zero terminator)
static uint16_t nv_read_str(char *dest, uint16_t addr) {
	uint16_t n = 0;
	uint8_t c;
	do {
		c = nv_read_byte(addr++);
		*dest++ = c;
		n++;
	} while (c);
	return n;
}


// calculate crc16 for a memory section
static uint16_t nv_calc_crc(uint16_t addr, uint16_t size) {
	uint16_t i, crc = 0xffff;

	nv_debug_printf("nv_calc_crc(%04X, %04X): ", addr, size);

	for (i=size; i > 0; i--)
		crc = _crc16_update(crc, nv_read_byte(addr++));

	nv_debug_printf("%04X\n", crc);
	return crc;
}


// check if there is a valid runtime configuration at given address
// returns true if a valid rtc block found
static bool nv_is_valid_rtc_block(uint16_t addr) {

	nv_debug_printf("nv_is_valid_rtc_block(%04X)\n", addr);

	// read stored CRC and size
	uint16_t saved_crc  = nv_read_word(addr + NV_CRC);
	uint16_t saved_size = nv_read_word(addr + NV_SIZE);
	nv_debug_printf("saved_crc: %04X, saved_size: %04X\n", saved_crc, saved_size);

	// shortcut: abort for illegal size values
	// (e.g. 0xFFFF in empty EEPROM)
	if (saved_size > NV_MEM_SIZE) {
		nv_debug_puts("ILL NV SIZE\n");
		return false;
	}

	nv_debug_printf("CRC @%04X: ", addr);
	if (saved_crc == nv_calc_crc(addr + NV_SIZE, saved_size)) {
		nv_debug_puts("OK\n");
		return true;
	} else {
		nv_debug_puts("INVALID\n");
		return false;
	}
}


// get address of existing rtc block for config with name rtc->name
// returns 0 if a rtc with that name does not exist
static uint16_t nv_locate(const rtconfig_t *rtc) {
	char name[MAX_LEN_BUSNAME + 1];

	nv_debug_printf("nv_locate(%s)\n", rtc->name);

	// Start at address 1, skip 0 which may corrupt on AVR EEPROMs
	uint16_t addr = 1;

	while (addr) {
		if (nv_is_valid_rtc_block(addr)) {
			// compare rtc names
			nv_read_str(name, addr + NV_NAME);
			nv_debug_printf("'%s' = '%s'?\n", name, rtc->name);
			if (strcmp(name, rtc->name)) {
				// name mismatch, try next block
				addr = nv_read_word(addr + NV_LINK);
				nv_debug_printf("no, cont @%04X\n", addr);
			} else {
				// found!
				nv_debug_puts("found!\n");
				break;
			}
		} else {
			// CRC failed, assume link field as invalid too
			addr = 0;
		}
	}
	nv_debug_printf("nv_locate: %04X\n", addr);
	return addr;
}


// create rtc header, return true on errors
// firmware version is written by nv_save_config() thus not needed here
static bool nv_create_entry(uint16_t addr, uint16_t link, const rtconfig_t *rtc) {
	bool fail = false;
	uint8_t len;

	nv_debug_printf("nv_create_entry(new: %04X, update link @%04X, name='%s')\n", addr, link, rtc->name);

	// update chain link
	fail |= nv_write_word(link + NV_LINK, addr);

	// mark link field as last entry
	fail |= (nv_write_word(addr + NV_LINK, 0));

	// store name
	len = nv_write_str(addr + NV_NAME, rtc->name);
	if (len != (strlen(rtc->name) + 1)) {
		nv_debug_printf("nv_write_str() wrote %d bytes, strlen + 1 = %d\n", len, strlen(rtc->name) + 1);
		fail = true;
	}

	return fail;
}


// get NV address to write a runtime config called rtc->name
// if a block with that name does not exist yet, update chain link to
// this new entry
static uint16_t nv_saveaddr(const rtconfig_t *rtc) {
	uint16_t addr, last_link;

	nv_debug_printf("nv_saveaddr (%s)\n", rtc->name);

	// recycle a stored rtc
	addr = nv_locate(rtc);
	if (addr) return addr;

	// search last valid block to update chain link
	addr = 1;
	last_link = 0;
	while (1) {
		nv_debug_printf("nv_saveaddr: check %04X\n", addr);
		if (nv_is_valid_rtc_block(addr)) {
			last_link = addr;
			addr = nv_read_word(addr + NV_LINK);
			nv_debug_printf("nv_saveaddr: valid block @%04X, link=%04X", last_link, addr);
			if (!addr) {
				// EOF reached, create new entry
				addr = last_link + NV_BLK_SIZE;
				nv_debug_printf("nv_saveaddr: create new entry @%04X\n", addr);
				if (addr > NV_MEM_SIZE) {
					debug_puts("NV MEM FULL\n");
					return 0;
				}
				if (nv_create_entry(addr, last_link, rtc))
					return 0;
				break;
			}
		} else {
			// invalid block found, overwrite it
			if (nv_create_entry(addr, last_link, rtc)) {
				nv_debug_puts("nv_saveaddr: nv_create_entry failed, return 0\n");
				return 0;
			}
			break;
		}
	}
	nv_debug_printf("nv_saveaddr exits with %04X\n", addr);
	return addr;
}

// save runtime configuration to non volatile memory
// returns true on errors
bool nv_save_config(const rtconfig_t *rtc) {
	uint16_t addr;			// Start address of rtc block
	uint16_t p;			// Write pointer
	bool fail = false;
	bool common;

	nv_debug_printf("nv_save_config(%s)\n", rtc->name);

	common = !strcmp(nv_common_name, rtc->name);

	addr = nv_saveaddr(rtc);
	if (!addr) return true;		// abort on errors

	fail |= nv_write_dword(addr + NV_VER, VERSION_U32);

	p = addr + NV_DATA;		// init write pointer

	// save NV data
	if (common) {
		fail |= nv_write_byte(p++, 42); // example code
		// --------------------------------------------------
		//      ---> insert new common data here
		// --------------------------------------------------
	} else {
		fail |= nv_write_byte(p++, rtc->device_address);
		fail |= nv_write_byte(p++, rtc->last_used_drive);
		// --------------------------------------------------
		//      ---> insert new bus dependent data here
		// --------------------------------------------------
	}

	// calculate and store size
	uint16_t nv_size = p - addr - NV_SIZE;
	fail |= nv_write_word(addr + NV_SIZE, nv_size);

	// compute and store CRC
	uint16_t crc = nv_calc_crc(addr + NV_SIZE, nv_size);
	fail |= nv_write_word(addr + NV_CRC, crc);
	nv_debug_printf("nv_size: %04X CRC: %04X\n", nv_size, crc);

	debug_printf("Write %s config to NV MEM: ", rtc->name);
	if (fail)
		debug_puts("FAILED");
	else
		debug_puts("OK");
	debug_putcrlf();
	return (fail ? CBM_ERROR_WRITE_ERROR : CBM_ERROR_OK);
}


// restore runtime config from non volatile memory
// returns true on errors
bool nv_restore_config(rtconfig_t *rtc) {
	uint16_t addr;			// Start address of rtc block
	uint16_t p;			// Read pointer
        bool fail = false;
	bool common;

	nv_debug_printf("nv_restore_config(%s)\n", rtc->name);

	common = !strcmp(nv_common_name, rtc->name);
	nv_debug_printf("common: %d\n", common);

	if(!(addr = nv_locate(rtc))) {
		debug_printf("ERR: NV conf '%s' not found\n", rtc->name);
		return true;
	} else nv_debug_printf("addr(%s)=%04X\n", rtc->name, addr);

	// If NV data is from a prior firmware version,
	// set fail flag to force rewrite in current format
	uint32_t version_in_nv_mem = nv_read_dword(addr + NV_VER);
	nv_debug_printf("VERSION_U32: %08lX\n", VERSION_U32);
	nv_debug_printf("stored: %08lX\n", version_in_nv_mem);
	if (VERSION_U32 > version_in_nv_mem) {
		debug_puts("(outdated) ");
		fail = true;
	}


	p = addr + NV_DATA;		// init read pointer

	// restore NV data
	debug_printf("NV %s: ", rtc->name);
	if (common) {
		uint8_t life_the_universe_and_everything = nv_read_byte(p++);
		debug_printf("*=%d\n", life_the_universe_and_everything);
		// you guessed it, this is an example
		//
		// --------------------------------------------------
		//      ---> insert new common data here
		// --------------------------------------------------
	} else {
		rtc->device_address	= nv_read_byte(p++);
		rtc->last_used_drive	= nv_read_byte(p++);
		// --------------------------------------------------
		//      ---> insert new bus dependent data here
		// --------------------------------------------------
		debug_printf("XU=%d, XD=%d\n", rtc->device_address, rtc->last_used_drive);
	}
	nv_debug_printf("nv_restore_config: fail=%d\n", fail);
	return fail;
}

// save common settings
bool nv_save_common_config (void) {
	rtconfig_t common_rtc;
	common_rtc.name = nv_common_name;
	return nv_save_config(&common_rtc);
}


// restore common settings
bool nv_restore_common_config (void) {
	rtconfig_t common_rtc;
	common_rtc.name = nv_common_name;
	return nv_restore_config(&common_rtc);
}
